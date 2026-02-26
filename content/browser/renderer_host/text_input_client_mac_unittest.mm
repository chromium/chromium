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
#include "content/common/features.h"
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

// Value for kTextInputClientIPCTimeout. These aren't specified directly in
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

class TextInputClientMacTestBase : public content::RenderViewHostTestHarness {
 public:
  TextInputClientMacTestBase(TimeoutParam timeout_param, bool use_nested_loop)
      : RenderViewHostTestHarness(BrowserTaskEnvironment::REAL_IO_THREAD) {
    base::TimeDelta ipc_timeout;
    switch (timeout_param) {
      case TimeoutParam::kLongTimeout:
        ipc_timeout = TestTimeouts::action_max_timeout();
        break;
      case TimeoutParam::kShortTimeout:
        // See SyncGetter_StaleResult for the exact value.
        ipc_timeout = TestTimeouts::tiny_timeout() * 1.5;
        break;
    }
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kTextInputClient,
        {{"ipc_timeout", absl::StrFormat("%dms", ipc_timeout.InMilliseconds())},
         {"use_nested_loop", use_nested_loop ? "true" : "false"}});
  }

 protected:
  virtual std::unique_ptr<TextInputClientMac::AsyncRequestDelegate>
  CreateDelegate() = 0;

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    RenderViewHostTester::For(rvh())->CreateTestRenderView();

    auto delegate = CreateDelegate();
    delegate_ = delegate.get();
    TextInputClientMac::GetInstance()->SetAsyncRequestDelegateForTesting(
        std::move(delegate));
  }

  void TearDown() override {
    FlushIOThreadAndReplies();

    delegate_ = nullptr;
    TextInputClientMac::GetInstance()->SetAsyncRequestDelegateForTesting(
        nullptr);

    RenderViewHostTestHarness::TearDown();
  }

  // Flush any tasks posted to the IO thread and their reply tasks.
  void FlushIOThreadAndReplies() {
    GetIOThreadTaskRunner()->PostTaskAndReply(
        FROM_HERE, base::DoNothing(), task_environment()->QuitClosure());
    task_environment()->RunUntilQuit();
  }

  TextInputClientMac::AsyncRequestDelegate* delegate() {
    return delegate_.get();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  raw_ptr<TextInputClientMac::AsyncRequestDelegate> delegate_ = nullptr;
};

