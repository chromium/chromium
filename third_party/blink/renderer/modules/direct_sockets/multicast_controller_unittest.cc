// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/direct_sockets/multicast_controller.h"

#include "net/base/net_errors.h"
#include "services/network/public/mojom/restricted_udp_socket.mojom-blink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/direct_sockets/udp_socket_mojo_remote.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {
namespace {

constexpr char kIPv4MulticastAddress[] = "233.252.0.1";
constexpr char kIPv6MulticastAddress[] = "ff02::1234";

class FakeRestrictedUDPSocket final
    : public GarbageCollected<FakeRestrictedUDPSocket>,
      public network::mojom::blink::RestrictedUDPSocket {
 public:
  explicit FakeRestrictedUDPSocket() = default;

  void JoinGroup(const net::IPAddress& address,
                 JoinGroupCallback callback) override {
    join_group_callback_ = std::move(callback);
  }

  void LeaveGroup(const net::IPAddress& address,
                  LeaveGroupCallback callback) override {
    leave_group_callback_ = std::move(callback);
  }

  void Send(base::span<const uint8_t> data, SendCallback callback) override {
    NOTREACHED();
  }

  void SendTo(base::span<const uint8_t> data,
              const net::HostPortPair& dest_addr,
              net::DnsQueryType dns_query_type,
              SendToCallback callback) override {
    NOTREACHED();
  }

  void ReceiveMore(uint32_t num_additional_datagrams) override { NOTREACHED(); }

  void CompleteJoinGroup(net::Error result) {
    test::RunPendingTasks();
    std::move(join_group_callback_).Run(result);
  }

  void CompleteLeaveGroup(net::Error result) {
    test::RunPendingTasks();
    std::move(leave_group_callback_).Run(result);
  }
  void Trace(cppgc::Visitor* visitor) const {}

 private:
  JoinGroupCallback join_group_callback_;
  LeaveGroupCallback leave_group_callback_;
};

class MulticastCreator : public GarbageCollected<MulticastCreator> {
 public:
  explicit MulticastCreator(const V8TestingScope& scope)
      : MulticastCreator(scope,
                         MakeGarbageCollected<FakeRestrictedUDPSocket>()) {}

  MulticastCreator(const V8TestingScope& scope, FakeRestrictedUDPSocket* socket)
      : fake_udp_socket_(socket),
        receiver_{fake_udp_socket_.Get(), scope.GetExecutionContext()} {}

  ~MulticastCreator() = default;

  MulticastController* Create(V8TestingScope& scope) {
    scoped_refptr<base::SingleThreadTaskRunner> task_runner =
        scope.GetExecutionContext()->GetTaskRunner(TaskType::kNetworking);
    auto* udp_socket =
        MakeGarbageCollected<UDPSocketMojoRemote>(scope.GetExecutionContext());

    udp_socket->get().Bind(receiver_.BindNewPipeAndPassRemote(task_runner),
                           task_runner);

    multicast_controller_ = MakeGarbageCollected<MulticastController>(
        scope.GetExecutionContext(), udp_socket, /*inspector_id=*/0);

    return multicast_controller_.Get();
  }

  void Trace(Visitor* visitor) const {
    visitor->Trace(fake_udp_socket_);
    visitor->Trace(receiver_);
    visitor->Trace(multicast_controller_);
  }

  FakeRestrictedUDPSocket* fake_udp_socket() { return fake_udp_socket_.Get(); }

  void Cleanup() { receiver_.reset(); }

 private:
  Member<FakeRestrictedUDPSocket> fake_udp_socket_;
  HeapMojoReceiver<network::mojom::blink::RestrictedUDPSocket,
                   FakeRestrictedUDPSocket>
      receiver_;
  Member<MulticastController> multicast_controller_;
};

class ScopedMulticastCreator {
 public:
  explicit ScopedMulticastCreator(MulticastCreator* multicast_creator)
      : multicast_creator_(multicast_creator) {}

  ~ScopedMulticastCreator() { multicast_creator_->Cleanup(); }

  MulticastCreator* operator->() const { return multicast_creator_; }

