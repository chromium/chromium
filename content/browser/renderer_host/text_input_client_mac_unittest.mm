// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/browser/renderer_host/text_input_client_mac.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <type_traits>
#include <utility>

#include "base/containers/queue.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/text_input_host_impl.h"
#include "content/common/features.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/range/range.h"

namespace content {

namespace {

constexpr base::TimeDelta kTaskDelay = base::Milliseconds(200);

// TextInputClientMacTest exercises two sync functions,
// GetCharacterIndexAtPoint() and GetFirstRectForRange(), that should have
// identical behaviour except for their return type. To avoid repeated test
// code, this traits class implements only the code that must change to test
// each function, parameteriezd on the return type.
template <typename ResponseType>
struct TestTraits {
  // Calls the TextInputClientMac sync getter method under test, with `rwh` as a
  // parameter.
  static ResponseType TextInputClientGetSync(RenderWidgetHost* rwh);

  // Calls the appropriate Got*() method of `host` with the given `response`,
  // which will unblock the waiting TextInputClientMac.
  static void TextInputHostGotResponse(TextInputHostImpl* host,
                                       ResponseType response);

  // Synchronously calls a test-only setter method on TextInputClientMac, to
  // simulate a response being received before returning from the delegate.
  static void TextInputClientSetSync(ResponseType response);

  // Initializes a ResponseType value from an arbitrary integer.
  static constexpr ResponseType CreateResponse(int value);

  // The ResponseType value that's used when the sync getter method times out.
  static constexpr ResponseType kTimeoutResponse;
};

template <>
struct TestTraits<uint32_t> {
  static uint32_t TextInputClientGetSync(RenderWidgetHost* rwh) {
    return TextInputClientMac::GetInstance()->GetCharacterIndexAtPoint(
        rwh, gfx::Point(2, 2));
  }

  static void TextInputHostGotResponse(TextInputHostImpl* host,
                                       uint32_t response) {
    host->GotCharacterIndexAtPoint(response);
  }

  static void TextInputClientSetSync(uint32_t response) {
    TextInputClientMac::GetInstance()->SetCharacterIndexWhileLockedForTesting(
        response);
  }

  static constexpr uint32_t CreateResponse(int value) { return value; }

  static constexpr uint32_t kTimeoutResponse = UINT32_MAX;
};

template <>
struct TestTraits<gfx::Rect> {
  static gfx::Rect TextInputClientGetSync(RenderWidgetHost* rwh) {
    return TextInputClientMac::GetInstance()->GetFirstRectForRange(
        rwh, gfx::Range(NSMakeRange(0, 32)));
  }

  static void TextInputHostGotResponse(TextInputHostImpl* host,
                                       gfx::Rect response) {
    host->GotFirstRectForRange(response);
  }

  static void TextInputClientSetSync(gfx::Rect response) {
    TextInputClientMac::GetInstance()->SetFirstRectWhileLockedForTesting(
        response);
  }

  static constexpr gfx::Rect CreateResponse(int value) {
    return gfx::Rect(value, value, value, value);
  }

  static constexpr gfx::Rect kTimeoutResponse = gfx::Rect();
};

// Fake that replaces mojo messages sent through LocalFrame by posting tasks
// directly to TextInputHostImpl on the IO thread.
//
// The standard way to implement the receiver of LocalFrame messages in unit
// tests is FakeLocalFrame, but its recievers are bound to the main test thread,
// not the IO thread. Blocking TextInputClientMac methods are also called on the
// main thread, and wait for the responses to those messages, so receivers bound
// to FakeLocalFrame won't get called until after the blocking method times out.
template <typename ResponseType>
class FakeAsyncRequestDelegate final
    : public TextInputClientMac::AsyncRequestDelegate {
 public:
  using Traits = TestTraits<ResponseType>;

  FakeAsyncRequestDelegate(RenderWidgetHost* widget) : widget_(widget) {
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

  void AddResponse(ResponseType response, base::TimeDelta delay) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    responses_.emplace(std::move(response), delay);
  }

  // AsyncRequestDelegate:

  void GetCharacterIndexAtPoint(RenderFrameHost* rfh,
                                const gfx::Point& point) final {
    if constexpr (std::is_same_v<ResponseType, uint32_t>) {
      SendNextResponse(rfh);
    } else {
      FAIL() << "Wrong test type for GetCharacterIndexAtPoint";
    }
  }

  void GetFirstRectForRange(RenderFrameHost* rfh,
                            const gfx::Range& range) final {
    if constexpr (std::is_same_v<ResponseType, gfx::Rect>) {
      SendNextResponse(rfh);
    } else {
      FAIL() << "Wrong test type for GetFirstRectForRange";
    }
  }

 private:
  void SendNextResponse(RenderFrameHost* rfh) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    ASSERT_TRUE(rfh);
    ASSERT_EQ(rfh->GetRenderWidgetHost(), widget_);
    if (responses_.empty()) {
      return;
    }
    auto [response, delay] = responses_.front();
    responses_.pop();

    if (delay.is_zero()) {
      // Poke the response into TextInputClient, bypassing TextInputHostImpl
      // which must be accessed on the IO thread. This simulates a response that
      // arrives while the calling thread is descheduled, before TextInputClient
      // blocks it.
      Traits::TextInputClientSetSync(response);
    } else {
      // Unretained is safe since `host_impl_` is deleted on the IO thread.
      GetIOThreadTaskRunner()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&Traits::TextInputHostGotResponse,
                         base::Unretained(host_impl_.get()),
                         std::move(response)),
          delay);
    }
  }

  SEQUENCE_CHECKER(sequence_checker_);

  raw_ptr<RenderWidgetHost> widget_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Queue of responses to send, with delay.
  base::queue<std::pair<ResponseType, base::TimeDelta>> responses_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // The TextInputHostImpl object must be accessed on the IO thread, but the
  // pointer to it must be accessed on this sequence. It's not using
  // SequenceBound because that doesn't easily support delayed tasks.
  std::unique_ptr<TextInputHostImpl> host_impl_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

