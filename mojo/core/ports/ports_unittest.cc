// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <map>
#include <sstream>
#include <string_view>
#include <utility>

#include "base/containers/contains.h"
#include "base/containers/heap_array.h"
#include "base/containers/queue.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "mojo/core/ports/event.h"
#include "mojo/core/ports/node.h"
#include "mojo/core/ports/node_delegate.h"
#include "mojo/core/ports/port_locker.h"
#include "mojo/core/ports/user_message.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace core {
namespace ports {
namespace test {

namespace {

// TODO(rockot): Remove this unnecessary alias.
using ScopedMessage = std::unique_ptr<UserMessageEvent>;

class TestMessage : public UserMessage {
 public:
  static const TypeInfo kUserMessageTypeInfo;

  TestMessage(const std::string_view& payload)
      : UserMessage(&kUserMessageTypeInfo), payload_(payload) {}
  ~TestMessage() override = default;

  const std::string& payload() const { return payload_; }

 private:
  std::string payload_;
};

const UserMessage::TypeInfo TestMessage::kUserMessageTypeInfo = {};

ScopedMessage NewUserMessageEvent(const std::string_view& payload,
                                  size_t num_ports) {
  auto event = std::make_unique<UserMessageEvent>(num_ports);
  event->AttachMessage(std::make_unique<TestMessage>(payload));
  return event;
}

bool MessageEquals(const ScopedMessage& message, const std::string_view& s) {
  return message->GetMessage<TestMessage>()->payload() == s;
}

class TestNode;

class MessageRouter {
 public:
  virtual ~MessageRouter() = default;

  virtual void ForwardEvent(TestNode* from_node,
                            const NodeName& node_name,
                            ScopedEvent event) = 0;
  virtual void BroadcastEvent(TestNode* from_node, ScopedEvent event) = 0;
};

class TestNode : public NodeDelegate {
 public:
  explicit TestNode(uint64_t id)
      : node_name_(id, 1),
        node_(node_name_, this),
        node_thread_(base::StringPrintf("Node %" PRIu64 " thread", id)),
        events_available_event_(
            base::WaitableEvent::ResetPolicy::AUTOMATIC,
            base::WaitableEvent::InitialState::NOT_SIGNALED),
        idle_event_(base::WaitableEvent::ResetPolicy::MANUAL,
                    base::WaitableEvent::InitialState::SIGNALED) {}

  ~TestNode() override {
    StopWhenIdle();
    node_thread_.Stop();
  }

  const NodeName& name() const { return node_name_; }

  // NOTE: Node is thread-safe.
  Node& node() { return node_; }

  base::WaitableEvent& idle_event() { return idle_event_; }

  bool IsIdle() {
    base::AutoLock lock(lock_);
    return started_ && !dispatching_ &&
           (incoming_events_.empty() || (block_on_event_ && blocked_));
  }

  void BlockOnEvent(Event::Type type) {
    base::AutoLock lock(lock_);
    blocked_event_type_ = type;
    block_on_event_ = true;
  }

  void Unblock() {
    base::AutoLock lock(lock_);
    block_on_event_ = false;
    events_available_event_.Signal();
  }

  void Start(MessageRouter* router) {
    router_ = router;
    node_thread_.Start();
    node_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&TestNode::ProcessEvents, base::Unretained(this)));
  }

  void StopWhenIdle() {
    base::AutoLock lock(lock_);
    should_quit_ = true;
    events_available_event_.Signal();
  }

  void WakeUp() { events_available_event_.Signal(); }

  int SendStringMessage(const PortRef& port, const std::string& s) {
    return node_.SendUserMessage(port, NewUserMessageEvent(s, 0));
  }

  int SendMultipleMessages(const PortRef& port, size_t num_messages) {
    for (size_t i = 0; i < num_messages; ++i) {
      int result = SendStringMessage(port, "");
      if (result != OK)
        return result;
    }
    return OK;
  }

  int SendStringMessageWithPort(const PortRef& port,
                                const std::string& s,
                                const PortName& sent_port_name) {
    auto event = NewUserMessageEvent(s, 1);
    event->ports()[0] = sent_port_name;
    return node_.SendUserMessage(port, std::move(event));
  }

  int SendStringMessageWithPort(const PortRef& port,
                                const std::string& s,
                                const PortRef& sent_port) {
    return SendStringMessageWithPort(port, s, sent_port.name());
  }

  void set_drop_messages(bool value) {
    base::AutoLock lock(lock_);
    drop_messages_ = value;
  }

  void set_save_messages(bool value) {
    base::AutoLock lock(lock_);
    save_messages_ = value;
  }

  bool ReadMessage(const PortRef& port, ScopedMessage* message) {
    return node_.GetMessage(port, message, nullptr) == OK && *message;
  }

  bool ReadMultipleMessages(const PortRef& port, size_t num_messages) {
    for (size_t i = 0; i < num_messages; ++i) {
      ScopedMessage message;
      if (!ReadMessage(port, &message))
        return false;
    }
    return true;
  }

  bool GetSavedMessage(ScopedMessage* message) {
    base::AutoLock lock(lock_);
    if (saved_messages_.empty()) {
      message->reset();
      return false;
    }
    std::swap(*message, saved_messages_.front());
    saved_messages_.pop();
    return true;
  }

  void EnqueueEvent(const NodeName& from_node, ScopedEvent event) {
    idle_event_.Reset();

    // NOTE: This may be called from ForwardMessage and thus must not reenter
    // |node_|.
    base::AutoLock lock(lock_);
    incoming_events_.push({from_node, std::move(event)});
    events_available_event_.Signal();
  }

  void ForwardEvent(const NodeName& node_name, ScopedEvent event) override {
    {
      base::AutoLock lock(lock_);
      if (drop_messages_) {
        DVLOG(1) << "Dropping ForwardMessage from node " << node_name_ << " to "
                 << node_name;

        base::AutoUnlock unlock(lock_);
        ClosePortsInEvent(event.get());
        return;
      }
    }

    DCHECK(router_);
    DVLOG(1) << "ForwardEvent from node " << node_name_ << " to " << node_name;
    router_->ForwardEvent(this, node_name, std::move(event));
  }

  void BroadcastEvent(ScopedEvent event) override {
    router_->BroadcastEvent(this, std::move(event));
  }

  void PortStatusChanged(const PortRef& port) override {
    // The port may be closed, in which case we ignore the notification.
    base::AutoLock lock(lock_);
    if (!save_messages_)
      return;

    for (;;) {
      ScopedMessage message;
      {
        base::AutoUnlock unlock(lock_);
        if (!ReadMessage(port, &message))
          break;
      }

      saved_messages_.emplace(std::move(message));
    }
  }

  void ClosePortsInEvent(Event* event) {
    if (event->type() != Event::Type::kUserMessage)
      return;

    UserMessageEvent* message_event = static_cast<UserMessageEvent*>(event);
    for (size_t i = 0; i < message_event->num_ports(); ++i) {
      PortRef port;
      ASSERT_EQ(OK, node_.GetPort(message_event->ports()[i], &port));
      EXPECT_EQ(OK, node_.ClosePort(port));
    }
  }

  uint64_t GetUnacknowledgedMessageCount(const PortRef& port_ref) {
    PortStatus status;
    if (node_.GetStatus(port_ref, &status) != OK)
      return 0;

    return status.unacknowledged_message_count;
  }

  void AllowPortMerge(const PortRef& port_ref) {
    SinglePortLocker locker(&port_ref);
    locker.port()->pending_merge_peer = true;
  }

 private:
  void ProcessEvents() {
    for (;;) {
      events_available_event_.Wait();
      base::AutoLock lock(lock_);

      if (should_quit_)
        return;

      dispatching_ = true;
      while (!incoming_events_.empty()) {
        if (block_on_event_ &&
            incoming_events_.front().second->type() == blocked_event_type_) {
          blocked_ = true;
          // Go idle if we hit a blocked event type.
          break;
        } else {
          blocked_ = false;
        }
        auto node_event_pair = std::move(incoming_events_.front());
        incoming_events_.pop();

        // NOTE: AcceptMessage() can re-enter this object to call any of the
        // NodeDelegate interface methods.
        base::AutoUnlock unlock(lock_);
        node_.AcceptEvent(node_event_pair.first,
                          std::move(node_event_pair.second));
      }

      dispatching_ = false;
      started_ = true;
      idle_event_.Signal();
    };
  }

