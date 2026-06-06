// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/browser/renderer_host/text_input_client_mac.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <variant>

#include "base/containers/queue.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "content/browser/renderer_host/text_input_host_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/range/range.h"

namespace content {

namespace {

using ::testing::Bool;
using ::testing::Combine;
using ::testing::Values;

// Value for TextInputClientMac timeout. These aren't specified directly in
// INSTANTIATE_TEST_SUITE_P because TestTimeouts isn't initialized before the
// test suit constructor.
enum class TimeoutParam {
  // Mosts tests use a long timeout, to make sure there's enough time for test
  // responses to arrive.
  kLongTimeout,
  // Tests that expect a timeout to happen use a short timeout.
  kShortTimeout,
};

// TextInputClientMacTest exercises two sync functions,
// GetCharacterIndexAtPoint() and GetFirstRectForRange(), that should have
// identical behaviour except for their return type.
enum class FunctionToTest {
  kGetCharacterIndexAtPoint,
  kGetFirstRectForRange,
};

// GetCharacterIndexAtPoint() returns uint32_t.
// GetFirstRectForRange() returns gfx::Rect.
using ResponseType = std::variant<uint32_t, gfx::Rect>;

// Fake that replaces mojo messages sent through LocalFrame by posting tasks
// directly to TextInputHostImpl on the IO thread.
//
// The standard way to implement the receiver of LocalFrame messages in unit
// tests is FakeLocalFrame, but its receivers are bound to the main test thread,
// not the IO thread. Blocking TextInputClientMac methods are also called on the
// main thread, and wait for the responses to those messages, so receivers bound
// to FakeLocalFrame won't get called until after the blocking method times out.
class FakeAsyncRequestDelegate final
    : public TextInputClientMac::AsyncRequestDelegate {
 public:
  using BeforeResponseCallback = base::OnceCallback<void(RenderFrameHost*)>;

  explicit FakeAsyncRequestDelegate(FunctionToTest function_to_test)
      : function_to_test_(function_to_test) {
    // Wait until `host_impl_` is created on the IO thread.
    base::test::TestFuture<std::unique_ptr<TextInputHostImpl>> host_future;
    GetIOThreadTaskRunner()->PostTaskAndReplyWithResult(
        FROM_HERE, base::BindOnce(&std::make_unique<TextInputHostImpl>),
        host_future.GetCallback());
    host_impl_ = host_future.Take();
  }

  ~FakeAsyncRequestDelegate() final {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // Detach `host_impl_` and free it on the IO thread.
    GetIOThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::DoNothingWithBoundArgs(std::exchange(host_impl_, nullptr)));
  }

  FakeAsyncRequestDelegate(const FakeAsyncRequestDelegate&) = delete;
  FakeAsyncRequestDelegate& operator=(const FakeAsyncRequestDelegate&) = delete;

  FunctionToTest function_to_test() const { return function_to_test_; }

  // Adds `response`, to be sent after `delay`, to the queue.
  // `before_response_callback` will be called before the delay starts so that
  // tests can add extra steps.
  void AddResponse(
      ResponseType response,
      base::TimeDelta delay = TestTimeouts::tiny_timeout(),
      BeforeResponseCallback before_response_callback = base::DoNothing()) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    responses_.emplace(std::move(response), delay,
                       std::move(before_response_callback));
  }

