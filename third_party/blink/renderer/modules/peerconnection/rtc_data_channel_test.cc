// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_data_channel.h"

#include <memory>
#include <string>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_simple_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/modules/peerconnection/mock_rtc_peer_connection_handler_platform.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/page_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
namespace {

using testing::_;

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
          std::move(closure), CrossThreadUnretained(&waitable_event)));
  waitable_event.Wait();
}

class MockEventListener final : public NativeEventListener {
 public:
  MOCK_METHOD(void, Invoke, (ExecutionContext * executionContext, Event*));
};

class MockPeerConnectionHandler : public MockRTCPeerConnectionHandlerPlatform {
 public:
  MockPeerConnectionHandler(
      scoped_refptr<base::TestSimpleTaskRunner> signaling_thread)
      : signaling_thread_(signaling_thread) {}

  MockPeerConnectionHandler(const MockPeerConnectionHandler&) = delete;
  MockPeerConnectionHandler& operator=(const MockPeerConnectionHandler&) =
      delete;

  scoped_refptr<base::SingleThreadTaskRunner> signaling_thread()
      const override {
    return signaling_thread_;
  }

 private:
  void RunOnceClosure() {
    DCHECK(signaling_thread_->BelongsToCurrentThread());
    std::move(closure_).Run();
  }

  scoped_refptr<base::TestSimpleTaskRunner> signaling_thread_;
  CrossThreadOnceClosure closure_;
};

class MockDataChannel : public webrtc::DataChannelInterface {
 public:
  explicit MockDataChannel(
      scoped_refptr<base::TestSimpleTaskRunner> signaling_thread)
      : signaling_thread_(signaling_thread),
        buffered_amount_(0),
        observer_(nullptr),
        state_(webrtc::DataChannelInterface::kConnecting) {}

  MockDataChannel(const MockDataChannel&) = delete;
  MockDataChannel& operator=(const MockDataChannel&) = delete;

  std::string label() const override { return std::string(); }
  bool reliable() const override { return false; }
  bool ordered() const override { return false; }
  std::optional<int> maxPacketLifeTime() const override { return std::nullopt; }
  std::optional<int> maxRetransmitsOpt() const override { return std::nullopt; }
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

