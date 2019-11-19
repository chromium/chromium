// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_data_channel.h"

#include <memory>
#include <string>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/test_simple_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/modules/peerconnection/mock_web_rtc_peer_connection_handler.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
namespace {

void RunSynchronous(base::TestSimpleTaskRunner* thread,
                    CrossThreadOnceClosure closure) {
  if (thread->BelongsToCurrentThread()) {
    std::move(closure).Run();
    return;
  }

  base::WaitableEvent waitable_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  PostCrossThreadTask(
      *thread, FROM_HERE,
      CrossThreadBindOnce(
          [](CrossThreadOnceClosure closure, base::WaitableEvent* event) {
            std::move(closure).Run();
            event->Signal();
          },
          WTF::Passed(std::move(closure)),
          CrossThreadUnretained(&waitable_event)));
  waitable_event.Wait();
}

class MockPeerConnectionHandler : public MockWebRTCPeerConnectionHandler {
 public:
  MockPeerConnectionHandler(
      scoped_refptr<base::TestSimpleTaskRunner> signaling_thread)
      : signaling_thread_(signaling_thread) {}

  void RunSynchronousOnceClosureOnSignalingThread(
      base::OnceClosure closure,
      const char* trace_event_name) override {
    closure_ = std::move(closure);
    RunSynchronous(
        signaling_thread_.get(),
        CrossThreadBindOnce(&MockPeerConnectionHandler::RunOnceClosure,
                            CrossThreadUnretained(this)));
  }

 private:
  void RunOnceClosure() {
    DCHECK(signaling_thread_->BelongsToCurrentThread());
    std::move(closure_).Run();
  }

  scoped_refptr<base::TestSimpleTaskRunner> signaling_thread_;
  base::OnceClosure closure_;

  DISALLOW_COPY_AND_ASSIGN(MockPeerConnectionHandler);
};

class MockDataChannel : public webrtc::DataChannelInterface {
 public:
  explicit MockDataChannel(
      scoped_refptr<base::TestSimpleTaskRunner> signaling_thread)
      : signaling_thread_(signaling_thread),
        buffered_amount_(0),
        observer_(nullptr),
        state_(webrtc::DataChannelInterface::kConnecting) {}

  std::string label() const override { return std::string(); }
  bool reliable() const override { return false; }
  bool ordered() const override { return false; }
  absl::optional<int> maxPacketLifeTime() const override {
    return absl::nullopt;
  }
  absl::optional<int> maxRetransmitsOpt() const override {
    return absl::nullopt;
  }
  std::string protocol() const override { return std::string(); }
  bool negotiated() const override { return false; }
  int id() const override { return 0; }
  uint32_t messages_sent() const override { return 0; }
  uint64_t bytes_sent() const override { return 0; }
  uint32_t messages_received() const override { return 0; }
  uint64_t bytes_received() const override { return 0; }
  void Close() override {}

  void RegisterObserver(webrtc::DataChannelObserver* observer) override {
    RunSynchronous(
        signaling_thread_.get(),
        CrossThreadBindOnce(&MockDataChannel::RegisterObserverOnSignalingThread,
                            CrossThreadUnretained(this),
                            CrossThreadUnretained(observer)));
  }

  void UnregisterObserver() override {
    RunSynchronous(signaling_thread_.get(),
                   CrossThreadBindOnce(
                       &MockDataChannel::UnregisterObserverOnSignalingThread,
                       CrossThreadUnretained(this)));
  }

  uint64_t buffered_amount() const override {
    uint64_t buffered_amount;
    RunSynchronous(signaling_thread_.get(),
                   CrossThreadBindOnce(
                       &MockDataChannel::GetBufferedAmountOnSignalingThread,
                       CrossThreadUnretained(this),
                       CrossThreadUnretained(&buffered_amount)));
    return buffered_amount;
  }

  DataState state() const override {
    DataState state;
    RunSynchronous(
        signaling_thread_.get(),
        CrossThreadBindOnce(&MockDataChannel::GetStateOnSignalingThread,
                            CrossThreadUnretained(this),
                            CrossThreadUnretained(&state)));
    return state;
  }

  bool Send(const webrtc::DataBuffer& buffer) override {
    RunSynchronous(
        signaling_thread_.get(),
        CrossThreadBindOnce(&MockDataChannel::SendOnSignalingThread,
                            CrossThreadUnretained(this), buffer.size()));
    return true;
  }