 private:
  Persistent<MulticastCreator> multicast_creator_;
};

TEST(MulticastControllerTest, JoinAndLeaveGroup) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScopedMulticastCreator creator(MakeGarbageCollected<MulticastCreator>(scope));
  MulticastController* multicast_controller = creator->Create(scope);

  ScriptState* script_state = scope.GetScriptState();
  NonThrowableExceptionState exception_state;

  // Join a group.
  ScriptPromise<IDLUndefined> join_promise = multicast_controller->joinGroup(
      script_state, kIPv4MulticastAddress, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  ScriptPromiseTester join_tester(script_state, join_promise);

  creator->fake_udp_socket()->CompleteJoinGroup(net::OK);
  join_tester.WaitUntilSettled();
  ASSERT_TRUE(join_tester.IsFulfilled());
  EXPECT_THAT(multicast_controller->joinedGroups(),
              testing::ElementsAre(kIPv4MulticastAddress));

  // Leave the group.
  ScriptPromise leave_promise = multicast_controller->leaveGroup(
      script_state, kIPv4MulticastAddress, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  ScriptPromiseTester leave_tester(script_state, leave_promise);

  creator->fake_udp_socket()->CompleteLeaveGroup(net::OK);
  leave_tester.WaitUntilSettled();
  ASSERT_TRUE(leave_tester.IsFulfilled());
  EXPECT_TRUE(multicast_controller->joinedGroups().empty());
}

TEST(MulticastControllerTest, JoinGroupFailure) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScopedMulticastCreator creator(MakeGarbageCollected<MulticastCreator>(scope));
  MulticastController* multicast_controller = creator->Create(scope);

  ScriptState* script_state = scope.GetScriptState();
  NonThrowableExceptionState exception_state;

  ScriptPromise promise = multicast_controller->joinGroup(
      script_state, kIPv4MulticastAddress, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  ScriptPromiseTester tester(script_state, promise);

  creator->fake_udp_socket()->CompleteJoinGroup(net::ERR_FAILED);
  tester.WaitUntilSettled();
  ASSERT_TRUE(tester.IsRejected());
  EXPECT_TRUE(multicast_controller->joinedGroups().empty());
}

TEST(MulticastControllerTest, LeaveGroupFailure) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScopedMulticastCreator creator(MakeGarbageCollected<MulticastCreator>(scope));
  MulticastController* multicast_controller = creator->Create(scope);

  ScriptState* script_state = scope.GetScriptState();
  NonThrowableExceptionState exception_state;

  // First, join a group successfully.
  ScriptPromise join_promise = multicast_controller->joinGroup(
      script_state, kIPv4MulticastAddress, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  ScriptPromiseTester join_tester(script_state, join_promise);

  creator->fake_udp_socket()->CompleteJoinGroup(net::OK);
  join_tester.WaitUntilSettled();
  ASSERT_TRUE(join_tester.IsFulfilled());

  // Then, fail to leave it.
  ScriptPromise leave_promise = multicast_controller->leaveGroup(
      script_state, kIPv4MulticastAddress, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  ScriptPromiseTester leave_tester(script_state, leave_promise);

  creator->fake_udp_socket()->CompleteLeaveGroup(net::ERR_FAILED);
  leave_tester.WaitUntilSettled();
  ASSERT_TRUE(leave_tester.IsRejected());
  EXPECT_THAT(multicast_controller->joinedGroups(),
              testing::ElementsAre(kIPv4MulticastAddress));
}

TEST(MulticastControllerTest, JoinInvalidAddress) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScopedMulticastCreator creator(MakeGarbageCollected<MulticastCreator>(scope));
  MulticastController* multicast_controller = creator->Create(scope);

  DummyExceptionStateForTesting exception_state;

  multicast_controller->joinGroup(scope.GetScriptState(), "invalid-address",
                                  exception_state);

  EXPECT_TRUE(exception_state.HadException());
}

TEST(MulticastControllerTest, JoinGroupTwice) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScopedMulticastCreator creator(MakeGarbageCollected<MulticastCreator>(scope));
  MulticastController* multicast_controller = creator->Create(scope);

  NonThrowableExceptionState exception_state;

  // Join a group.
  ScriptPromise join_promise = multicast_controller->joinGroup(
      scope.GetScriptState(), kIPv4MulticastAddress, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  ScriptPromiseTester join_tester(scope.GetScriptState(), join_promise);
  creator->fake_udp_socket()->CompleteJoinGroup(net::OK);
  join_tester.WaitUntilSettled();
  ASSERT_TRUE(join_tester.IsFulfilled());

  // Try to join again.
  DummyExceptionStateForTesting exception_state2;
  multicast_controller->joinGroup(scope.GetScriptState(), kIPv4MulticastAddress,
                                  exception_state2);
  EXPECT_TRUE(exception_state2.HadException());
  EXPECT_EQ(exception_state2.CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kInvalidStateError);
}

TEST(MulticastControllerTest, LeaveUnjoinedGroup) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScopedMulticastCreator creator(MakeGarbageCollected<MulticastCreator>(scope));
  MulticastController* multicast_controller = creator->Create(scope);

  DummyExceptionStateForTesting exception_state;
  multicast_controller->leaveGroup(scope.GetScriptState(),
                                   kIPv4MulticastAddress, exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(exception_state.CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kInvalidStateError);
}

TEST(MulticastControllerTest, OnCloseOrAbort) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScopedMulticastCreator creator(MakeGarbageCollected<MulticastCreator>(scope));
  MulticastController* multicast_controller = creator->Create(scope);

  NonThrowableExceptionState exception_state;

  // Join a group.
  ScriptPromise join_promise = multicast_controller->joinGroup(
      scope.GetScriptState(), kIPv4MulticastAddress, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  ScriptPromiseTester join_tester(scope.GetScriptState(), join_promise);
  creator->fake_udp_socket()->CompleteJoinGroup(net::OK);
  join_tester.WaitUntilSettled();
  ASSERT_TRUE(join_tester.IsFulfilled());
  EXPECT_FALSE(multicast_controller->joinedGroups().empty());

  multicast_controller->OnCloseOrAbort();

  EXPECT_TRUE(multicast_controller->joinedGroups().empty());

  // Cannot join after close.
  DummyExceptionStateForTesting exception_state2;
  multicast_controller->joinGroup(scope.GetScriptState(), kIPv6MulticastAddress,
                                  exception_state2);
  EXPECT_TRUE(exception_state2.HadException());
  EXPECT_EQ(exception_state2.CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kInvalidStateError);
}

TEST(MulticastControllerTest, HasPendingActivity) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScopedMulticastCreator creator(MakeGarbageCollected<MulticastCreator>(scope));
  MulticastController* multicast_controller = creator->Create(scope);

  ScriptState* script_state = scope.GetScriptState();
  NonThrowableExceptionState exception_state;

  // Initially, there should be no pending activity.
  EXPECT_FALSE(multicast_controller->HasPendingActivity());

  // When a joinGroup request is made, pending activity should be true.
  ScriptPromise<IDLUndefined> join_promise = multicast_controller->joinGroup(
      script_state, kIPv4MulticastAddress, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  ScriptPromiseTester join_tester(script_state, join_promise);

  EXPECT_TRUE(multicast_controller->HasPendingActivity());

  // Once the promise settles (fulfillment), pending activity should be false.
  creator->fake_udp_socket()->CompleteJoinGroup(net::OK);
  join_tester.WaitUntilSettled();
  ASSERT_TRUE(join_tester.IsFulfilled());
  EXPECT_FALSE(multicast_controller->HasPendingActivity());

  // When a leaveGroup request is made, pending activity should be true.
  ScriptPromise leave_promise = multicast_controller->leaveGroup(
      script_state, kIPv4MulticastAddress, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  ScriptPromiseTester leave_tester(script_state, leave_promise);

  EXPECT_TRUE(multicast_controller->HasPendingActivity());

  // Once the promise settles (rejection), pending activity should be false.
  creator->fake_udp_socket()->CompleteLeaveGroup(net::ERR_FAILED);
  leave_tester.WaitUntilSettled();
  ASSERT_TRUE(leave_tester.IsRejected());
  EXPECT_FALSE(multicast_controller->HasPendingActivity());
}

}  // namespace

}  // namespace blink