  const NodeName node_name_;
  Node node_;
  raw_ptr<MessageRouter> router_ = nullptr;

  base::Thread node_thread_;
  base::WaitableEvent events_available_event_;
  base::WaitableEvent idle_event_;

  // Guards fields below.
  base::Lock lock_;
  bool started_ = false;
  bool dispatching_ = false;
  bool should_quit_ = false;
  bool drop_messages_ = false;
  bool save_messages_ = false;
  bool blocked_ = false;
  bool block_on_event_ = false;
  Event::Type blocked_event_type_;
  base::queue<std::pair<NodeName, ScopedEvent>> incoming_events_;
  base::queue<ScopedMessage> saved_messages_;
};

class PortsTest : public testing::Test, public MessageRouter {
 public:
  void AddNode(TestNode* node) {
    {
      base::AutoLock lock(lock_);
      nodes_[node->name()] = node;
    }
    node->Start(this);
  }

  void RemoveNode(TestNode* node) {
    {
      base::AutoLock lock(lock_);
      nodes_.erase(node->name());
    }

    for (const auto& entry : nodes_)
      entry.second->node().LostConnectionToNode(node->name());
  }

  // Waits until all known Nodes are idle. Message forwarding and processing
  // is handled in such a way that idleness is a stable state: once all nodes in
  // the system are idle, they will remain idle until the test explicitly
  // initiates some further event (e.g. sending a message, closing a port, or
  // removing a Node).
  void WaitForIdle() {
    for (;;) {
      base::AutoLock global_lock(global_lock_);
      bool all_nodes_idle = true;
      for (const auto& entry : nodes_) {
        if (!entry.second->IsIdle())
          all_nodes_idle = false;
        entry.second->WakeUp();
      }
      if (all_nodes_idle)
        return;

      // Wait for any Node to signal that it's idle.
      base::AutoUnlock global_unlock(global_lock_);
      std::vector<base::WaitableEvent*> events;
      for (const auto& entry : nodes_)
        events.push_back(&entry.second->idle_event());
      base::WaitableEvent::WaitMany(events.data(), events.size());
    }
  }

  void CreatePortPair(TestNode* node0,
                      PortRef* port0,
                      TestNode* node1,
                      PortRef* port1) {
    if (node0 == node1) {
      EXPECT_EQ(OK, node0->node().CreatePortPair(port0, port1));
    } else {
      EXPECT_EQ(OK, node0->node().CreateUninitializedPort(port0));
      EXPECT_EQ(OK, node1->node().CreateUninitializedPort(port1));
      EXPECT_EQ(
          OK, node0->node().InitializePort(*port0, node1->name(), port1->name(),
                                           node1->name(), port1->name()));
      EXPECT_EQ(
          OK, node1->node().InitializePort(*port1, node0->name(), port0->name(),
                                           node0->name(), port0->name()));
    }
  }

 private:
  // MessageRouter:
  void ForwardEvent(TestNode* from_node,
                    const NodeName& node_name,
                    ScopedEvent event) override {
    base::AutoLock global_lock(global_lock_);
    base::AutoLock lock(lock_);
    // Drop messages from nodes that have been removed.
    if (!base::Contains(nodes_, from_node->name())) {
      from_node->ClosePortsInEvent(event.get());
      return;
    }

    auto it = nodes_.find(node_name);
    if (it == nodes_.end()) {
      DVLOG(1) << "Node not found: " << node_name;
      return;
    }

    // Serialize and de-serialize all forwarded events.
    size_t buf_size = event->GetSerializedSize();
    auto buf = base::HeapArray<char>::Uninit(buf_size);
    event->Serialize(buf.data());
    ScopedEvent copy = Event::Deserialize(buf.data(), buf.size());
    // This should always succeed unless serialization or deserialization
    // is broken. In that case, the loss of events should cause a test failure.
    ASSERT_TRUE(copy);

    // Also copy the payload for user messages.
    if (event->type() == Event::Type::kUserMessage) {
      UserMessageEvent* message_event =
          static_cast<UserMessageEvent*>(event.get());
      UserMessageEvent* message_copy =
          static_cast<UserMessageEvent*>(copy.get());

      message_copy->AttachMessage(std::make_unique<TestMessage>(
          message_event->GetMessage<TestMessage>()->payload()));
    }

    it->second->EnqueueEvent(from_node->name(), std::move(event));
  }

  void BroadcastEvent(TestNode* from_node, ScopedEvent event) override {
    base::AutoLock global_lock(global_lock_);
    base::AutoLock lock(lock_);

    // Drop messages from nodes that have been removed.
    if (!base::Contains(nodes_, from_node->name())) {
      return;
    }

    for (const auto& entry : nodes_) {
      TestNode* node = entry.second;
      // Broadcast doesn't deliver to the local node.
      if (node == from_node)
        continue;
      node->EnqueueEvent(from_node->name(), event->CloneForBroadcast());
    }
  }

  base::test::TaskEnvironment task_environment_;

  // Acquired before any operation which makes a Node busy, and before testing
  // if all nodes are idle.
  base::Lock global_lock_;

  base::Lock lock_;
  std::map<NodeName, raw_ptr<TestNode, CtnExperimental>> nodes_;
};

}  // namespace

TEST_F(PortsTest, Basic1) {
  TestNode node0(0);
  AddNode(&node0);

  TestNode node1(1);
  AddNode(&node1);

  PortRef x0, x1;
  CreatePortPair(&node0, &x0, &node1, &x1);

  PortRef a0, a1;
  EXPECT_EQ(OK, node0.node().CreatePortPair(&a0, &a1));
  EXPECT_EQ(OK, node0.SendStringMessageWithPort(x0, "hello", a1));
  EXPECT_EQ(OK, node0.node().ClosePort(a0));

  EXPECT_EQ(OK, node0.node().ClosePort(x0));
  EXPECT_EQ(OK, node1.node().ClosePort(x1));

  WaitForIdle();

  EXPECT_TRUE(node0.node().CanShutdownCleanly());
  EXPECT_TRUE(node1.node().CanShutdownCleanly());
}

TEST_F(PortsTest, Basic2) {
  TestNode node0(0);
  AddNode(&node0);

  TestNode node1(1);
  AddNode(&node1);

  PortRef x0, x1;
  CreatePortPair(&node0, &x0, &node1, &x1);

  PortRef b0, b1;
  EXPECT_EQ(OK, node0.node().CreatePortPair(&b0, &b1));
  EXPECT_EQ(OK, node0.SendStringMessageWithPort(x0, "hello", b1));
  EXPECT_EQ(OK, node0.SendStringMessage(b0, "hello again"));

  EXPECT_EQ(OK, node0.node().ClosePort(b0));

  EXPECT_EQ(OK, node0.node().ClosePort(x0));
  EXPECT_EQ(OK, node1.node().ClosePort(x1));

  WaitForIdle();

  EXPECT_TRUE(node0.node().CanShutdownCleanly());
  EXPECT_TRUE(node1.node().CanShutdownCleanly());
}

TEST_F(PortsTest, Basic3) {
  TestNode node0(0);
  AddNode(&node0);

  TestNode node1(1);
  AddNode(&node1);

  PortRef x0, x1;
  CreatePortPair(&node0, &x0, &node1, &x1);

  PortRef a0, a1;
  EXPECT_EQ(OK, node0.node().CreatePortPair(&a0, &a1));

  EXPECT_EQ(OK, node0.SendStringMessageWithPort(x0, "hello", a1));
  EXPECT_EQ(OK, node0.SendStringMessage(a0, "hello again"));

  EXPECT_EQ(OK, node0.SendStringMessageWithPort(x0, "foo", a0));

  PortRef b0, b1;
  EXPECT_EQ(OK, node0.node().CreatePortPair(&b0, &b1));
  EXPECT_EQ(OK, node0.SendStringMessageWithPort(x0, "bar", b1));
  EXPECT_EQ(OK, node0.SendStringMessage(b0, "baz"));

  EXPECT_EQ(OK, node0.node().ClosePort(b0));

  EXPECT_EQ(OK, node0.node().ClosePort(x0));
  EXPECT_EQ(OK, node1.node().ClosePort(x1));

  WaitForIdle();

  EXPECT_TRUE(node0.node().CanShutdownCleanly());
  EXPECT_TRUE(node1.node().CanShutdownCleanly());
}