// This test does not test the WebKit side of the dictionary system (which
// performs the actual data fetching), but rather this just tests that the
// service's signaling system works.
class TextInputClientMacTest
    : public TextInputClientMacTestBase,
      public ::testing::WithParamInterface<
          std::tuple<FunctionToTest, TimeoutParam, bool>> {
 public:
  TextInputClientMacTest()
      : TextInputClientMacTestBase(/*ipc_timeout=*/std::get<1>(GetParam()),
                                   /*use_nested_loop=*/std::get<2>(GetParam())),
        function_to_test_(std::get<0>(GetParam())) {}

 protected:
  std::unique_ptr<TextInputClientMac::AsyncRequestDelegate> CreateDelegate()
      override {
    return std::make_unique<FakeAsyncRequestDelegate>(function_to_test_);
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

  // Calls the TextInputClientMac sync getter method under test, with `rwh` as a
  // parameter.
  ResponseType TextInputClientGetSync(RenderWidgetHost* rwh) const {
    switch (function_to_test_) {
      case FunctionToTest::kGetCharacterIndexAtPoint:
        return TextInputClientMac::GetInstance()->GetCharacterIndexAtPoint(
            rwh, gfx::Point(2, 2));
      case FunctionToTest::kGetFirstRectForRange:
        return TextInputClientMac::GetInstance()->GetFirstRectForRange(
            rwh, gfx::Range(NSMakeRange(0, 32)));
    }
  }

  RenderWidgetHost* widget() { return rvh()->GetWidget(); }

  FakeAsyncRequestDelegate& request_delegate() {
    return *reinterpret_cast<FakeAsyncRequestDelegate*>(delegate());
  }

 private:
  FunctionToTest function_to_test_;
};

using TextInputClientMacTimeoutTest = TextInputClientMacTest;

INSTANTIATE_TEST_SUITE_P(
    All,
    TextInputClientMacTest,
    Combine(Values(FunctionToTest::kGetCharacterIndexAtPoint,
                   FunctionToTest::kGetFirstRectForRange),
            Values(TimeoutParam::kLongTimeout),
            Bool()));

INSTANTIATE_TEST_SUITE_P(
    All,
    TextInputClientMacTimeoutTest,
    Combine(Values(FunctionToTest::kGetCharacterIndexAtPoint,
                   FunctionToTest::kGetFirstRectForRange),
            Values(TimeoutParam::kShortTimeout),
            Bool()));

class TextInputClientMacReentryDeathTest
    : public TextInputClientMacTestBase,
      public ::testing::WithParamInterface<bool> {
 public:
  TextInputClientMacReentryDeathTest()
      : TextInputClientMacTestBase(TimeoutParam::kLongTimeout,
                                   /*use_nested_loop=*/GetParam()) {}

 protected:
  // A delegate that calls back into TextInputClientMac on the same thread. This
  // should fail with a CHECK because reentry is unsafe.
  class Delegate final : public TextInputClientMac::AsyncRequestDelegate {
   public:
    void GetCharacterIndexAtPoint(
        RenderFrameHost* rfh,
        const TextInputClientMac::RequestToken& request_token,
        const gfx::Point& point) final {
      ASSERT_TRUE(rfh);
      TextInputClientMac::GetInstance()->GetFirstRectForRange(
          rfh->GetRenderWidgetHost(), gfx::Range(NSMakeRange(0, 32)));
    }

    void GetFirstRectForRange(
        RenderFrameHost* rfh,
        const TextInputClientMac::RequestToken& request_token,
        const gfx::Range& range) final {
      ASSERT_TRUE(rfh);
      TextInputClientMac::GetInstance()->GetCharacterIndexAtPoint(
          rfh->GetRenderWidgetHost(), gfx::Point(2, 2));
    }
  };

  std::unique_ptr<TextInputClientMac::AsyncRequestDelegate> CreateDelegate()
      override {
    return std::make_unique<Delegate>();
  }

  void SetUp() override {
    TextInputClientMacTestBase::SetUp();
    FocusWebContentsOnMainFrame();
  }
};

INSTANTIATE_TEST_SUITE_P(All, TextInputClientMacReentryDeathTest, Bool());

}  // namespace

// Test Cases //////////////////////////////////////////////////////////////////

TEST_P(TextInputClientMacTest, SyncGetter_NoFocus) {
  // Return this value if the client (incorrectly) sends a request to an
  // unfocused frame.
  request_delegate().AddResponse(CreateResponse(42));
  EXPECT_EQ(TextInputClientGetSync(widget()), NoFocusResponse());
}

TEST_P(TextInputClientMacTest, SyncGetter_Basic) {
  const ResponseType kSuccessValue = CreateResponse(42);
  request_delegate().AddResponse(
      kSuccessValue, TestTimeouts::tiny_timeout(),
      base::BindLambdaForTesting(
          [this](RenderFrameHost* rfh) { EXPECT_EQ(rfh, this->main_rfh()); }));

  FocusWebContentsOnMainFrame();
  EXPECT_EQ(TextInputClientGetSync(widget()), kSuccessValue);
}

TEST_P(TextInputClientMacTimeoutTest, SyncGetter_Timeout) {
  FocusWebContentsOnMainFrame();
  EXPECT_EQ(TextInputClientGetSync(widget()), TimeoutResponse());
}

