// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/screen_orientation/screen_orientation_provider.h"

#include "base/bind.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "content/common/frame_messages.h"
#include "content/public/browser/screen_orientation_delegate.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "third_party/blink/public/common/screen_orientation/web_screen_orientation_lock_type.h"
#include "third_party/blink/public/mojom/frame/fullscreen.mojom.h"

namespace content {

using device::mojom::ScreenOrientationLockResult;

namespace {

class FakeScreenOrientationDelegate : public ScreenOrientationDelegate {
 public:
  FakeScreenOrientationDelegate(bool supported, bool full_screen_required)
      : supported_(supported), full_screen_required_(full_screen_required) {
    ScreenOrientationProvider::SetDelegate(this);
  }

  ~FakeScreenOrientationDelegate() override = default;

  bool FullScreenRequired(WebContents* web_contents) override {
    return full_screen_required_;
  }

  bool ScreenOrientationProviderSupported() override { return supported_; }

  void Lock(WebContents* web_contents,
            blink::WebScreenOrientationLockType lock_orientation) override {
    lock_count_++;
  }

  void Unlock(WebContents* web_contents) override { unlock_count_++; }

  int lock_count() const { return lock_count_; }
  int unlock_count() const { return unlock_count_; }

 private:
  bool supported_ = true;
  bool full_screen_required_ = false;

  int lock_count_ = 0;
  int unlock_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(FakeScreenOrientationDelegate);
};

class FakeWebContentsDelegate : public WebContentsDelegate {
 public:
  FakeWebContentsDelegate() = default;
  ~FakeWebContentsDelegate() override = default;

  void EnterFullscreenModeForTab(
      WebContents* web_contents,
      const GURL& origin,
      const blink::mojom::FullscreenOptions& options) override {
    fullscreened_contents_ = web_contents;
  }

  void ExitFullscreenModeForTab(WebContents* web_contents) override {
    fullscreened_contents_ = nullptr;
  }

  bool IsFullscreenForTabOrPending(const WebContents* web_contents) override {
    return fullscreened_contents_ && web_contents == fullscreened_contents_;
  }

 private:
  WebContents* fullscreened_contents_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(FakeWebContentsDelegate);
};

void LockResultCallback(base::Optional<ScreenOrientationLockResult>* out_result,
                        ScreenOrientationLockResult result) {
  *out_result = result;
}

}  // namespace

class ScreenOrientationProviderTest : public RenderViewHostImplTestHarness {
 public:
  ScreenOrientationProviderTest() = default;

  void SetUp() override {
    content::RenderViewHostImplTestHarness::SetUp();

    contents()->SetDelegate(&wc_delegate_);
  }

  // Helpers for testing ScreenOrientationProvider methods.
  void CallLockAndGetResult(
      blink::WebScreenOrientationLockType orientation,
      base::Optional<ScreenOrientationLockResult>* out_result) {
    contents()->GetScreenOrientationProviderForTesting()->LockOrientation(
        orientation, base::BindOnce(&LockResultCallback, out_result));

    base::RunLoop().RunUntilIdle();
  }

  void CallUnlock() {
    contents()->GetScreenOrientationProviderForTesting()->UnlockOrientation();
  }