TEST_F(PortsTest, LostConnectionToNode1) {
  TestNode node0(0);
  AddNode(&node0);

  TestNode node1(1);
  AddNode(&node1);
  node1.set_drop_messages(true);

  PortRef x0, x1;
  CreatePortPair(&node0, &x0, &node1, &x1);

  // Transfer a port to node1 and simulate a lost connection to node1.

  PortRef a0, a1;
  EXPECT_EQ(OK, node0.node().CreatePortPair(&a0, &a1));
  EXPECT_EQ(OK, node0.SendStringMessageWithPort(x0, "foo", a1));

  WaitForIdle();

  RemoveNode(&node1);

  WaitForIdle();

  EXPECT_EQ(OK, node0.node().ClosePort(a0));
  EXPECT_EQ(OK, node0.node().ClosePort(x0));
  EXPECT_EQ(OK, node1.node().ClosePort(x1));

  WaitForIdle();

  EXPECT_TRUE(node0.node().CanShutdownCleanly());
  EXPECT_TRUE(node1.node().CanShutdownCleanly());
}

TEST_F(PortsTest, LostConnectionToNode2) {
  TestNode node0(0);
  AddNode(&node0);

  TestNode node1(1);
  AddNode(&node1);

  PortRef x0, x1;
  CreatePortPair(&node0, &x0, &node1, &x1);

  PortRef a0, a1;
  EXPECT_EQ(OK, node0.node().CreatePortPair(&a0, &a1));
  EXPECT_EQ(OK, node0.SendStringMessageWithPort(x0, "take a1", a1));

  WaitForIdle();

  node1.set_drop_messages(true);

  RemoveNode(&node1);

  WaitForIdle();

  // a0 should have eventually detected peer closure after node loss.
  ScopedMessage message;
  EXPECT_EQ(ERROR_PORT_PEER_CLOSED,
            node0.node().GetMessage(a0, &message, nullptr));
  EXPECT_FALSE(message);

  EXPECT_EQ(OK, node0.node().ClosePort(a0));

  EXPECT_EQ(OK, node0.node().ClosePort(x0));

  EXPECT_EQ(OK, node1.node().GetMessage(x1, &message, nullptr));
  EXPECT_TRUE(message);
  node1.ClosePortsInEvent(message.get());

  EXPECT_EQ(OK, node1.node().ClosePort(x1));

  WaitForIdle();

  EXPECT_TRUE(node0.node().CanShutdownCleanly());
  EXPECT_TRUE(node1.node().CanShutdownCleanly());
}

TEST_F(PortsTest, LostConnectionToNodeWithSecondaryProxy) {
  // Tests that a proxy gets cleaned up when its indirect peer lives on a lost
  // node.

  TestNode node0(0);
  AddNode(&node0);

  TestNode node1(1);
  AddNode(&node1);

  TestNode node2(2);
  AddNode(&node2);

  // Create A-B spanning nodes 0 and 1 and C-D spanning 1 and 2.
  PortRef A, B, C, D;
  CreatePortPair(&node0, &A, &node1, &B);
  CreatePortPair(&node1, &C, &node2, &D);

  // Create E-F and send F over A to node 1.
  PortRef E, F;
  EXPECT_EQ(OK, node0.node().CreatePortPair(&E, &F));
  EXPECT_EQ(OK, node0.SendStringMessageWithPort(A, ".", F));

  WaitForIdle();

  ScopedMessage message;
  ASSERT_TRUE(node1.ReadMessage(B, &message));
  ASSERT_EQ(1u, message->num_ports());

  EXPECT_EQ(OK, node1.node().GetPort(message->ports()[0], &F));

  // Send F over C to node 2 and then simulate node 2 loss from node 1. Node 1
  // will trivially become aware of the loss, and this test verifies that the
  // port A on node 0 will eventually also become aware of it.

  // Make sure node2 stops processing events when it encounters an ObserveProxy.
  node2.BlockOnEvent(Event::Type::kObserveProxy);

  EXPECT_EQ(OK, node1.SendStringMessageWithPort(C, ".", F));
  WaitForIdle();

  // Simulate node 1 and 2 disconnecting.
  EXPECT_EQ(OK, node1.node().LostConnectionToNode(node2.name()));

  // Let node2 continue processing events and wait for everyone to go idle.
  node2.Unblock();
  WaitForIdle();

  // Port F should be gone.
  EXPECT_EQ(ERROR_PORT_UNKNOWN, node1.node().GetPort(F.name(), &F));

  // Port E should have detected peer closure despite the fact that there is
  // no longer a continuous route from F to E over which the event could travel.
  PortStatus status;
  EXPECT_EQ(OK, node0.node().GetStatus(E, &status));
  EXPECT_TRUE(status.peer_closed);

  EXPECT_EQ(OK, node0.node().ClosePort(A));
  EXPECT_EQ(OK, node1.node().ClosePort(B));
  EXPECT_EQ(OK, node1.node().ClosePort(C));
  EXPECT_EQ(OK, node0.node().ClosePort(E));

  WaitForIdle();

  EXPECT_TRUE(node0.node().CanShutdownCleanly());
  EXPECT_TRUE(node1.node().CanShutdownCleanly());
}

TEST_F(PortsTest, LostConnectionToNodeAfterSendingObserveProxy) {
  // Tests that a proxy gets cleaned up after a node disconnect if the
  // previous port already received the ObserveProxy event.

  TestNode node0(0);
  AddNode(&node0);

  TestNode node1(1);
  AddNode(&node1);

  TestNode node2(2);
  AddNode(&node2);

  // Create A-B spanning nodes 0 and 1 and C-D spanning 1 and 2.
  PortRef A, B, C, D;
  CreatePortPair(&node0, &A, &node1, &B);
  CreatePortPair(&node1, &C, &node2, &D);

  // Create E-F and send F over A to node 1.
  PortRef E, F;
  EXPECT_EQ(OK, node0.node().CreatePortPair(&E, &F));
  EXPECT_EQ(OK, node0.SendStringMessageWithPort(A, ".", F));

  WaitForIdle();

  ScopedMessage message;
  ASSERT_TRUE(node1.ReadMessage(B, &message));
  ASSERT_EQ(1u, message->num_ports());

  EXPECT_EQ(OK, node1.node().GetPort(message->ports()[0], &F));

  // Send F over C to node 2 and then simulate node 2 loss from node 1 after
  // node 0 received the ObserveProxy event. Node 1 needs to clean up the
  // closed proxy while the node 0 to node 2 connection is still intact.
  node0.BlockOnEvent(Event::Type::kObserveProxy);

  EXPECT_EQ(OK, node1.SendStringMessageWithPort(C, ".", F));
  WaitForIdle();

  // Simulate node 1 and 2 disconnecting.
  EXPECT_EQ(OK, node1.node().LostConnectionToNode(node2.name()));

  // Let node2 continue processing events and wait for everyone to go idle.
  node0.Unblock();
  WaitForIdle();

  // Port F should be gone.
  EXPECT_EQ(ERROR_PORT_UNKNOWN, node1.node().GetPort(F.name(), &F));

  EXPECT_EQ(OK, node0.node().ClosePort(A));
  EXPECT_EQ(OK, node1.node().ClosePort(B));
  EXPECT_EQ(OK, node1.node().ClosePort(C));
  EXPECT_EQ(OK, node0.node().ClosePort(E));

  WaitForIdle();

  EXPECT_TRUE(node0.node().CanShutdownCleanly());
  EXPECT_TRUE(node1.node().CanShutdownCleanly());
}

TEST_F(PortsTest, LostConnectionToNodeWithLocalProxy) {
  // Tests that a proxy gets cleaned up when its direct peer lives on a lost
  // node and it's predecessor lives on the same node.

  TestNode node0(0);
  AddNode(&node0);

  TestNode node1(1);
  AddNode(&node1);

  PortRef A, B;
  CreatePortPair(&node0, &A, &node1, &B);

  PortRef C, D;
  EXPECT_EQ(OK, node0.node().CreatePortPair(&C, &D));

  // Send D but block node0 on an ObserveProxy event.
  node0.BlockOnEvent(Event::Type::kObserveProxy);
  EXPECT_EQ(OK, node0.SendStringMessageWithPort(A, ".", D));

  // node0 won't collapse the proxy but node1 will receive the message before
  // going idle.
  WaitForIdle();

  ScopedMessage message;
  ASSERT_TRUE(node1.ReadMessage(B, &message));
  ASSERT_EQ(1u, message->num_ports());
  PortRef E;
  EXPECT_EQ(OK, node1.node().GetPort(message->ports()[0], &E));

  RemoveNode(&node1);

  node0.Unblock();
  WaitForIdle();

  // Port C should have detected peer closure.
  PortStatus status;
  EXPECT_EQ(OK, node0.node().GetStatus(C, &status));
  EXPECT_TRUE(status.peer_closed);

  EXPECT_EQ(OK, node0.node().ClosePort(A));
  EXPECT_EQ(OK, node1.node().ClosePort(B));
  EXPECT_EQ(OK, node0.node().ClosePort(C));
  EXPECT_EQ(OK, node1.node().ClosePort(E));

  EXPECT_TRUE(node0.node().CanShutdownCleanly());
  EXPECT_TRUE(node1.node().CanShutdownCleanly());
}