// Tests that TextInputClient doesn't get confused if TextInputHost sends a
// response that's also used if a request times out. (eg.
// GetCharacterIndexAtPoint() can return NSNotFound, which is UINT32_MAX.)
TEST_P(TextInputClientMacTest, SyncGetter_NotFound) {
  // Set an arbitrary value to ensure the response doesn't just default to the
  // timeout value.
  const ResponseType kPreviousValue = CreateResponse(42);
  request_delegate().AddResponse(kPreviousValue);

  // Set the response to the timeout value after the previous setting.
  request_delegate().AddResponse(TimeoutResponse());

  FocusWebContentsOnMainFrame();
  EXPECT_EQ(TextInputClientGetSync(widget()), kPreviousValue);
  EXPECT_EQ(TextInputClientGetSync(widget()), TimeoutResponse());
}

// Tests that TextInputClient doesn't get confused if TextInputHost sends a
// response before the calling thread blocks.
TEST_P(TextInputClientMacTest, SyncGetter_Immediate) {
  // A response with 0 delay is sent immediately, not posted.
  const ResponseType kSuccessValue = CreateResponse(42);
  request_delegate().AddResponse(kSuccessValue, base::TimeDelta());

  FocusWebContentsOnMainFrame();
  EXPECT_EQ(TextInputClientGetSync(widget()), kSuccessValue);
}

// Tests that TextInputClient sends a request to the focused frame, even if it's
// not the main frame.
TEST_P(TextInputClientMacTest, SyncGetter_ChildFrame) {
  const ResponseType kSuccessValue = CreateResponse(42);

  RenderFrameHost* child_rfh =
      RenderFrameHostTester::For(main_rfh())->AppendChild("child frame");
  FocusWebContentsOnFrame(child_rfh);

  request_delegate().AddResponse(
      kSuccessValue, TestTimeouts::tiny_timeout(),
      base::BindLambdaForTesting(
          [child_rfh](RenderFrameHost* rfh) { EXPECT_EQ(child_rfh, rfh); }));

  EXPECT_EQ(TextInputClientGetSync(widget()), kSuccessValue);
}

// Tests that TextInputClient can handle a frame being deleted by a nested
// RunLoop while waiting for a reply.
TEST_P(TextInputClientMacTest, SyncGetter_DeleteFrame) {
  const ResponseType kSuccessValue = CreateResponse(42);

  request_delegate().AddResponse(
      kSuccessValue, TestTimeouts::tiny_timeout(),
      base::BindLambdaForTesting([this](RenderFrameHost* rfh) {
        EXPECT_EQ(WebContents::FromRenderFrameHost(rfh), this->web_contents());
        this->DeleteContents();
      }));
  FocusWebContentsOnMainFrame();

  // GetFirstRectForRange needs the frame to do coordinate translation.
  EXPECT_EQ(TextInputClientGetSync(widget()),
            request_delegate().function_to_test() ==
                    FunctionToTest::kGetFirstRectForRange
                ? NoFocusResponse()
                : kSuccessValue);
}

// Tests that TextInputClient ignores replies that arrive after it times out.
TEST_P(TextInputClientMacTimeoutTest, SyncGetter_StaleResult) {
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
  ASSERT_EQ(features::kTextInputClientIPCTimeout.Get(), delay * 1.5);

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

    first_response = TextInputClientGetSync(widget());
    second_response = TextInputClientGetSync(widget());
  } while (first_response != TimeoutResponse() ||
           second_response == TimeoutResponse());
  EXPECT_EQ(first_response, TimeoutResponse());  // Replaces kStaleValue.
  EXPECT_EQ(second_response, kSuccessValue);
}

TEST_P(TextInputClientMacReentryDeathTest, GetCharacterIndexAtPoint) {
  EXPECT_CHECK_DEATH(
      TextInputClientMac::GetInstance()->GetCharacterIndexAtPoint(
          rvh()->GetWidget(), gfx::Point(2, 2)));
}

TEST_P(TextInputClientMacReentryDeathTest, GetFirstRectForRange) {
  EXPECT_CHECK_DEATH(TextInputClientMac::GetInstance()->GetFirstRectForRange(
      rvh()->GetWidget(), gfx::Range(NSMakeRange(0, 32))));
}

}  // namespace content