  size_t NumResponses() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return responses_.size();
  }

  // AsyncRequestDelegate:

  void GetCharacterIndexAtPoint(
      RenderFrameHost* rfh,
      const TextInputClientMac::RequestToken& request_token,
      const gfx::Point& point) final {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    ASSERT_EQ(function_to_test_, FunctionToTest::kGetCharacterIndexAtPoint);
    SendNextResponse(rfh, request_token);
  }

  void GetFirstRectForRange(
      RenderFrameHost* rfh,
      const TextInputClientMac::RequestToken& request_token,
      const gfx::Range& range) final {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    ASSERT_EQ(function_to_test_, FunctionToTest::kGetFirstRectForRange);
    SendNextResponse(rfh, request_token);
  }

 private:
  // Calls the appropriate Got*() method of `host` based on `function_to_test`,
  // with the given `request_token` and `response` as params. This will unblock
  // the waiting TextInputClientMac.
  static void TextInputHostGotResponse(
      FunctionToTest function_to_test,
      TextInputHostImpl* host,
      const TextInputClientMac::RequestToken& request_token,
      ResponseType response) {
    switch (function_to_test) {
      case FunctionToTest::kGetCharacterIndexAtPoint:
        ASSERT_TRUE(std::holds_alternative<uint32_t>(response));
        host->GotCharacterIndexAtPoint(request_token.value(),
                                       std::get<uint32_t>(response));
        break;
      case FunctionToTest::kGetFirstRectForRange:
        ASSERT_TRUE(std::holds_alternative<gfx::Rect>(response));
        host->GotFirstRectForRange(request_token.value(),
                                   std::get<gfx::Rect>(response));
        break;
    }
  }

  void SendNextResponse(RenderFrameHost* rfh,
                        const TextInputClientMac::RequestToken& request_token) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    ASSERT_TRUE(rfh);
    if (responses_.empty()) {
      return;
    }
    auto& [response, delay, before_response_callback] = responses_.front();
    std::move(before_response_callback).Run(rfh);

    if (delay.is_zero()) {
      // Poke the response into TextInputClient, bypassing TextInputHostImpl
      // which must be accessed on the IO thread. This simulates a response that
      // arrives while the calling thread is descheduled, before TextInputClient
      // blocks it.
      switch (function_to_test_) {
        case FunctionToTest::kGetCharacterIndexAtPoint:
          ASSERT_TRUE(std::holds_alternative<uint32_t>(response));
          TextInputClientMac::GetInstance()
              ->SetCharacterIndexWhileLockedForTesting(
                  request_token, std::get<uint32_t>(response));
          break;
        case FunctionToTest::kGetFirstRectForRange:
          ASSERT_TRUE(std::holds_alternative<gfx::Rect>(response));
          TextInputClientMac::GetInstance()->SetFirstRectWhileLockedForTesting(
              request_token, std::get<gfx::Rect>(response));
          break;
      }
    } else {
      // Unretained is safe since `host_impl_` is deleted on the IO thread.
      GetIOThreadTaskRunner()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&FakeAsyncRequestDelegate::TextInputHostGotResponse,
                         function_to_test_, base::Unretained(host_impl_.get()),
                         request_token, std::move(response)),
          delay);
    }

    responses_.pop();
  }

  const FunctionToTest function_to_test_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Queue of responses to send, with delay.
  base::queue<std::tuple<ResponseType, base::TimeDelta, BeforeResponseCallback>>
      responses_ GUARDED_BY_CONTEXT(sequence_checker_);

  // The TextInputHostImpl object must be accessed on the IO thread, but the
  // pointer to it must be accessed on this sequence. It's not using
  // SequenceBound because that doesn't easily support delayed tasks.
  std::unique_ptr<TextInputHostImpl> host_impl_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