TEST_F(PortsTest, GetMessage1) {
  TestNode node(0);
  AddNode(&node);

  PortRef a0, a1;
  EXPECT_EQ(OK, node.node().CreatePortPair(&a0, &a1));

  ScopedMessage message;
  EXPECT_EQ(OK, node.node().GetMessage(a0, &message, nullptr));
  EXPECT_FALSE(message);

  EXPECT_EQ(OK, node.node().ClosePort(a1));

  WaitForIdle();

  EXPECT_EQ(ERROR_PORT_PEER_CLOSED,
            node.node().GetMessage(a0, &message, nullptr));
  EXPECT_FALSE(message);

  EXPECT_EQ(OK, node.node().ClosePort(a0));

  WaitForIdle();

  EXPECT_TRUE(node.node().CanShutdownCleanly());
}

TEST_F(PortsTest, GetMessage2) {
  TestNode node(0);
  AddNode(&node);

  PortRef a0, a1;
  EXPECT_EQ(OK, node.node().CreatePortPair(&a0, &a1));

  EXPECT_EQ(OK, node.SendStringMessage(a1, "1"));

  ScopedMessage message;
  EXPECT_EQ(OK, node.node().GetMessage(a0, &message, nullptr));

  ASSERT_TRUE(message);
  EXPECT_TRUE(MessageEquals(message, "1"));

  EXPECT_EQ(OK, node.node().ClosePort(a0));
  EXPECT_EQ(OK, node.node().ClosePort(a1));

  EXPECT_TRUE(node.node().CanShutdownCleanly());
}

TEST_F(PortsTest, GetMessage3) {
  TestNode node(0);
  AddNode(&node);

  PortRef a0, a1;
  EXPECT_EQ(OK, node.node().CreatePortPair(&a0, &a1));

  const char* kStrings[] = {"1", "2", "3"};

  for (size_t i = 0; i < sizeof(kStrings) / sizeof(kStrings[0]); ++i)
    EXPECT_EQ(OK, node.SendStringMessage(a1, kStrings[i]));

  ScopedMessage message;
  for (size_t i = 0; i < sizeof(kStrings) / sizeof(kStrings[0]); ++i) {
    EXPECT_EQ(OK, node.node().GetMessage(a0, &message, nullptr));
    ASSERT_TRUE(message);
    EXPECT_TRUE(MessageEquals(message, kStrings[i]));
  }

  EXPECT_EQ(OK, node.node().ClosePort(a0));
  EXPECT_EQ(OK, node.node().ClosePort(a1));

  EXPECT_TRUE(node.node().CanShutdownCleanly());
}

TEST_F(PortsTest, Delegation1) {
  TestNode node0(0);
  AddNode(&node0);

  TestNode node1(1);
  AddNode(&node1);

  PortRef x0, x1;
  CreatePortPair(&node0, &x0, &node1, &x1);

  // In this test, we send a message to a port that has been moved.

  PortRef a0, a1;
  EXPECT_EQ(OK, node0.node().CreatePortPair(&a0, &a1));
  EXPECT_EQ(OK, node0.SendStringMessageWithPort(x0, "a1", a1));
  WaitForIdle();

  ScopedMessage message;
  ASSERT_TRUE(node1.ReadMessage(x1, &message));
  ASSERT_EQ(1u, message->num_ports());
  EXPECT_TRUE(MessageEquals(message, "a1"));

  // This is "a1" from the point of view of node1.
  PortName a2_name = message->ports()[0];
  EXPECT_EQ(OK, node1.SendStringMessageWithPort(x1, "a2", a2_name));
  EXPECT_EQ(OK, node0.SendStringMessage(a0, "hello"));

  WaitForIdle();

  ASSERT_TRUE(node0.ReadMessage(x0, &message));
  ASSERT_EQ(1u, message->num_ports());
  EXPECT_TRUE(MessageEquals(message, "a2"));

  // This is "a2" from the point of view of node1.
  PortName a3_name = message->ports()[0];

  PortRef a3;
  EXPECT_EQ(OK, node0.node().GetPort(a3_name, &a3));

  ASSERT_TRUE(node0.ReadMessage(a3, &message));
  EXPECT_EQ(0u, message->num_ports());
  EXPECT_TRUE(MessageEquals(message, "hello"));

  EXPECT_EQ(OK, node0.node().ClosePort(a0));
  EXPECT_EQ(OK, node0.node().ClosePort(a3));

  EXPECT_EQ(OK, node0.node().ClosePort(x0));
  EXPECT_EQ(OK, node1.node().ClosePort(x1));

  EXPECT_TRUE(node0.node().CanShutdownCleanly());
  EXPECT_TRUE(node1.node().CanShutdownCleanly());
}

TEST_F(PortsTest, Delegation2) {
  TestNode node0(0);
  AddNode(&node0);

  TestNode node1(1);
  AddNode(&node1);

  for (int i = 0; i < 100; ++i) {
    // Setup pipe a<->b between node0 and node1.
    PortRef A, B;
    CreatePortPair(&node0, &A, &node1, &B);

    PortRef C, D;
    EXPECT_EQ(OK, node0.node().CreatePortPair(&C, &D));

    PortRef E, F;
    EXPECT_EQ(OK, node0.node().CreatePortPair(&E, &F));

    node1.set_save_messages(true);

    // Pass D over A to B.
    EXPECT_EQ(OK, node0.SendStringMessageWithPort(A, "1", D));

    // Pass F over C to D.
    EXPECT_EQ(OK, node0.SendStringMessageWithPort(C, "1", F));

    // This message should find its way to node1.
    EXPECT_EQ(OK, node0.SendStringMessage(E, "hello"));

    WaitForIdle();

    EXPECT_EQ(OK, node0.node().ClosePort(C));
    EXPECT_EQ(OK, node0.node().ClosePort(E));

    EXPECT_EQ(OK, node0.node().ClosePort(A));
    EXPECT_EQ(OK, node1.node().ClosePort(B));

    bool got_hello = false;
    ScopedMessage message;
    while (node1.GetSavedMessage(&message)) {
      node1.ClosePortsInEvent(message.get());
      if (MessageEquals(message, "hello")) {
        got_hello = true;
        break;
      }
    }

    EXPECT_TRUE(got_hello);

    WaitForIdle();  // Because closing ports may have generated tasks.
  }

  EXPECT_TRUE(node0.node().CanShutdownCleanly());
  EXPECT_TRUE(node1.node().CanShutdownCleanly());
}

TEST_F(PortsTest, SendUninitialized) {
  TestNode node(0);
  AddNode(&node);

  PortRef x0;
  EXPECT_EQ(OK, node.node().CreateUninitializedPort(&x0));
  EXPECT_EQ(ERROR_PORT_STATE_UNEXPECTED, node.SendStringMessage(x0, "oops"));
  EXPECT_EQ(OK, node.node().ClosePort(x0));
  EXPECT_TRUE(node.node().CanShutdownCleanly());
}

TEST_F(PortsTest, SendFailure) {
  TestNode node(0);
  AddNode(&node);

  node.set_save_messages(true);

  PortRef A, B;
  EXPECT_EQ(OK, node.node().CreatePortPair(&A, &B));

  // Try to send A over itself.

  EXPECT_EQ(ERROR_PORT_CANNOT_SEND_SELF,
            node.SendStringMessageWithPort(A, "oops", A));

  // Try to send B over A.

  EXPECT_EQ(ERROR_PORT_CANNOT_SEND_PEER,
            node.SendStringMessageWithPort(A, "nope", B));

  // B should be closed immediately.
  EXPECT_EQ(ERROR_PORT_UNKNOWN, node.node().GetPort(B.name(), &B));

  WaitForIdle();

  // There should have been no messages accepted.
  ScopedMessage message;
  EXPECT_FALSE(node.GetSavedMessage(&message));

  EXPECT_EQ(OK, node.node().ClosePort(A));

  WaitForIdle();

  EXPECT_TRUE(node.node().CanShutdownCleanly());
}