  void SendAsync(
      webrtc::DataBuffer buffer,
      absl::AnyInvocable<void(webrtc::RTCError) &&> on_complete) override {
    base::WaitableEvent waitable_event(
        base::WaitableEvent::ResetPolicy::MANUAL,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    auto* adapter = new absl::AnyInvocable<void(webrtc::RTCError) &&>(
        std::move(on_complete));

    PostCrossThreadTask(
        *signaling_thread_.get(), FROM_HERE,
        CrossThreadBindOnce(
            [](MockDataChannel* channel, uint64_t buffer_size,
               absl::AnyInvocable<void(webrtc::RTCError) &&>* adapter) {
              channel->SendOnSignalingThread(buffer_size);
              if (*adapter) {
                std::move (*adapter)(webrtc::RTCError::OK());
              }
              delete adapter;
            },
            CrossThreadUnretained(this), buffer.size(),
            CrossThreadUnretained(adapter)));
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
  raw_ptr<webrtc::DataChannelObserver> observer_;
  webrtc::DataChannelInterface::DataState state_;
};

class RTCDataChannelTest : public ::testing::Test {
 public:
  RTCDataChannelTest() : signaling_thread_(new base::TestSimpleTaskRunner()) {}

  RTCDataChannelTest(const RTCDataChannelTest&) = delete;
  RTCDataChannelTest& operator=(const RTCDataChannelTest&) = delete;

  ~RTCDataChannelTest() override {
    execution_context_->NotifyContextDestroyed();
  }

  scoped_refptr<base::TestSimpleTaskRunner> signaling_thread() {
    return signaling_thread_;
  }

  void VerifyNoTransfersAfterSend(
      base::OnceCallback<void(RTCDataChannel*)> send_data_callback) {
    V8TestingScope scope;
    ScopedTransferableRTCDataChannelForTest scoped_feature(/*enabled=*/true);

    rtc::scoped_refptr<MockDataChannel> webrtc_channel(
        new rtc::RefCountedObject<MockDataChannel>(signaling_thread()));
    auto* channel = MakeGarbageCollected<RTCDataChannel>(
        scope.GetExecutionContext(), webrtc_channel);

    EXPECT_TRUE(channel->IsTransferable());

    // Perform a `send()` operation. We do not care that `channel` is in the
    // "opening" state and that the `send()` operation will throw.
    std::move(send_data_callback).Run(channel);

    // The channel should no longer be transferable after `send()` has been
    // called.
    EXPECT_FALSE(channel->IsTransferable());
  }

 protected:
  test::TaskEnvironment task_environment_;
  Persistent<NullExecutionContext> execution_context_ =
      MakeGarbageCollected<NullExecutionContext>();

 private:
  scoped_refptr<base::TestSimpleTaskRunner> signaling_thread_;
};

}  // namespace

TEST_F(RTCDataChannelTest, ChangeStateEarly) {
  rtc::scoped_refptr<MockDataChannel> webrtc_channel(
      new rtc::RefCountedObject<MockDataChannel>(signaling_thread()));

  // Change state on the webrtc channel before creating the blink channel.
  webrtc_channel->ChangeState(webrtc::DataChannelInterface::kOpen);

  auto* channel =
      MakeGarbageCollected<RTCDataChannel>(execution_context_, webrtc_channel);

  // In RTCDataChannel::Create, the state change update is posted from the
  // signaling thread to the main thread. Wait for posted the task to be
  // executed.
  base::RunLoop().RunUntilIdle();

  // Verify that the early state change was not lost.
  EXPECT_EQ("open", channel->readyState());
}

TEST_F(RTCDataChannelTest, BufferedAmount) {
  rtc::scoped_refptr<MockDataChannel> webrtc_channel(
      new rtc::RefCountedObject<MockDataChannel>(signaling_thread()));
  auto* channel =
      MakeGarbageCollected<RTCDataChannel>(execution_context_, webrtc_channel);
  webrtc_channel->ChangeState(webrtc::DataChannelInterface::kOpen);

  String message(std::string(100, 'A').c_str());
  channel->send(message, IGNORE_EXCEPTION_FOR_TESTING);
  EXPECT_EQ(100U, channel->bufferedAmount());
  // The actual send operation is posted to the signaling thread; wait for it
  // to run to avoid a memory leak.
  signaling_thread()->RunUntilIdle();
}

TEST_F(RTCDataChannelTest, BufferedAmountLow) {
  rtc::scoped_refptr<MockDataChannel> webrtc_channel(
      new rtc::RefCountedObject<MockDataChannel>(signaling_thread()));
  auto* onbufferedamountlow_handler = MakeGarbageCollected<MockEventListener>();
  auto* channel =
      MakeGarbageCollected<RTCDataChannel>(execution_context_, webrtc_channel);
  channel->addEventListener(event_type_names::kBufferedamountlow,
                            onbufferedamountlow_handler);
  EXPECT_CALL(*onbufferedamountlow_handler, Invoke(_, _));
  webrtc_channel->ChangeState(webrtc::DataChannelInterface::kOpen);

  channel->setBufferedAmountLowThreshold(1);
  channel->send("TEST", IGNORE_EXCEPTION_FOR_TESTING);
  EXPECT_EQ(4U, channel->bufferedAmount());
  channel->OnBufferedAmountChange(4);

  // The actual send operation is posted to the signaling thread; wait for it
  // to run to avoid a memory leak.
  signaling_thread()->RunUntilIdle();
}

TEST_F(RTCDataChannelTest, Open) {
  rtc::scoped_refptr<MockDataChannel> webrtc_channel(
      new rtc::RefCountedObject<MockDataChannel>(signaling_thread()));
  auto* channel =
      MakeGarbageCollected<RTCDataChannel>(execution_context_, webrtc_channel);
  channel->OnStateChange(webrtc::DataChannelInterface::kOpen);
  EXPECT_EQ("open", channel->readyState());
}

TEST_F(RTCDataChannelTest, Close) {
  rtc::scoped_refptr<MockDataChannel> webrtc_channel(
      new rtc::RefCountedObject<MockDataChannel>(signaling_thread()));
  auto* channel =
      MakeGarbageCollected<RTCDataChannel>(execution_context_, webrtc_channel);
  channel->OnStateChange(webrtc::DataChannelInterface::kClosed);
  EXPECT_EQ("closed", channel->readyState());
}

TEST_F(RTCDataChannelTest, Message) {
  rtc::scoped_refptr<MockDataChannel> webrtc_channel(
      new rtc::RefCountedObject<MockDataChannel>(signaling_thread()));
  auto* onmessage_handler = MakeGarbageCollected<MockEventListener>();
  auto* channel =
      MakeGarbageCollected<RTCDataChannel>(execution_context_, webrtc_channel);
  channel->addEventListener(event_type_names::kMessage, onmessage_handler);
  EXPECT_CALL(*onmessage_handler, Invoke(_, _));

  channel->OnMessage(webrtc::DataBuffer("A"));
}

TEST_F(RTCDataChannelTest, SendAfterContextDestroyed) {
  rtc::scoped_refptr<MockDataChannel> webrtc_channel(
      new rtc::RefCountedObject<MockDataChannel>(signaling_thread()));
  auto* channel =
      MakeGarbageCollected<RTCDataChannel>(execution_context_, webrtc_channel);
  webrtc_channel->ChangeState(webrtc::DataChannelInterface::kOpen);

  channel->ContextDestroyed();

  String message(std::string(100, 'A').c_str());
  DummyExceptionStateForTesting exception_state;
  channel->send(message, exception_state);

  EXPECT_TRUE(exception_state.HadException());
}

TEST_F(RTCDataChannelTest, CloseAfterContextDestroyed) {
  rtc::scoped_refptr<MockDataChannel> webrtc_channel(
      new rtc::RefCountedObject<MockDataChannel>(signaling_thread()));
  auto* channel =
      MakeGarbageCollected<RTCDataChannel>(execution_context_, webrtc_channel);
  webrtc_channel->ChangeState(webrtc::DataChannelInterface::kOpen);

  channel->ContextDestroyed();
  channel->close();
  EXPECT_EQ(String::FromUTF8("closed"), channel->readyState());
}

TEST_F(RTCDataChannelTest, StopsThrottling) {
  V8TestingScope scope;

  auto* scheduler = scope.GetFrame().GetFrameScheduler()->GetPageScheduler();
  EXPECT_FALSE(scheduler->OptedOutFromAggressiveThrottlingForTest());

  // Creating an RTCDataChannel doesn't enable the opt-out.
  rtc::scoped_refptr<MockDataChannel> webrtc_channel(
      new rtc::RefCountedObject<MockDataChannel>(signaling_thread()));
  auto* channel = MakeGarbageCollected<RTCDataChannel>(
      scope.GetExecutionContext(), webrtc_channel);
  EXPECT_EQ("connecting", channel->readyState());
  EXPECT_FALSE(scheduler->OptedOutFromAggressiveThrottlingForTest());

  // Transitioning to 'open' enables the opt-out.
  webrtc_channel->ChangeState(webrtc::DataChannelInterface::kOpen);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("open", channel->readyState());
  EXPECT_TRUE(scheduler->OptedOutFromAggressiveThrottlingForTest());

  // Transitioning to 'closing' keeps the opt-out enabled.
  webrtc_channel->ChangeState(webrtc::DataChannelInterface::kClosing);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("closing", channel->readyState());
  EXPECT_TRUE(scheduler->OptedOutFromAggressiveThrottlingForTest());

  // Transitioning to 'closed' stops the opt-out.
  webrtc_channel->ChangeState(webrtc::DataChannelInterface::kClosed);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("closed", channel->readyState());
  EXPECT_FALSE(scheduler->OptedOutFromAggressiveThrottlingForTest());
}

TEST_F(RTCDataChannelTest, TransfersDisabled) {
  V8TestingScope scope;
  ScopedTransferableRTCDataChannelForTest scoped_feature(/*enabled=*/false);

  rtc::scoped_refptr<MockDataChannel> webrtc_channel(
      new rtc::RefCountedObject<MockDataChannel>(signaling_thread()));
  auto* channel = MakeGarbageCollected<RTCDataChannel>(
      scope.GetExecutionContext(), webrtc_channel);

  EXPECT_FALSE(channel->IsTransferable());
}

TEST_F(RTCDataChannelTest, TransferableInCreationScopeOnly) {
  V8TestingScope scope;
  ScopedTransferableRTCDataChannelForTest scoped_feature(/*enabled=*/true);

  rtc::scoped_refptr<MockDataChannel> webrtc_channel(
      new rtc::RefCountedObject<MockDataChannel>(signaling_thread()));
  auto* channel = MakeGarbageCollected<RTCDataChannel>(
      scope.GetExecutionContext(), webrtc_channel);

  EXPECT_TRUE(channel->IsTransferable());

  // RTCDataChannel cannot be transferred once it has connected to
  // `webrtc_channel`, as we could lose incoming messages during the transfer.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(channel->IsTransferable());
}

TEST_F(RTCDataChannelTest, TransferAllowedOnlyOnce) {
  V8TestingScope scope;
  ScopedTransferableRTCDataChannelForTest scoped_feature(/*enabled=*/true);

  rtc::scoped_refptr<MockDataChannel> webrtc_channel(
      new rtc::RefCountedObject<MockDataChannel>(signaling_thread()));
  auto* channel = MakeGarbageCollected<RTCDataChannel>(
      scope.GetExecutionContext(), webrtc_channel);

  EXPECT_TRUE(channel->IsTransferable());
  EXPECT_NE(channel->TransferUnderlyingChannel(), nullptr);

  // The channel should no longer be transferable.
  EXPECT_FALSE(channel->IsTransferable());
}

TEST_F(RTCDataChannelTest, SendPreventsTransfers) {
  {
    SCOPED_TRACE("RTCDataChannel::send(const string&)");
    VerifyNoTransfersAfterSend(WTF::BindOnce([](RTCDataChannel* channel) {
      String message(std::string(100, 'A').c_str());
      channel->send(message, IGNORE_EXCEPTION_FOR_TESTING);
    }));
  }

  {
    SCOPED_TRACE("RTCDataChannel::send(DOMArrayBuffer*)");
    VerifyNoTransfersAfterSend(WTF::BindOnce([](RTCDataChannel* channel) {
      DOMArrayBuffer* buffer = DOMArrayBuffer::Create(10, 4);
      channel->send(buffer, IGNORE_EXCEPTION_FOR_TESTING);
    }));
  }

  {
    SCOPED_TRACE("RTCDataChannel::send(NotShared<DOMArrayBufferView>)");
    VerifyNoTransfersAfterSend(WTF::BindOnce([](RTCDataChannel* channel) {
      DOMArrayBuffer* buffer = DOMArrayBuffer::Create(10, 4);
      channel->send(
          NotShared<DOMArrayBufferView>(DOMDataView::Create(buffer, 0, 10)),
          IGNORE_EXCEPTION_FOR_TESTING);
    }));
  }

  {
    SCOPED_TRACE("RTCDataChannel::send(Blob*)");
    VerifyNoTransfersAfterSend(WTF::BindOnce([](RTCDataChannel* channel) {
      const char kHelloWorld[] = "Hello world!";
      Blob* blob = Blob::Create(
          base::as_bytes(base::span_with_nul_from_cstring(kHelloWorld)),
          "text/plain");
      channel->send(blob, IGNORE_EXCEPTION_FOR_TESTING);
    }));
  }
}

TEST_F(RTCDataChannelTest, NoSendAfterClose) {
  V8TestingScope scope;

  rtc::scoped_refptr<MockDataChannel> webrtc_channel(
      new rtc::RefCountedObject<MockDataChannel>(signaling_thread()));
  auto* channel = MakeGarbageCollected<RTCDataChannel>(
      scope.GetExecutionContext(), webrtc_channel);
  channel->close();

  {
    SCOPED_TRACE("RTCDataChannel::send(const string&)");
    String message(std::string(100, 'A').c_str());
    DummyExceptionStateForTesting exception_state;
    channel->send(message, exception_state);
    EXPECT_TRUE(exception_state.HadException());
  }

  {
    SCOPED_TRACE("RTCDataChannel::send(DOMArrayBuffer*)");
    DOMArrayBuffer* buffer = DOMArrayBuffer::Create(10, 4);
    DummyExceptionStateForTesting exception_state;
    channel->send(buffer, exception_state);
    EXPECT_TRUE(exception_state.HadException());
  }

  {
    SCOPED_TRACE("RTCDataChannel::send(NotShared<DOMArrayBufferView>)");
    DOMArrayBuffer* buffer = DOMArrayBuffer::Create(10, 4);
    DummyExceptionStateForTesting exception_state;
    channel->send(
        NotShared<DOMArrayBufferView>(DOMDataView::Create(buffer, 0, 10)),
        exception_state);
    EXPECT_TRUE(exception_state.HadException());
  }

  {
    SCOPED_TRACE("RTCDataChannel::send(Blob*)");
    const char kHelloWorld[] = "Hello world!";
    Blob* blob = Blob::Create(
        base::as_bytes(base::span_with_nul_from_cstring(kHelloWorld)),
        "text/plain");
    DummyExceptionStateForTesting exception_state;
    channel->send(blob, exception_state);
    EXPECT_TRUE(exception_state.HadException());
  }
}

}  // namespace blink