// This test does not test the Blink side of the dictionary system (which
// performs the actual data fetching), but rather this just tests that the
// service's signaling system works.
class TextInputClientMacTest
    : public content::RenderViewHostTestHarness,
      public ::testing::WithParamInterface<
          std::tuple<FunctionToTest, bool, TimeoutParam>> {
 public:
  TextInputClientMacTest()
      : RenderViewHostTestHarness(BrowserTaskEnvironment::REAL_IO_THREAD),
        function_to_test_(std::get<0>(GetParam())),
        is_sync_(std::get<1>(GetParam())) {
    base::TimeDelta ipc_timeout;
    switch (std::get<2>(GetParam())) {
      case TimeoutParam::kLongTimeout:
        ipc_timeout = TestTimeouts::action_max_timeout();
        break;
      case TimeoutParam::kShortTimeout:
        // See SyncOrAsyncGetter_StaleResult for the exact value.
        ipc_timeout = TestTimeouts::tiny_timeout() * 1.5;
        break;
    }
    TextInputClientMac::GetInstance()->SetTimeoutForTesting(ipc_timeout);
  }

 protected:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    RenderViewHostTester::For(rvh())->CreateTestRenderView();

    auto delegate =
        std::make_unique<FakeAsyncRequestDelegate>(function_to_test_);
    delegate_ = delegate.get();
    TextInputClientMac::GetInstance()->SetAsyncRequestDelegateForTesting(
        std::move(delegate));
  }

  void TearDown() override {
    FlushIOThreadAndReplies();

    delegate_ = nullptr;
    TextInputClientMac::GetInstance()->SetAsyncRequestDelegateForTesting(
        nullptr);
    TextInputClientMac::GetInstance()->SetTimeoutForTesting(
        base::Milliseconds(1500));

    RenderViewHostTestHarness::TearDown();
  }

  // Flush any tasks posted to the IO thread and their reply tasks.
  void FlushIOThreadAndReplies() {
    GetIOThreadTaskRunner()->PostTaskAndReply(
        FROM_HERE, base::DoNothing(), task_environment()->QuitClosure());
    task_environment()->RunUntilQuit();
  }

  // Initializes a ResponseType value from an arbitrary integer.
  ResponseType CreateResponse(unsigned value) const {
    switch (function_to_test_) {
      case FunctionToTest::kGetCharacterIndexAtPoint:
        return value;
      case FunctionToTest::kGetFirstRectForRange:
        return gfx::Rect(value, value, value, value);
    }
  }

  // The ResponseType value that's used when there's no focused widget.
  ResponseType NoFocusResponse() const {
    switch (function_to_test_) {
      case FunctionToTest::kGetCharacterIndexAtPoint:
        return 0u;
      case FunctionToTest::kGetFirstRectForRange:
        return gfx::Rect();
    }
  }

  // The ResponseType value that's used when the sync getter method times out.
  ResponseType TimeoutResponse() const {
    switch (function_to_test_) {
      case FunctionToTest::kGetCharacterIndexAtPoint:
        return UINT32_MAX;
      case FunctionToTest::kGetFirstRectForRange:
        return gfx::Rect();
    }
  }

  // Wrappers to call the TextInputClientMac getter method under test, with
  // `rwh` as a parameter.

  ResponseType TextInputClientSyncGet(RenderWidgetHost* rwh) const {
    switch (function_to_test_) {
      case FunctionToTest::kGetCharacterIndexAtPoint:
        return TextInputClientMac::GetInstance()->SyncGetCharacterIndexAtPoint(
            rwh, gfx::Point(2, 2));
      case FunctionToTest::kGetFirstRectForRange:
        return TextInputClientMac::GetInstance()->SyncGetFirstRectForRange(
            rwh, gfx::Range(NSMakeRange(0, 32)));
    }
  }

  base::test::TestFuture<ResponseType> TextInputClientAsyncGet(
      RenderWidgetHost* rwh) const {
    base::test::TestFuture<ResponseType> future;
    switch (function_to_test_) {
      case FunctionToTest::kGetCharacterIndexAtPoint:
        TextInputClientMac::GetInstance()->AsyncGetCharacterIndexAtPoint(
            rwh, gfx::Point(2, 2), base::BindOnce([](uint32_t index) {
                                     return ResponseType(index);
                                   }).Then(future.GetCallback()));
        break;
      case FunctionToTest::kGetFirstRectForRange:
        TextInputClientMac::GetInstance()->AsyncGetFirstRectForRange(
            rwh, gfx::Range(NSMakeRange(0, 32)),
            base::BindOnce([](gfx::Rect rect) {
              return ResponseType(rect);
            }).Then(future.GetCallback()));
        break;
    }
    return future;
  }

  ResponseType TextInputClientGet(RenderWidgetHost* rwh) const {
    if (is_sync_) {
      return TextInputClientSyncGet(rwh);
    }
    return TextInputClientAsyncGet(rwh).Get();
  }

  RenderWidgetHost* widget() { return rvh()->GetWidget(); }

  FakeAsyncRequestDelegate& request_delegate() { return *delegate_; }

 private:
  FunctionToTest function_to_test_;
  bool is_sync_;
  raw_ptr<FakeAsyncRequestDelegate> delegate_ = nullptr;
};

using TextInputClientMacTimeoutTest = TextInputClientMacTest;

// Test cases that only apply to sync or async getters.
using TextInputClientMacSyncTest = TextInputClientMacTest;
using TextInputClientMacAsyncTest = TextInputClientMacTest;

INSTANTIATE_TEST_SUITE_P(
    All,
    TextInputClientMacTest,
    Combine(Values(FunctionToTest::kGetCharacterIndexAtPoint,
                   FunctionToTest::kGetFirstRectForRange),
            /*is_sync=*/Bool(),
            Values(TimeoutParam::kLongTimeout)));

INSTANTIATE_TEST_SUITE_P(
    All,
    TextInputClientMacTimeoutTest,
    Combine(Values(FunctionToTest::kGetCharacterIndexAtPoint,
                   FunctionToTest::kGetFirstRectForRange),
            /*is_sync=*/Bool(),
            Values(TimeoutParam::kShortTimeout)));

INSTANTIATE_TEST_SUITE_P(
    All,
    TextInputClientMacSyncTest,
    Combine(Values(FunctionToTest::kGetCharacterIndexAtPoint,
                   FunctionToTest::kGetFirstRectForRange),
            /*is_sync=*/Values(true),
            Values(TimeoutParam::kLongTimeout)));