TEST_F(PortsTest, DontLeakUnreceivedPorts) {
  TestNode node(0);
  AddNode(&node);

  PortRef A, B, C, D;
  EXPECT_EQ(OK, node.node().CreatePortPair(&A, &B));
  EXPECT_EQ(OK, node.node().CreatePortPair(&C, &D));

  EXPECT_EQ(OK, node.SendStringMessageWithPort(A, "foo", D));

  EXPECT_EQ(OK, node.node().ClosePort(C));
  EXPECT_EQ(OK, node.node().ClosePort(A));
  EXPECT_EQ(OK, node.node().ClosePort(B));

  WaitForIdle();

  EXPECT_TRUE(node.node().CanShutdownCleanly());
}

TEST_F(PortsTest, AllowShutdownWithLocalPortsOpen) {
  TestNode node(0);
  AddNode(&node);

  PortRef A, B, C, D;
  EXPECT_EQ(OK, node.node().CreatePortPair(&A, &B));
  EXPECT_EQ(OK, node.node().CreatePortPair(&C, &D));

  EXPECT_EQ(OK, node.SendStringMessageWithPort(A, "foo", D));

  ScopedMessage message;
  EXPECT_TRUE(node.ReadMessage(B, &message));
  ASSERT_EQ(1u, message->num_ports());
  EXPECT_TRUE(MessageEquals(message, "foo"));
  PortRef E;
  ASSERT_EQ(OK, node.node().GetPort(message->ports()[0], &E));

  EXPECT_TRUE(
      node.node().CanShutdownCleanly(Node::ShutdownPolicy::ALLOW_LOCAL_PORTS));

  WaitForIdle();

  EXPECT_TRUE(
      node.node().CanShutdownCleanly(Node::ShutdownPolicy::ALLOW_LOCAL_PORTS));
  EXPECT_FALSE(node.node().CanShutdownCleanly());

  EXPECT_EQ(OK, node.node().ClosePort(A));
  EXPECT_EQ(OK, node.node().ClosePort(B));
  EXPECT_EQ(OK, node.node().ClosePort(C));
  EXPECT_EQ(OK, node.node().ClosePort(E));

  WaitForIdle();

  EXPECT_TRUE(node.node().CanShutdownCleanly());
}

TEST_F(PortsTest, ProxyCollapse1) {
  TestNode node(0);
  AddNode(&node);

  PortRef A, B;
  EXPECT_EQ(OK, node.node().CreatePortPair(&A, &B));

  PortRef X, Y;
  EXPECT_EQ(OK, node.node().CreatePortPair(&X, &Y));

  ScopedMessage message;

  // Send B and receive it as C.
  EXPECT_EQ(OK, node.SendStringMessageWithPort(X, "foo", B));
  ASSERT_TRUE(node.ReadMessage(Y, &message));
  ASSERT_EQ(1u, message->num_ports());
  PortRef C;
  ASSERT_EQ(OK, node.node().GetPort(message->ports()[0], &C));

  // Send C and receive it as D.
  EXPECT_EQ(OK, node.SendStringMessageWithPort(X, "foo", C));
  ASSERT_TRUE(node.ReadMessage(Y, &message));
  ASSERT_EQ(1u, message->num_ports());
  PortRef D;
  ASSERT_EQ(OK, node.node().GetPort(message->ports()[0], &D));

  // Send D and receive it as E.
  EXPECT_EQ(OK, node.SendStringMessageWithPort(X, "foo", D));
  ASSERT_TRUE(node.ReadMessage(Y, &message));
  ASSERT_EQ(1u, message->num_ports());
  PortRef E;
  ASSERT_EQ(OK, node.node().GetPort(message->ports()[0], &E));

  EXPECT_EQ(OK, node.node().ClosePort(X));
  EXPECT_EQ(OK, node.node().ClosePort(Y));

  EXPECT_EQ(OK, node.node().ClosePort(A));
  EXPECT_EQ(OK, node.node().ClosePort(E));

  // The node should not idle until all proxies are collapsed.
  WaitForIdle();

  EXPECT_TRUE(node.node().CanShutdownCleanly());
}

TEST_F(PortsTest, ProxyCollapse2) {
  TestNode node(0);
  AddNode(&node);

  PortRef A, B;
  EXPECT_EQ(OK, node.node().CreatePortPair(&A, &B));

  PortRef X, Y;
  EXPECT_EQ(OK, node.node().CreatePortPair(&X, &Y));

  ScopedMessage message;

  // Send B and A to create proxies in each direction.
  EXPECT_EQ(OK, node.SendStringMessageWithPort(X, "foo", B));
  EXPECT_EQ(OK, node.SendStringMessageWithPort(X, "foo", A));

  EXPECT_EQ(OK, node.node().ClosePort(X));
  EXPECT_EQ(OK, node.node().ClosePort(Y));

  // At this point we have a scenario with:
  //
  // D -> [B] -> C -> [A]
  //
  // Ensure that the proxies can collapse. The sent ports will be closed
  // eventually as a result of Y's closure.

  WaitForIdle();

  EXPECT_TRUE(node.node().CanShutdownCleanly());
}

TEST_F(PortsTest, SendWithClosedPeer) {
  // This tests that if a port is sent when its peer is already known to be
  // closed, the newly created port will be aware of that peer closure, and the
  // proxy will eventually collapse.

  TestNode node(0);
  AddNode(&node);

  // Send a message from A to B, then close A.
  PortRef A, B;
  EXPECT_EQ(OK, node.node().CreatePortPair(&A, &B));
  EXPECT_EQ(OK, node.SendStringMessage(A, "hey"));
  EXPECT_EQ(OK, node.node().ClosePort(A));

  // Now send B over X-Y as new port C.
  PortRef X, Y;
  EXPECT_EQ(OK, node.node().CreatePortPair(&X, &Y));
  EXPECT_EQ(OK, node.SendStringMessageWithPort(X, "foo", B));
  ScopedMessage message;
  ASSERT_TRUE(node.ReadMessage(Y, &message));
  ASSERT_EQ(1u, message->num_ports());
  PortRef C;
  ASSERT_EQ(OK, node.node().GetPort(message->ports()[0], &C));

  EXPECT_EQ(OK, node.node().ClosePort(X));
  EXPECT_EQ(OK, node.node().ClosePort(Y));

  WaitForIdle();

  // C should have received the message originally sent to B, and it should also
  // be aware of A's closure.

  ASSERT_TRUE(node.ReadMessage(C, &message));
  EXPECT_TRUE(MessageEquals(message, "hey"));

  PortStatus status;
  EXPECT_EQ(OK, node.node().GetStatus(C, &status));
  EXPECT_FALSE(status.receiving_messages);
  EXPECT_FALSE(status.has_messages);
  EXPECT_TRUE(status.peer_closed);

  node.node().ClosePort(C);

  WaitForIdle();

  EXPECT_TRUE(node.node().CanShutdownCleanly());
}