  // For testing.
  void ChangeState(DataState state) {
    RunSynchronous(
        signaling_thread_.get(),
        CrossThreadBindOnce(&MockDataChannel::ChangeStateOnSignalingThread,
                            CrossThreadUnretained(this), state));
    // The observer posts the state change from the signaling thread to the main
    // thread. Wait for the posted task to be executed.
    base::RunLoop().RunUntilIdle();
  }

 protected:
  ~MockDataChannel() override = default;

 private:
  void RegisterObserverOnSignalingThread(
      webrtc::DataChannelObserver* observer) {
    DCHECK(signaling_thread_->BelongsToCurrentThread());
    observer_ = observer;
  }

  void UnregisterObserverOnSignalingThread() {
    DCHECK(signaling_thread_->BelongsToCurrentThread());
    observer_ = nullptr;
  }

  void GetBufferedAmountOnSignalingThread(uint64_t* buffered_amount) const {
    DCHECK(signaling_thread_->BelongsToCurrentThread());
    *buffered_amount = buffered_amount_;
  }

  void GetStateOnSignalingThread(DataState* state) const {
    DCHECK(signaling_thread_->BelongsToCurrentThread());
    *state = state_;
  }

  void SendOnSignalingThread(uint64_t buffer_size) {
    DCHECK(signaling_thread_->BelongsToCurrentThread());
    buffered_amount_ += buffer_size;
  }

  void ChangeStateOnSignalingThread(DataState state) {
    DCHECK(signaling_thread_->BelongsToCurrentThread());
    state_ = state;
    if (observer_) {
      observer_->OnStateChange();
    }
  }

  scoped_refptr<base::TestSimpleTaskRunner> signaling_thread_;

  // Accessed on signaling thread.
  uint64_t buffered_amount_;
  webrtc::DataChannelObserver* observer_;
  webrtc::DataChannelInterface::DataState state_;

  DISALLOW_COPY_AND_ASSIGN(MockDataChannel);
};

class RTCDataChannelTest : public ::testing::Test {
 public:
  RTCDataChannelTest() : signaling_thread_(new base::TestSimpleTaskRunner()) {}

  scoped_refptr<base::TestSimpleTaskRunner> signaling_thread() {
    return signaling_thread_;
  }

 private:
  scoped_refptr<base::TestSimpleTaskRunner> signaling_thread_;

  DISALLOW_COPY_AND_ASSIGN(RTCDataChannelTest);
};

}  // namespace

TEST_F(RTCDataChannelTest, ChangeStateEarly) {
  scoped_refptr<MockDataChannel> webrtc_channel(
      new rtc::RefCountedObject<MockDataChannel>(signaling_thread()));

  // Change state on the webrtc channel before creating the blink channel.
  webrtc_channel->ChangeState(webrtc::DataChannelInterface::kOpen);

  std::unique_ptr<MockPeerConnectionHandler> pc(
      new MockPeerConnectionHandler(signaling_thread()));
  auto* channel = MakeGarbageCollected<RTCDataChannel>(
      MakeGarbageCollected<NullExecutionContext>(), webrtc_channel.get(),
      pc.get());

  // In RTCDataChannel::Create, the state change update is posted from the
  // signaling thread to the main thread. Wait for posted the task to be
  // executed.
  base::RunLoop().RunUntilIdle();

  // Verify that the early state change was not lost.
  EXPECT_EQ("open", channel->readyState());
}

TEST_F(RTCDataChannelTest, BufferedAmount) {
  scoped_refptr<MockDataChannel> webrtc_channel(
      new rtc::RefCountedObject<MockDataChannel>(signaling_thread()));
  std::unique_ptr<MockPeerConnectionHandler> pc(
      new MockPeerConnectionHandler(signaling_thread()));
  auto* channel = MakeGarbageCollected<RTCDataChannel>(
      MakeGarbageCollected<NullExecutionContext>(), webrtc_channel.get(),
      pc.get());
  webrtc_channel->ChangeState(webrtc::DataChannelInterface::kOpen);

  String message(std::string(100, 'A').c_str());
  channel->send(message, IGNORE_EXCEPTION_FOR_TESTING);
  EXPECT_EQ(100U, channel->bufferedAmount());
}