INSTANTIATE_TEST_SUITE_P(
    All,
    TextInputClientMacAsyncTest,
    Combine(Values(FunctionToTest::kGetCharacterIndexAtPoint,
                   FunctionToTest::kGetFirstRectForRange),
            /*is_sync=*/Values(false),
            Values(TimeoutParam::kLongTimeout)));

}  // namespace

// Test Cases //////////////////////////////////////////////////////////////////

TEST_P(TextInputClientMacTest, SyncOrAsyncGetter_NoFocus) {
  // Return this value if the client (incorrectly) sends a request to an
  // unfocused frame.
  request_delegate().AddResponse(CreateResponse(42));
  EXPECT_EQ(TextInputClientGet(widget()), NoFocusResponse());
}

TEST_P(TextInputClientMacTest, SyncOrAsyncGetter_Basic) {
  const ResponseType kSuccessValue = CreateResponse(42);
  request_delegate().AddResponse(
      kSuccessValue, TestTimeouts::tiny_timeout(),
      base::BindLambdaForTesting(
          [this](RenderFrameHost* rfh) { EXPECT_EQ(rfh, this->main_rfh()); }));

  FocusWebContentsOnMainFrame();
  EXPECT_EQ(TextInputClientGet(widget()), kSuccessValue);
}

TEST_P(TextInputClientMacTimeoutTest, SyncOrAsyncGetter_Timeout) {
  FocusWebContentsOnMainFrame();
  EXPECT_EQ(TextInputClientGet(widget()), TimeoutResponse());
}

// Tests that TextInputClient doesn't get confused if TextInputHost sends a
// response that's also used if a request times out. (eg.
// GetCharacterIndexAtPoint() can return NSNotFound, which is UINT32_MAX.)
TEST_P(TextInputClientMacTest, SyncOrAsyncGetter_NotFound) {
  // Set an arbitrary value to ensure the response doesn't just default to the
  // timeout value.
  const ResponseType kPreviousValue = CreateResponse(42);
  request_delegate().AddResponse(kPreviousValue);

  // Set the response to the timeout value after the previous setting.
  request_delegate().AddResponse(TimeoutResponse());

  FocusWebContentsOnMainFrame();
  EXPECT_EQ(TextInputClientGet(widget()), kPreviousValue);
  EXPECT_EQ(TextInputClientGet(widget()), TimeoutResponse());
}

// Tests that TextInputClient doesn't get confused if TextInputHost sends a
// response before the calling thread blocks.
TEST_P(TextInputClientMacSyncTest, SyncGetter_Immediate) {
  // A response with 0 delay is sent immediately, not posted.
  const ResponseType kSuccessValue = CreateResponse(42);
  request_delegate().AddResponse(kSuccessValue, base::TimeDelta());

  FocusWebContentsOnMainFrame();
  EXPECT_EQ(TextInputClientGet(widget()), kSuccessValue);
}

// Tests that TextInputClient sends a request to the focused frame, even if it's
// not the main frame.
TEST_P(TextInputClientMacTest, SyncOrAsyncGetter_ChildFrame) {
  const ResponseType kSuccessValue = CreateResponse(42);

  RenderFrameHost* child_rfh =
      RenderFrameHostTester::For(main_rfh())->AppendChild("child frame");
  FocusWebContentsOnFrame(child_rfh);

  request_delegate().AddResponse(
      kSuccessValue, TestTimeouts::tiny_timeout(),
      base::BindLambdaForTesting(
          [child_rfh](RenderFrameHost* rfh) { EXPECT_EQ(child_rfh, rfh); }));

  EXPECT_EQ(TextInputClientGet(widget()), kSuccessValue);
}

// Tests that TextInputClient can handle a frame being deleted while waiting for
// a reply.
TEST_P(TextInputClientMacTest, SyncOrAsyncGetter_DeleteFrame) {
  const ResponseType kSuccessValue = CreateResponse(42);

  request_delegate().AddResponse(
      kSuccessValue, TestTimeouts::tiny_timeout(),
      base::BindLambdaForTesting([this](RenderFrameHost* rfh) {
        EXPECT_EQ(WebContents::FromRenderFrameHost(rfh), this->web_contents());
        this->DeleteContents();
      }));
  FocusWebContentsOnMainFrame();

  // GetFirstRectForRange needs the frame to do coordinate translation.
  EXPECT_EQ(TextInputClientGet(widget()),
            request_delegate().function_to_test() ==
                    FunctionToTest::kGetFirstRectForRange
                ? NoFocusResponse()
                : kSuccessValue);
}