TEST_F(PortsTest, SendWithClosedPeerSent) {
  // This tests that if a port is closed while some number of proxies are still
  // routing messages (directly or indirectly) to it, that the peer port is
  // eventually notified of the closure, and the dead-end proxies will
  // eventually be removed.

  TestNode node(0);
  AddNode(&node);

  PortRef X, Y;
  EXPECT_EQ(OK, node.node().CreatePortPair(&X, &Y));

  PortRef A, B;
  EXPECT_EQ(OK, node.node().CreatePortPair(&A, &B));

  ScopedMessage message;

  // Send A as new port C.
  EXPECT_EQ(OK, node.SendStringMessageWithPort(X, "foo", A));

  ASSERT_TRUE(node.ReadMessage(Y, &message));
  ASSERT_EQ(1u, message->num_ports());
  PortRef C;
  ASSERT_EQ(OK, node.node().GetPort(message->ports()[0], &C));

  // Send C as new port D.
  EXPECT_EQ(OK, node.SendStringMessageWithPort(X, "foo", C));

  ASSERT_TRUE(node.ReadMessage(Y, &message));
  ASSERT_EQ(1u, message->num_ports());
  PortRef D;
  ASSERT_EQ(OK, node.node().GetPort(message->ports()[0], &D));

  // Send a message to B through D, then close D.
  EXPECT_EQ(OK, node.SendStringMessage(D, "hey"));
  EXPECT_EQ(OK, node.node().ClosePort(D));

  // Now send B as new port E.

  EXPECT_EQ(OK, node.SendStringMessageWithPort(X, "foo", B));
  EXPECT_EQ(OK, node.node().ClosePort(X));

  ASSERT_TRUE(node.ReadMessage(Y, &message));
  ASSERT_EQ(1u, message->num_ports());
  PortRef E;
  ASSERT_EQ(OK, node.node().GetPort(message->ports()[0], &E));

  EXPECT_EQ(OK, node.node().ClosePort(Y));

  WaitForIdle();

  // E should receive the message originally sent to B, and it should also be
  // aware of D's closure.

  ASSERT_TRUE(node.ReadMessage(E, &message));
  EXPECT_TRUE(MessageEquals(message, "hey"));

  PortStatus status;
  EXPECT_EQ(OK, node.node().GetStatus(E, &status));
  EXPECT_FALSE(status.receiving_messages);
  EXPECT_FALSE(status.has_messages);
  EXPECT_TRUE(status.peer_closed);

  EXPECT_EQ(OK, node.node().ClosePort(E));

  WaitForIdle();

  EXPECT_TRUE(node.node().CanShutdownCleanly());
}

TEST_F(PortsTest, MergePorts) {
  TestNode node0(0);
  AddNode(&node0);

  TestNode node1(1);
  AddNode(&node1);

  // Setup two independent port pairs, A-B on node0 and C-D on node1.
  PortRef A, B, C, D;
  EXPECT_EQ(OK, node0.node().CreatePortPair(&A, &B));
  EXPECT_EQ(OK, node1.node().CreatePortPair(&C, &D));

  // Write a message on A.
  EXPECT_EQ(OK, node0.SendStringMessage(A, "hey"));

  // Initiate a merge between B and C.
  node1.AllowPortMerge(C);
  EXPECT_EQ(OK, node0.node().MergePorts(B, node1.name(), C.name()));

  WaitForIdle();

  // Expect all proxies to be gone once idle.
  EXPECT_TRUE(
      node0.node().CanShutdownCleanly(Node::ShutdownPolicy::ALLOW_LOCAL_PORTS));
  EXPECT_TRUE(
      node1.node().CanShutdownCleanly(Node::ShutdownPolicy::ALLOW_LOCAL_PORTS));

  // Expect D to have received the message sent on A.
  ScopedMessage message;
  ASSERT_TRUE(node1.ReadMessage(D, &message));
  EXPECT_TRUE(MessageEquals(message, "hey"));

  EXPECT_EQ(OK, node0.node().ClosePort(A));
  EXPECT_EQ(OK, node1.node().ClosePort(D));

  // No more ports should be open.
  EXPECT_TRUE(node0.node().CanShutdownCleanly());
  EXPECT_TRUE(node1.node().CanShutdownCleanly());
}

TEST_F(PortsTest, MergePortWithClosedPeer1) {
  // This tests that the right thing happens when initiating a merge on a port
  // whose peer has already been closed.

  TestNode node0(0);
  AddNode(&node0);

  TestNode node1(1);
  AddNode(&node1);

  // Setup two independent port pairs, A-B on node0 and C-D on node1.
  PortRef A, B, C, D;
  EXPECT_EQ(OK, node0.node().CreatePortPair(&A, &B));
  EXPECT_EQ(OK, node1.node().CreatePortPair(&C, &D));

  // Write a message on A.
  EXPECT_EQ(OK, node0.SendStringMessage(A, "hey"));

  // Close A.
  EXPECT_EQ(OK, node0.node().ClosePort(A));

  // Initiate a merge between B and C.
  node1.AllowPortMerge(C);
  EXPECT_EQ(OK, node0.node().MergePorts(B, node1.name(), C.name()));

  WaitForIdle();

  // Expect all proxies to be gone once idle. node0 should have no ports since
  // A was explicitly closed.
  EXPECT_TRUE(node0.node().CanShutdownCleanly());
  EXPECT_TRUE(
      node1.node().CanShutdownCleanly(Node::ShutdownPolicy::ALLOW_LOCAL_PORTS));

  // Expect D to have received the message sent on A.
  ScopedMessage message;
  ASSERT_TRUE(node1.ReadMessage(D, &message));
  EXPECT_TRUE(MessageEquals(message, "hey"));

  EXPECT_EQ(OK, node1.node().ClosePort(D));

  // No more ports should be open.
  EXPECT_TRUE(node0.node().CanShutdownCleanly());
  EXPECT_TRUE(node1.node().CanShutdownCleanly());
}

TEST_F(PortsTest, MergePortWithClosedPeer2) {
  // This tests that the right thing happens when merging into a port whose peer
  // has already been closed.

  TestNode node0(0);
  AddNode(&node0);

  TestNode node1(1);
  AddNode(&node1);

  // Setup two independent port pairs, A-B on node0 and C-D on node1.
  PortRef A, B, C, D;
  EXPECT_EQ(OK, node0.node().CreatePortPair(&A, &B));
  EXPECT_EQ(OK, node1.node().CreatePortPair(&C, &D));

  // Write a message on D and close it.
  EXPECT_EQ(OK, node1.SendStringMessage(D, "hey"));
  EXPECT_EQ(OK, node1.node().ClosePort(D));

  // Initiate a merge between B and C.
  node1.AllowPortMerge(C);
  EXPECT_EQ(OK, node0.node().MergePorts(B, node1.name(), C.name()));

  WaitForIdle();

  // Expect all proxies to be gone once idle. node1 should have no ports since
  // D was explicitly closed.
  EXPECT_TRUE(
      node0.node().CanShutdownCleanly(Node::ShutdownPolicy::ALLOW_LOCAL_PORTS));
  EXPECT_TRUE(node1.node().CanShutdownCleanly());

  // Expect A to have received the message sent on D.
  ScopedMessage message;
  ASSERT_TRUE(node0.ReadMessage(A, &message));
  EXPECT_TRUE(MessageEquals(message, "hey"));

  EXPECT_EQ(OK, node0.node().ClosePort(A));

  // No more ports should be open.
  EXPECT_TRUE(node0.node().CanShutdownCleanly());
  EXPECT_TRUE(node1.node().CanShutdownCleanly());
}

TEST_F(PortsTest, MergePortsWithClosedPeers) {
  // This tests that no residual ports are left behind if two ports are merged
  // when both of their peers have been closed.

  TestNode node0(0);
  AddNode(&node0);

  TestNode node1(1);
  AddNode(&node1);

  // Setup two independent port pairs, A-B on node0 and C-D on node1.
  PortRef A, B, C, D;
  EXPECT_EQ(OK, node0.node().CreatePortPair(&A, &B));
  EXPECT_EQ(OK, node1.node().CreatePortPair(&C, &D));

  // Close A and D.
  EXPECT_EQ(OK, node0.node().ClosePort(A));
  EXPECT_EQ(OK, node1.node().ClosePort(D));

  WaitForIdle();

  // Initiate a merge between B and C.
  node1.AllowPortMerge(C);
  EXPECT_EQ(OK, node0.node().MergePorts(B, node1.name(), C.name()));

  WaitForIdle();

  // Expect everything to have gone away.
  EXPECT_TRUE(node0.node().CanShutdownCleanly());
  EXPECT_TRUE(node1.node().CanShutdownCleanly());
}