 private:
  FakeWebContentsDelegate wc_delegate_;
};

// Lock operation is not available.
TEST_F(ScreenOrientationProviderTest, DelegateNotAvailableLockOnce) {
  // No ScreenOrientationDelegate.
  base::Optional<ScreenOrientationLockResult> result_1;
  CallLockAndGetResult(blink::WebScreenOrientationLockType::
                           kWebScreenOrientationLockLandscapeSecondary,
                       &result_1);
  EXPECT_EQ(ScreenOrientationLockResult::
                SCREEN_ORIENTATION_LOCK_RESULT_ERROR_NOT_AVAILABLE,
            *result_1);

  // ScreenOrientationDelegate not supported.
  FakeScreenOrientationDelegate delegate(false, false);
  base::Optional<ScreenOrientationLockResult> result_2;
  CallLockAndGetResult(blink::WebScreenOrientationLockType::
                           kWebScreenOrientationLockLandscapeSecondary,
                       &result_2);
  EXPECT_EQ(ScreenOrientationLockResult::
                SCREEN_ORIENTATION_LOCK_RESULT_ERROR_NOT_AVAILABLE,
            *result_2);
}

// Full screen is not required by delegate, normally lock once.
TEST_F(ScreenOrientationProviderTest, DelegateLockOnce) {
  // ScreenOrientationDelegate does not require full screen.
  FakeScreenOrientationDelegate delegate(true, false);

  // Navigate to a site.
  const GURL url("http://www.google.com");
  controller().LoadURL(url, Referrer(), ui::PAGE_TRANSITION_TYPED,
                       std::string());

  base::Optional<ScreenOrientationLockResult> result_1;
  CallLockAndGetResult(blink::WebScreenOrientationLockType::
                           kWebScreenOrientationLockLandscapeSecondary,
                       &result_1);
  // Lock request is pending.
  EXPECT_FALSE(result_1.has_value());
  // Delegate did apply lock once.
  EXPECT_EQ(1, delegate.lock_count());
}

// Full screen is required by delegate.
TEST_F(ScreenOrientationProviderTest, DelegateRequireFullScreenLockOnce) {
  // ScreenOrientationDelegate requires full screen.
  FakeScreenOrientationDelegate delegate(true, true);

  // Navigate to a site.
  const GURL url("http://www.google.com");
  controller().LoadURL(url, Referrer(), ui::PAGE_TRANSITION_TYPED,
                       std::string());

  // Current web contents is not in full screen.
  ASSERT_FALSE(contents()->IsFullscreenForCurrentTab());
  base::Optional<ScreenOrientationLockResult> result_1;
  CallLockAndGetResult(blink::WebScreenOrientationLockType::
                           kWebScreenOrientationLockLandscapeSecondary,
                       &result_1);
  EXPECT_EQ(ScreenOrientationLockResult::
                SCREEN_ORIENTATION_LOCK_RESULT_ERROR_FULLSCREEN_REQUIRED,
            *result_1);
  // Delegate did not apply any lock.
  EXPECT_EQ(0, delegate.lock_count());

  // Simulates entering full screen.
  main_test_rfh()->EnterFullscreen(blink::mojom::FullscreenOptions::New());
  ASSERT_TRUE(contents()->IsFullscreenForCurrentTab());

  base::Optional<ScreenOrientationLockResult> result_2;
  CallLockAndGetResult(blink::WebScreenOrientationLockType::
                           kWebScreenOrientationLockLandscapeSecondary,
                       &result_2);
  // Lock request is pending.
  EXPECT_FALSE(result_2.has_value());
  // Delegate did apply lock once.
  EXPECT_EQ(1, delegate.lock_count());
}

// Lock once, then unlock once, the pending lock request will be cancelled.
TEST_F(ScreenOrientationProviderTest, DelegateLockThenUnlock) {
  FakeScreenOrientationDelegate delegate(true, false);

  // Navigate to a site.
  const GURL url("http://www.google.com");
  controller().LoadURL(url, Referrer(), ui::PAGE_TRANSITION_TYPED,
                       std::string());

  base::Optional<ScreenOrientationLockResult> result_1;
  CallLockAndGetResult(blink::WebScreenOrientationLockType::
                           kWebScreenOrientationLockLandscapeSecondary,
                       &result_1);
  // The lock request will be pending.
  EXPECT_FALSE(result_1.has_value());
  // Delegate did apply lock once.
  EXPECT_EQ(1, delegate.lock_count());
  EXPECT_EQ(0, delegate.unlock_count());

  CallUnlock();
  // The pending lock request is cancelled.
  EXPECT_EQ(ScreenOrientationLockResult::
                SCREEN_ORIENTATION_LOCK_RESULT_ERROR_CANCELED,
            result_1);
  // Delegate did apply unlock once.
  EXPECT_EQ(1, delegate.unlock_count());
}

// Lock twice, the first lock request will be cancelled by the second one.
TEST_F(ScreenOrientationProviderTest, DelegateLockThenLock) {
  FakeScreenOrientationDelegate delegate(true, false);

  // Navigate to a site.
  const GURL url("http://www.google.com");
  controller().LoadURL(url, Referrer(), ui::PAGE_TRANSITION_TYPED,
                       std::string());

  base::Optional<ScreenOrientationLockResult> result_1;
  CallLockAndGetResult(blink::WebScreenOrientationLockType::
                           kWebScreenOrientationLockLandscapeSecondary,
                       &result_1);
  // The lock request will be pending.
  EXPECT_FALSE(result_1.has_value());
  // Delegate did apply lock once.
  EXPECT_EQ(1, delegate.lock_count());
  EXPECT_EQ(0, delegate.unlock_count());

  base::Optional<ScreenOrientationLockResult> result_2;
  CallLockAndGetResult(blink::WebScreenOrientationLockType::
                           kWebScreenOrientationLockLandscapeSecondary,
                       &result_2);
  // The pending lock request is cancelled.
  EXPECT_EQ(ScreenOrientationLockResult::
                SCREEN_ORIENTATION_LOCK_RESULT_ERROR_CANCELED,
            result_1);
  // The second one became pending.
  EXPECT_FALSE(result_2.has_value());
  // Delegate did apply lock once more.
  EXPECT_EQ(2, delegate.lock_count());
  EXPECT_EQ(0, delegate.unlock_count());
}

// Unlock won't be applied if no lock has been applied previously.
TEST_F(ScreenOrientationProviderTest, NoUnlockWithoutLock) {
  FakeScreenOrientationDelegate delegate(true, false);

  // Navigate to a site.
  const GURL url("http://www.google.com");
  controller().LoadURL(url, Referrer(), ui::PAGE_TRANSITION_TYPED,
                       std::string());

  CallUnlock();
  // Delegate did not apply any unlock.
  EXPECT_EQ(0, delegate.unlock_count());
}

// If lock already applied once in full screen, then unlock should be triggered
// once automatically when exiting full screen, and previous pending lock
// request will be cancelled.
TEST_F(ScreenOrientationProviderTest, UnlockWhenExitingFullScreen) {
  // ScreenOrientationDelegate requires full screen.
  FakeScreenOrientationDelegate delegate(true, true);

  // Navigate to a site.
  const GURL url("http://www.google.com");
  controller().LoadURL(url, Referrer(), ui::PAGE_TRANSITION_TYPED,
                       std::string());

  // Simulates entering full screen.
  main_test_rfh()->EnterFullscreen(blink::mojom::FullscreenOptions::New());
  ASSERT_TRUE(contents()->IsFullscreenForCurrentTab());

  base::Optional<ScreenOrientationLockResult> result;
  CallLockAndGetResult(blink::WebScreenOrientationLockType::
                           kWebScreenOrientationLockLandscapeSecondary,
                       &result);
  // The lock request will be pending.
  EXPECT_FALSE(result.has_value());
  // Delegate did apply lock once.
  EXPECT_EQ(1, delegate.lock_count());
  EXPECT_EQ(0, delegate.unlock_count());

  // Simulates exiting full screen.
  main_test_rfh()->ExitFullscreen();
  ASSERT_FALSE(contents()->IsFullscreenForCurrentTab());
  // The pending lock request is cancelled.
  EXPECT_EQ(ScreenOrientationLockResult::
                SCREEN_ORIENTATION_LOCK_RESULT_ERROR_CANCELED,
            result);
  // Delegate did apply unlock once.
  EXPECT_EQ(1, delegate.unlock_count());
}

// If lock already applied once, then unlock should be triggered once
// automatically when navigating to other web page, and previous pending lock
// request will be cancelled.
TEST_F(ScreenOrientationProviderTest, UnlockWhenNavigation) {
  FakeScreenOrientationDelegate delegate(true, false);

  // Navigate to a site.
  const GURL url("http://www.google.com");
  controller().LoadURL(url, Referrer(), ui::PAGE_TRANSITION_TYPED,
                       std::string());

  base::Optional<ScreenOrientationLockResult> result;
  CallLockAndGetResult(blink::WebScreenOrientationLockType::
                           kWebScreenOrientationLockLandscapeSecondary,
                       &result);
  // The lock request will be pending.
  EXPECT_FALSE(result.has_value());
  // Delegate did apply lock once.
  EXPECT_EQ(1, delegate.lock_count());
  EXPECT_EQ(0, delegate.unlock_count());

  // Navigate to another site.
  const GURL another_url("http://www.google.com/abc.html");
  contents()->NavigateAndCommit(another_url);
  // The pending lock request is cancelled.
  EXPECT_EQ(ScreenOrientationLockResult::
                SCREEN_ORIENTATION_LOCK_RESULT_ERROR_CANCELED,
            result);
  // Delegate did apply unlock once.
  EXPECT_EQ(1, delegate.unlock_count());
}

}  // namespace content