// This test does not test the WebKit side of the dictionary system (which
// performs the actual data fetching), but rather this just tests that the
// service's signaling system works.
//
// Tests a different sync getter function depending on ResponseType:
//  * uint32_t -> GetCharacterIndexAtPoint()
//  * gfx::Rect -> GetFirstRectForRange()
template <typename ResponseType>
class TextInputClientMacTest : public content::RenderViewHostTestHarness {
 public:
  using Delegate = FakeAsyncRequestDelegate<ResponseType>;

  TextInputClientMacTest()
      : RenderViewHostTestHarness(BrowserTaskEnvironment::REAL_IO_THREAD) {}

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    RenderViewHostTester::For(rvh())->CreateTestRenderView();

    auto delegate = std::make_unique<Delegate>(widget());
    request_delegate_ = delegate.get();
    TextInputClientMac::GetInstance()->SetAsyncRequestDelegateForTesting(
        std::move(delegate));
  }

  void TearDown() override {
    // Flush any tasks posted to the IO thread and reply tasks before exiting.
    GetIOThreadTaskRunner()->PostTaskAndReply(
        FROM_HERE, base::DoNothing(), task_environment()->QuitClosure());
    task_environment()->RunUntilQuit();

    request_delegate_ = nullptr;
    TextInputClientMac::GetInstance()->SetAsyncRequestDelegateForTesting(
        nullptr);

    RenderViewHostTestHarness::TearDown();
  }

  RenderWidgetHost* widget() { return rvh()->GetWidget(); }

  Delegate& request_delegate() { return *request_delegate_; }

 private:
  raw_ptr<Delegate> request_delegate_;
};

TYPED_TEST_SUITE_P(TextInputClientMacTest);

}  // namespace

// Test Cases //////////////////////////////////////////////////////////////////

TYPED_TEST_P(TextInputClientMacTest, SyncGetter_NoFocus) {
  using Traits = TestTraits<TypeParam>;

  // Return this value if the client (incorrectly) sends a request to an
  // unfocused frame.
  this->request_delegate().AddResponse(Traits::CreateResponse(42), kTaskDelay);
  EXPECT_EQ(Traits::TextInputClientGetSync(this->widget()), TypeParam{});
}

TYPED_TEST_P(TextInputClientMacTest, SyncGetter_Basic) {
  using Traits = TestTraits<TypeParam>;

  constexpr TypeParam kSuccessValue = Traits::CreateResponse(42);
  this->request_delegate().AddResponse(kSuccessValue, kTaskDelay);

  this->FocusWebContentsOnMainFrame();
  EXPECT_EQ(Traits::TextInputClientGetSync(this->widget()), kSuccessValue);
}

TYPED_TEST_P(TextInputClientMacTest, SyncGetter_Timeout) {
  using Traits = TestTraits<TypeParam>;

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(features::kTextInputClient,
                                                  {{"ipc_timeout", "300ms"}});

  this->FocusWebContentsOnMainFrame();
  EXPECT_EQ(Traits::TextInputClientGetSync(this->widget()),
            Traits::kTimeoutResponse);
}

// Tests that TextInputClient doesn't get confused if TextInputHost sends a
// response that's also used if a request times out. (eg.
// GetCharacterIndexAtPoint() can return NSNotFound, which is UINT32_MAX.)
TYPED_TEST_P(TextInputClientMacTest, SyncGetter_NotFound) {
  using Traits = TestTraits<TypeParam>;

  // Set an arbitrary value to ensure the response doesn't just default to the
  // timeout value.
  const TypeParam kPreviousValue = Traits::CreateResponse(42);
  this->request_delegate().AddResponse(kPreviousValue, kTaskDelay);

  // Set the response to the timeout value after the previous setting.
  this->request_delegate().AddResponse(Traits::kTimeoutResponse, kTaskDelay);

  this->FocusWebContentsOnMainFrame();
  EXPECT_EQ(Traits::TextInputClientGetSync(this->widget()), kPreviousValue);
  EXPECT_EQ(Traits::TextInputClientGetSync(this->widget()),
            Traits::kTimeoutResponse);
}

// Tests that TextInputClient doesn't get confused if TextInputHost sends a
// response before the calling thread blocks.
TYPED_TEST_P(TextInputClientMacTest, SyncGetter_Immediate) {
  using Traits = TestTraits<TypeParam>;

  // A response with 0 delay is sent immediately, not posted.
  const TypeParam kSuccessValue = Traits::CreateResponse(42);
  this->request_delegate().AddResponse(kSuccessValue, base::TimeDelta());

  this->FocusWebContentsOnMainFrame();
  EXPECT_EQ(Traits::TextInputClientGetSync(this->widget()), kSuccessValue);
}

REGISTER_TYPED_TEST_SUITE_P(TextInputClientMacTest,
                            SyncGetter_NoFocus,
                            SyncGetter_Basic,
                            SyncGetter_Timeout,
                            SyncGetter_NotFound,
                            SyncGetter_Immediate);

using ResponseTypes = ::testing::Types<uint32_t, gfx::Rect>;
INSTANTIATE_TYPED_TEST_SUITE_P(ResponseType,
                               TextInputClientMacTest,
                               ResponseTypes);

}  // namespace content