TEST_F(RTCDataChannelTest, BufferedAmountLow) {
  scoped_refptr<MockDataChannel> webrtc_channel(
      new rtc::RefCountedObject<MockDataChannel>(signaling_thread()));
  std::unique_ptr<MockPeerConnectionHandler> pc(
      new MockPeerConnectionHandler(signaling_thread()));
  auto* channel = MakeGarbageCollected<RTCDataChannel>(
      MakeGarbageCollected<NullExecutionContext>(), webrtc_channel.get(),
      pc.get());
  webrtc_channel->ChangeState(webrtc::DataChannelInterface::kOpen);

  channel->setBufferedAmountLowThreshold(1);
  channel->send("TEST", IGNORE_EXCEPTION_FOR_TESTING);
  EXPECT_EQ(4U, channel->bufferedAmount());
  channel->OnBufferedAmountChange(4);
  ASSERT_EQ(1U, channel->scheduled_events_.size());
  EXPECT_EQ("bufferedamountlow",
            channel->scheduled_events_.back()->type().Utf8());
}

TEST_F(RTCDataChannelTest, Open) {
  scoped_refptr<MockDataChannel> webrtc_channel(
      new rtc::RefCountedObject<MockDataChannel>(signaling_thread()));
  std::unique_ptr<MockPeerConnectionHandler> pc(
      new MockPeerConnectionHandler(signaling_thread()));
  auto* channel = MakeGarbageCollected<RTCDataChannel>(
      MakeGarbageCollected<NullExecutionContext>(), webrtc_channel.get(),
      pc.get());
  channel->OnStateChange(webrtc::DataChannelInterface::kOpen);
  ASSERT_EQ(1U, channel->scheduled_events_.size());
  EXPECT_EQ("open", channel->scheduled_events_.back()->type().Utf8());
}

TEST_F(RTCDataChannelTest, Close) {
  scoped_refptr<MockDataChannel> webrtc_channel(
      new rtc::RefCountedObject<MockDataChannel>(signaling_thread()));
  std::unique_ptr<MockPeerConnectionHandler> pc(
      new MockPeerConnectionHandler(signaling_thread()));
  auto* channel = MakeGarbageCollected<RTCDataChannel>(
      MakeGarbageCollected<NullExecutionContext>(), webrtc_channel.get(),
      pc.get());
  channel->OnStateChange(webrtc::DataChannelInterface::kClosed);
  ASSERT_EQ(1U, channel->scheduled_events_.size());
  EXPECT_EQ("close", channel->scheduled_events_.back()->type().Utf8());
}

TEST_F(RTCDataChannelTest, Message) {
  scoped_refptr<MockDataChannel> webrtc_channel(
      new rtc::RefCountedObject<MockDataChannel>(signaling_thread()));
  std::unique_ptr<MockPeerConnectionHandler> pc(
      new MockPeerConnectionHandler(signaling_thread()));
  auto* channel = MakeGarbageCollected<RTCDataChannel>(
      MakeGarbageCollected<NullExecutionContext>(), webrtc_channel.get(),
      pc.get());

  std::unique_ptr<webrtc::DataBuffer> message(new webrtc::DataBuffer("A"));
  channel->OnMessage(std::move(message));
  ASSERT_EQ(1U, channel->scheduled_events_.size());
  EXPECT_EQ("message", channel->scheduled_events_.back()->type().Utf8());
}

TEST_F(RTCDataChannelTest, SendAfterContextDestroyed) {
  scoped_refptr<MockDataChannel> webrtc_channel(
      new rtc::RefCountedObject<MockDataChannel>(signaling_thread()));
  std::unique_ptr<MockPeerConnectionHandler> pc(
      new MockPeerConnectionHandler(signaling_thread()));
  auto* channel = MakeGarbageCollected<RTCDataChannel>(
      MakeGarbageCollected<NullExecutionContext>(), webrtc_channel.get(),
      pc.get());
  webrtc_channel->ChangeState(webrtc::DataChannelInterface::kOpen);

  channel->ContextDestroyed(nullptr);

  String message(std::string(100, 'A').c_str());
  DummyExceptionStateForTesting exception_state;
  channel->send(message, exception_state);

  EXPECT_TRUE(exception_state.HadException());
}

TEST_F(RTCDataChannelTest, CloseAfterContextDestroyed) {
  scoped_refptr<MockDataChannel> webrtc_channel(
      new rtc::RefCountedObject<MockDataChannel>(signaling_thread()));
  std::unique_ptr<MockPeerConnectionHandler> pc(
      new MockPeerConnectionHandler(signaling_thread()));
  auto* channel = MakeGarbageCollected<RTCDataChannel>(
      MakeGarbageCollected<NullExecutionContext>(), webrtc_channel.get(),
      pc.get());
  webrtc_channel->ChangeState(webrtc::DataChannelInterface::kOpen);

  channel->ContextDestroyed(nullptr);
  channel->close();
  EXPECT_EQ(String::FromUTF8("closed"), channel->readyState());
}

}  // namespace blink