TEST_F(PortsTest, MergePortsWithMovedPeers) {
  // This tests that ports can be merged successfully even if their peers are
  // moved around.

  TestNode node0(0);
  AddNode(&node0);

  TestNode node1(1);
  AddNode(&node1);

  // Setup two independent port pairs, A-B on node0 and C-D on node1.
  PortRef A, B, C, D;
  EXPECT_EQ(OK, node0.node().CreatePortPair(&A, &B));
  EXPECT_EQ(OK, node1.node().CreatePortPair(&C, &D));

  // Set up another pair X-Y for moving ports on node0.
  PortRef X, Y;
  EXPECT_EQ(OK, node0.node().CreatePortPair(&X, &Y));

  ScopedMessage message;

  // Move A to new port E.
  EXPECT_EQ(OK, node0.SendStringMessageWithPort(X, "foo", A));
  ASSERT_TRUE(node0.ReadMessage(Y, &message));
  ASSERT_EQ(1u, message->num_ports());
  PortRef E;
  ASSERT_EQ(OK, node0.node().GetPort(message->ports()[0], &E));

  EXPECT_EQ(OK, node0.node().ClosePort(X));
  EXPECT_EQ(OK, node0.node().ClosePort(Y));

  // Write messages on E and D.
  EXPECT_EQ(OK, node0.SendStringMessage(E, "hey"));
  EXPECT_EQ(OK, node1.SendStringMessage(D, "hi"));

  // Initiate a merge between B and C.
  node1.AllowPortMerge(C);
  EXPECT_EQ(OK, node0.node().MergePorts(B, node1.name(), C.name()));

  WaitForIdle();

  // Expect to receive D's message on E and E's message on D.
  ASSERT_TRUE(node0.ReadMessage(E, &message));
  EXPECT_TRUE(MessageEquals(message, "hi"));
  ASSERT_TRUE(node1.ReadMessage(D, &message));
  EXPECT_TRUE(MessageEquals(message, "hey"));

  // Close E and D.
  EXPECT_EQ(OK, node0.node().ClosePort(E));
  EXPECT_EQ(OK, node1.node().ClosePort(D));

  WaitForIdle();

  // Expect everything to have gone away.
  EXPECT_TRUE(node0.node().CanShutdownCleanly());
  EXPECT_TRUE(node1.node().CanShutdownCleanly());
}

TEST_F(PortsTest, MergePortsFailsGracefully) {
  // This tests that the system remains in a well-defined state if something
  // goes wrong during port merge.

  TestNode node0(0);
  AddNode(&node0);

  TestNode node1(1);
  AddNode(&node1);

  // Setup two independent port pairs, A-B on node0 and C-D on node1.
  PortRef A, B, C, D;
  EXPECT_EQ(OK, node0.node().CreatePortPair(&A, &B));
  EXPECT_EQ(OK, node1.node().CreatePortPair(&C, &D));

  ScopedMessage message;
  PortRef X, Y;
  EXPECT_EQ(OK, node0.node().CreateUninitializedPort(&X));
  EXPECT_EQ(OK, node1.node().CreateUninitializedPort(&Y));
  EXPECT_EQ(OK, node0.node().InitializePort(X, node1.name(), Y.name(),
                                            node1.name(), Y.name()));
  EXPECT_EQ(OK, node1.node().InitializePort(Y, node0.name(), X.name(),
                                            node0.name(), X.name()));

  // Block the merge from proceeding until we can do something stupid with port
  // C. This avoids the test logic racing with async merge logic.
  node1.BlockOnEvent(Event::Type::kMergePort);

  // Initiate the merge between B and C.
  node1.AllowPortMerge(C);
  EXPECT_EQ(OK, node0.node().MergePorts(B, node1.name(), C.name()));

  // Move C to a new port E. This is not a sane use of Node's public API but
  // is still hypothetically possible. It allows us to force a merge failure
  // because C will be in an invalid state by the time the merge is processed.
  // As a result, B should be closed.
  EXPECT_EQ(OK, node1.SendStringMessageWithPort(Y, "foo", C));

  node1.Unblock();

  WaitForIdle();

  ASSERT_TRUE(node0.ReadMessage(X, &message));
  ASSERT_EQ(1u, message->num_ports());
  PortRef E;
  ASSERT_EQ(OK, node0.node().GetPort(message->ports()[0], &E));

  EXPECT_EQ(OK, node0.node().ClosePort(X));
  EXPECT_EQ(OK, node1.node().ClosePort(Y));

  WaitForIdle();

  // C goes away as a result of normal proxy removal. B should have been closed
  // cleanly by the failed MergePorts.
  EXPECT_EQ(ERROR_PORT_UNKNOWN, node1.node().GetPort(C.name(), &C));
  EXPECT_EQ(ERROR_PORT_UNKNOWN, node0.node().GetPort(B.name(), &B));

  // Close A, D, and E.
  EXPECT_EQ(OK, node0.node().ClosePort(A));
  EXPECT_EQ(OK, node1.node().ClosePort(D));
  EXPECT_EQ(OK, node0.node().ClosePort(E));

  WaitForIdle();

  // Expect everything to have gone away.
  EXPECT_TRUE(node0.node().CanShutdownCleanly());
  EXPECT_TRUE(node1.node().CanShutdownCleanly());
}

TEST_F(PortsTest, RemotePeerStatus) {
  TestNode node0(0);
  AddNode(&node0);

  TestNode node1(1);
  AddNode(&node1);

  // Create a local port pair. Neither port should appear to have a remote peer.
  PortRef a, b;
  PortStatus status;
  node0.node().CreatePortPair(&a, &b);
  ASSERT_EQ(OK, node0.node().GetStatus(a, &status));
  EXPECT_FALSE(status.peer_remote);
  ASSERT_EQ(OK, node0.node().GetStatus(b, &status));
  EXPECT_FALSE(status.peer_remote);

  // Create a port pair spanning the two nodes. Both spanning ports should
  // immediately appear to have a remote peer.
  PortRef x0, x1;
  CreatePortPair(&node0, &x0, &node1, &x1);

  ASSERT_EQ(OK, node0.node().GetStatus(x0, &status));
  EXPECT_TRUE(status.peer_remote);
  ASSERT_EQ(OK, node1.node().GetStatus(x1, &status));
  EXPECT_TRUE(status.peer_remote);

  PortRef x2, x3;
  CreatePortPair(&node0, &x2, &node1, &x3);

  // Transfer |b| to |node1| and |x1| to |node0|. i.e., make the local peers
  // remote and the remote peers local.
  EXPECT_EQ(OK, node0.SendStringMessageWithPort(x2, "foo", b));
  EXPECT_EQ(OK, node1.SendStringMessageWithPort(x3, "bar", x1));
  WaitForIdle();

  ScopedMessage message;
  ASSERT_TRUE(node0.ReadMessage(x2, &message));
  ASSERT_EQ(1u, message->num_ports());
  ASSERT_EQ(OK, node0.node().GetPort(message->ports()[0], &x1));

  ASSERT_TRUE(node1.ReadMessage(x3, &message));
  ASSERT_EQ(1u, message->num_ports());
  ASSERT_EQ(OK, node1.node().GetPort(message->ports()[0], &b));

  // Now x0-x1 should be local to node0 and a-b should span the nodes.
  ASSERT_EQ(OK, node0.node().GetStatus(x0, &status));
  EXPECT_FALSE(status.peer_remote);
  ASSERT_EQ(OK, node0.node().GetStatus(x1, &status));
  EXPECT_FALSE(status.peer_remote);
  ASSERT_EQ(OK, node0.node().GetStatus(a, &status));
  EXPECT_TRUE(status.peer_remote);
  ASSERT_EQ(OK, node1.node().GetStatus(b, &status));
  EXPECT_TRUE(status.peer_remote);

  // And swap them back one more time.
  EXPECT_EQ(OK, node0.SendStringMessageWithPort(x2, "foo", x1));
  EXPECT_EQ(OK, node1.SendStringMessageWithPort(x3, "bar", b));
  WaitForIdle();

  ASSERT_TRUE(node0.ReadMessage(x2, &message));
  ASSERT_EQ(1u, message->num_ports());
  ASSERT_EQ(OK, node0.node().GetPort(message->ports()[0], &b));

  ASSERT_TRUE(node1.ReadMessage(x3, &message));
  ASSERT_EQ(1u, message->num_ports());
  ASSERT_EQ(OK, node1.node().GetPort(message->ports()[0], &x1));

  ASSERT_EQ(OK, node0.node().GetStatus(x0, &status));
  EXPECT_TRUE(status.peer_remote);
  ASSERT_EQ(OK, node1.node().GetStatus(x1, &status));
  EXPECT_TRUE(status.peer_remote);
  ASSERT_EQ(OK, node0.node().GetStatus(a, &status));
  EXPECT_FALSE(status.peer_remote);
  ASSERT_EQ(OK, node0.node().GetStatus(b, &status));
  EXPECT_FALSE(status.peer_remote);

  EXPECT_EQ(OK, node0.node().ClosePort(x0));
  EXPECT_EQ(OK, node1.node().ClosePort(x1));
  EXPECT_EQ(OK, node0.node().ClosePort(x2));
  EXPECT_EQ(OK, node1.node().ClosePort(x3));
  EXPECT_EQ(OK, node0.node().ClosePort(a));
  EXPECT_EQ(OK, node0.node().ClosePort(b));

  EXPECT_TRUE(node0.node().CanShutdownCleanly());
  EXPECT_TRUE(node1.node().CanShutdownCleanly());
}