// Tests that multiple async calls can be made at the same time.
TEST_P(TextInputClientMacAsyncTest, AsyncGetter_MultipleCalls) {
  const ResponseType kSuccessValue1 = CreateResponse(42);
  const ResponseType kSuccessValue2 = CreateResponse(43);
  const ResponseType kSuccessValue3 = CreateResponse(44);
  const ResponseType kSuccessValue4 = CreateResponse(45);

  // Responses arrive in reverse order, except that the last request arrives
  // between 2 and 3.
  request_delegate().AddResponse(kSuccessValue1,
                                 3 * TestTimeouts::tiny_timeout());
  request_delegate().AddResponse(kSuccessValue2,
                                 2 * TestTimeouts::tiny_timeout());
  request_delegate().AddResponse(kSuccessValue3,
                                 1 * TestTimeouts::tiny_timeout());
  request_delegate().AddResponse(kSuccessValue4,
                                 1.5 * TestTimeouts::tiny_timeout());

  FocusWebContentsOnMainFrame();

  base::test::TestFuture<ResponseType> result_future1 =
      TextInputClientAsyncGet(widget());
  base::test::TestFuture<ResponseType> result_future2 =
      TextInputClientAsyncGet(widget());
  base::test::TestFuture<ResponseType> result_future3 =
      TextInputClientAsyncGet(widget());
  ResponseType result4 = TextInputClientSyncGet(widget());

  // Even though result 3 has arrived on the IO thread, it shouldn't be
  // delivered to the UI thread because nothing pumped the message loop after
  // the sync getter was unblocked. This shows that async callbacks won't be
  // invoked in the middle of a sync method.
  EXPECT_EQ(result4, kSuccessValue4);
  EXPECT_FALSE(result_future3.IsReady());
  EXPECT_FALSE(result_future2.IsReady());
  EXPECT_FALSE(result_future1.IsReady());

  // TestFuture::Get() pumps the message loop, so now the async results start to
  // arrive.
  EXPECT_EQ(result_future3.Get(), kSuccessValue3);
  EXPECT_FALSE(result_future2.IsReady());
  EXPECT_FALSE(result_future1.IsReady());

  EXPECT_EQ(result_future2.Get(), kSuccessValue2);
  EXPECT_FALSE(result_future1.IsReady());

  EXPECT_EQ(result_future1.Get(), kSuccessValue1);
}

// Tests that TextInputClient ignores replies that arrive after it times out.
TEST_P(TextInputClientMacTimeoutTest, SyncOrAsyncGetter_StaleResult) {
  const ResponseType kStaleValue = CreateResponse(42);
  const ResponseType kSuccessValue = CreateResponse(84);

  // The timeout must be set to 1.5 * delay.
  // eg. With delay = 200ms and timeout = 300ms:
  // T=0: First request sent.
  // T=300ms: First request times out, second request sent.
  // T=400ms: First reply arrives. (Should be ignored.)
  // T=500ms: Second reply arrives, 200ms after the request.
  // T=600ms: Second request times out. (Shouldn't reach here, but see below...)
  const base::TimeDelta delay = TestTimeouts::tiny_timeout();
  ASSERT_EQ(TextInputClientMac::GetInstance()->GetTimeoutForTesting(),
            delay * 1.5);

  FocusWebContentsOnMainFrame();

  // Because the timeout is so close to `delay`, with unlucky thread
  // scheduling it's possible for the first response to arrive before the
  // timeout is checked, or for the second response to be delayed until after
  // the timeout. Raising the timeout would make the whole test unreasonably
  // slow, though. So on an unexpected timeout response, repeat the test.
  ResponseType first_response;
  ResponseType second_response;
  do {
    // Clear out responses from last time through the loop.
    FlushIOThreadAndReplies();
    ASSERT_EQ(request_delegate().NumResponses(), 0u);

    request_delegate().AddResponse(kStaleValue, delay * 2);
    request_delegate().AddResponse(kSuccessValue, delay);

    first_response = TextInputClientGet(widget());
    second_response = TextInputClientGet(widget());
  } while (first_response != TimeoutResponse() ||
           second_response == TimeoutResponse());
  EXPECT_EQ(first_response, TimeoutResponse());  // Replaces kStaleValue.
  EXPECT_EQ(second_response, kSuccessValue);
}

}  // namespace content
