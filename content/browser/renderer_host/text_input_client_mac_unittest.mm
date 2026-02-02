// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/browser/renderer_host/text_input_client_mac.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
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

// Fake that replaces mojo messages sent through LocalFrame by posting tasks
// directly to TextInputHostImpl on the IO thread.
//
// The standard way to implement the receiver of LocalFrame messages in unit
// tests is FakeLocalFrame, but its recievers are bound to the main test thread,
// not the IO thread. Blocking TextInputClientMac methods are also called on the
// main thread, and wait for the responses to those messages, so receivers bound
// to FakeLocalFrame won't get called until after the blocking method times out.
class FakeAsyncRequestDelegate final
    : public TextInputClientMac::AsyncRequestDelegate {
 public:
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

  void AddCharacterIndexResponse(uint32_t index, base::TimeDelta delay) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    index_responses_.emplace(index, delay);
  }

  void AddFirstRectResponse(gfx::Rect rect, base::TimeDelta delay) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    rect_responses_.emplace(std::move(rect), delay);
  }

  // AsyncRequestDelegate:

  void GetCharacterIndexAtPoint(RenderFrameHost* rfh,
                                const gfx::Point& point) final {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    ASSERT_TRUE(rfh);
    ASSERT_EQ(rfh->GetRenderWidgetHost(), widget_);
    if (index_responses_.empty()) {
      return;
    }
    auto [index, delay] = index_responses_.front();
    index_responses_.pop();

    // Unretained is safe since `host_impl_` is deleted on the IO thread.
    GetIOThreadTaskRunner()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&TextInputHostImpl::GotCharacterIndexAtPoint,
                       base::Unretained(host_impl_.get()), index),
        delay);
  }

  void GetFirstRectForRange(RenderFrameHost* rfh,
                            const gfx::Range& range) final {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    ASSERT_TRUE(rfh);
    ASSERT_EQ(rfh->GetRenderWidgetHost(), widget_);
    if (rect_responses_.empty()) {
      return;
    }
    auto [rect, delay] = rect_responses_.front();
    rect_responses_.pop();

    // Unretained is safe since `host_impl_` is deleted on the IO thread.
    GetIOThreadTaskRunner()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&TextInputHostImpl::GotFirstRectForRange,
                       base::Unretained(host_impl_.get()), std::move(rect)),
        delay);
  }

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  raw_ptr<RenderWidgetHost> widget_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Responses to the next GetCharacterIndexAtPoint calls, with delay.
  base::queue<std::pair<uint32_t, base::TimeDelta>> index_responses_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Responses to the next GetFirstRectForRange calls, with delay.
  base::queue<std::pair<gfx::Rect, base::TimeDelta>> rect_responses_
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
class TextInputClientMacTest : public content::RenderViewHostTestHarness {
 public:
  TextInputClientMacTest()
      : RenderViewHostTestHarness(BrowserTaskEnvironment::REAL_IO_THREAD) {}

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    RenderViewHostTester::For(rvh())->CreateTestRenderView();

    auto delegate = std::make_unique<FakeAsyncRequestDelegate>(widget());
    request_delegate_ = delegate.get();
    TextInputClientMac::GetInstance()->SetAsyncRequestDelegateForTesting(
        std::move(delegate));

    FocusWebContentsOnMainFrame();
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

  FakeAsyncRequestDelegate& request_delegate() { return *request_delegate_; }

 private:
  raw_ptr<FakeAsyncRequestDelegate> request_delegate_;
};

}  // namespace

// Test Cases //////////////////////////////////////////////////////////////////

TEST_F(TextInputClientMacTest, GetCharacterIndex) {
  const NSUInteger kSuccessValue = 42;

  request_delegate().AddCharacterIndexResponse(kSuccessValue, kTaskDelay);

  NSUInteger index =
      TextInputClientMac::GetInstance()->GetCharacterIndexAtPoint(
          widget(), gfx::Point(2, 2));

  EXPECT_EQ(kSuccessValue, index);
}

TEST_F(TextInputClientMacTest, TimeoutCharacterIndex) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(features::kTextInputClient,
                                                  {{"ipc_timeout", "300ms"}});

  uint32_t index = TextInputClientMac::GetInstance()->GetCharacterIndexAtPoint(
      widget(), gfx::Point(2, 2));

  EXPECT_EQ(UINT32_MAX, index);
}

TEST_F(TextInputClientMacTest, NotFoundCharacterIndex) {
  const NSUInteger kPreviousValue = 42;

  // Set an arbitrary value to ensure the index is not |NSNotFound|.
  request_delegate().AddCharacterIndexResponse(kPreviousValue, kTaskDelay);

  // Set UINT32_MAX to the index |kTaskDelay| msec after the previous setting.
  request_delegate().AddCharacterIndexResponse(UINT32_MAX, kTaskDelay);

  uint32_t index = TextInputClientMac::GetInstance()->GetCharacterIndexAtPoint(
      widget(), gfx::Point(2, 2));
  EXPECT_EQ(kPreviousValue, index);

  index = TextInputClientMac::GetInstance()->GetCharacterIndexAtPoint(
      widget(), gfx::Point(2, 2));
  EXPECT_EQ(UINT32_MAX, index);
}

TEST_F(TextInputClientMacTest, GetRectForRange) {
  const gfx::Rect kSuccessValue(42, 43, 44, 45);

  request_delegate().AddFirstRectResponse(kSuccessValue, kTaskDelay);

  gfx::Rect rect = TextInputClientMac::GetInstance()->GetFirstRectForRange(
      widget(), gfx::Range(NSMakeRange(0, 32)));
  EXPECT_EQ(kSuccessValue, rect);
}

TEST_F(TextInputClientMacTest, TimeoutRectForRange) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(features::kTextInputClient,
                                                  {{"ipc_timeout", "300ms"}});

  gfx::Rect rect = TextInputClientMac::GetInstance()->GetFirstRectForRange(
      widget(), gfx::Range(NSMakeRange(0, 32)));
  EXPECT_EQ(gfx::Rect(), rect);
}

}  // namespace content