TEST_F(PortsTest, RemotePeerStatusAfterLocalPortMerge) {
  TestNode node0(0);
  AddNode(&node0);

  TestNode node1(1);
  AddNode(&node1);

  // Set up a-b on node0 and c-d spanning node0-node1.
  PortRef a, b, c, d;
  node0.node().CreatePortPair(&a, &b);
  CreatePortPair(&node0, &c, &node1, &d);

  PortStatus status;
  ASSERT_EQ(OK, node0.node().GetStatus(a, &status));
  EXPECT_FALSE(status.peer_remote);
  ASSERT_EQ(OK, node0.node().GetStatus(b, &status));
  EXPECT_FALSE(status.peer_remote);
  ASSERT_EQ(OK, node0.node().GetStatus(c, &status));
  EXPECT_TRUE(status.peer_remote);
  ASSERT_EQ(OK, node1.node().GetStatus(d, &status));
  EXPECT_TRUE(status.peer_remote);

  EXPECT_EQ(OK, node0.node().MergeLocalPorts(b, c));
  WaitForIdle();

  ASSERT_EQ(OK, node0.node().GetStatus(a, &status));
  EXPECT_TRUE(status.peer_remote);
  ASSERT_EQ(OK, node1.node().GetStatus(d, &status));
  EXPECT_TRUE(status.peer_remote);

  EXPECT_EQ(OK, node0.node().ClosePort(a));
  EXPECT_EQ(OK, node1.node().ClosePort(d));
  EXPECT_TRUE(node0.node().CanShutdownCleanly());
  EXPECT_TRUE(node1.node().CanShutdownCleanly());
}

TEST_F(PortsTest, RemotePeerStatusAfterRemotePortMerge) {
  TestNode node0(0);
  AddNode(&node0);

  TestNode node1(1);
  AddNode(&node1);

  // Set up a-b on node0 and c-d on node1.
  PortRef a, b, c, d;
  node0.node().CreatePortPair(&a, &b);
  node1.node().CreatePortPair(&c, &d);

  PortStatus status;
  ASSERT_EQ(OK, node0.node().GetStatus(a, &status));
  EXPECT_FALSE(status.peer_remote);
  ASSERT_EQ(OK, node0.node().GetStatus(b, &status));
  EXPECT_FALSE(status.peer_remote);
  ASSERT_EQ(OK, node1.node().GetStatus(c, &status));
  EXPECT_FALSE(status.peer_remote);
  ASSERT_EQ(OK, node1.node().GetStatus(d, &status));
  EXPECT_FALSE(status.peer_remote);

  node1.AllowPortMerge(c);
  EXPECT_EQ(OK, node0.node().MergePorts(b, node1.name(), c.name()));
  WaitForIdle();

  ASSERT_EQ(OK, node0.node().GetStatus(a, &status));
  EXPECT_TRUE(status.peer_remote);
  ASSERT_EQ(OK, node1.node().GetStatus(d, &status));
  EXPECT_TRUE(status.peer_remote);

  EXPECT_EQ(OK, node0.node().ClosePort(a));
  EXPECT_EQ(OK, node1.node().ClosePort(d));
  EXPECT_TRUE(node0.node().CanShutdownCleanly());
  EXPECT_TRUE(node1.node().CanShutdownCleanly());
}

TEST_F(PortsTest, RetransmitUserMessageEvents) {
  // Ensures that user message events can be retransmitted properly.
  TestNode node0(0);
  AddNode(&node0);

  PortRef a, b;
  node0.node().CreatePortPair(&a, &b);

  // Ping.
  const char* kMessage = "hey";
  ScopedMessage message;
  EXPECT_EQ(OK, node0.SendStringMessage(a, kMessage));
  ASSERT_TRUE(node0.ReadMessage(b, &message));
  EXPECT_TRUE(MessageEquals(message, kMessage));

  // Pong.
  EXPECT_EQ(OK, node0.node().SendUserMessage(b, std::move(message)));
  EXPECT_FALSE(message);
  ASSERT_TRUE(node0.ReadMessage(a, &message));
  EXPECT_TRUE(MessageEquals(message, kMessage));

  // Ping again.
  EXPECT_EQ(OK, node0.node().SendUserMessage(a, std::move(message)));
  EXPECT_FALSE(message);
  ASSERT_TRUE(node0.ReadMessage(b, &message));
  EXPECT_TRUE(MessageEquals(message, kMessage));

  // Pong again!
  EXPECT_EQ(OK, node0.node().SendUserMessage(b, std::move(message)));
  EXPECT_FALSE(message);
  ASSERT_TRUE(node0.ReadMessage(a, &message));
  EXPECT_TRUE(MessageEquals(message, kMessage));

  EXPECT_EQ(OK, node0.node().ClosePort(a));
  EXPECT_EQ(OK, node0.node().ClosePort(b));
}

TEST_F(PortsTest, SetAcknowledgeRequestInterval) {
  TestNode node0(0);
  AddNode(&node0);

  PortRef a0, a1;
  EXPECT_EQ(OK, node0.node().CreatePortPair(&a0, &a1));
  EXPECT_EQ(0u, node0.GetUnacknowledgedMessageCount(a0));

  // Send a batch of messages.
  EXPECT_EQ(OK, node0.SendMultipleMessages(a0, 15));
  EXPECT_EQ(15u, node0.GetUnacknowledgedMessageCount(a0));
  EXPECT_TRUE(node0.ReadMultipleMessages(a1, 5));
  WaitForIdle();
  EXPECT_EQ(15u, node0.GetUnacknowledgedMessageCount(a0));

  // Set to acknowledge every read message, and validate that already-read
  // messages are acknowledged.
  EXPECT_EQ(OK, node0.node().SetAcknowledgeRequestInterval(a0, 1));
  WaitForIdle();
  EXPECT_EQ(10u, node0.GetUnacknowledgedMessageCount(a0));

  // Read a third of the messages from the other end.
  EXPECT_TRUE(node0.ReadMultipleMessages(a1, 5));
  WaitForIdle();

  EXPECT_EQ(5u, node0.GetUnacknowledgedMessageCount(a0));

  TestNode node1(1);
  AddNode(&node1);

  // Transfer a1 across to node1.
  PortRef x0, x1;
  CreatePortPair(&node0, &x0, &node1, &x1);
  EXPECT_EQ(OK, node0.SendStringMessageWithPort(x0, "foo", a1));
  WaitForIdle();

  ScopedMessage message;
  ASSERT_TRUE(node1.ReadMessage(x1, &message));
  ASSERT_EQ(1u, message->num_ports());
  ASSERT_EQ(OK, node1.node().GetPort(message->ports()[0], &a1));

  // Read the last third of the messages from the transferred node, and
  // validate that the unacknowledge message count updates correctly.
  EXPECT_TRUE(node1.ReadMultipleMessages(a1, 5));
  WaitForIdle();
  EXPECT_EQ(0u, node0.GetUnacknowledgedMessageCount(a0));

  // Turn the acknowledges down and validate that they don't go on indefinitely.
  EXPECT_EQ(OK, node0.node().SetAcknowledgeRequestInterval(a0, 0));
  EXPECT_EQ(OK, node0.SendMultipleMessages(a0, 10));
  WaitForIdle();
  EXPECT_TRUE(node1.ReadMultipleMessages(a1, 10));
  WaitForIdle();
  EXPECT_NE(0u, node0.GetUnacknowledgedMessageCount(a0));

  // Close the far port and validate that the closure updates the unacknowledged
  // count.
  EXPECT_EQ(OK, node1.node().ClosePort(a1));
  WaitForIdle();
  EXPECT_EQ(0u, node0.GetUnacknowledgedMessageCount(a0));

  EXPECT_EQ(OK, node0.node().ClosePort(a0));
  EXPECT_EQ(OK, node0.node().ClosePort(x0));
  EXPECT_EQ(OK, node1.node().ClosePort(x1));
}

}  // namespace test
}  // namespace ports
}  // namespace core
}  // namespace mojo
