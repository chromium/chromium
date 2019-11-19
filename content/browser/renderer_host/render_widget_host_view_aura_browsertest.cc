// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_aura.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/renderer_host/delegated_frame_host.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"

namespace content {
namespace {

#if defined(OS_CHROMEOS)
const char kMinimalPageDataURL[] =
    "data:text/html,<html><head></head><body>Hello, world</body></html>";

// Run the current message loop for a short time without unwinding the current
// call stack.
void GiveItSomeTime() {
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(),
      base::TimeDelta::FromMilliseconds(250));
  run_loop.Run();
}
#endif  // defined(OS_CHROMEOS)

class FakeWebContentsDelegate : public WebContentsDelegate {
 public:
  FakeWebContentsDelegate() = default;
  ~FakeWebContentsDelegate() override = default;

  void SetShowStaleContentOnEviction(bool value) {
    show_stale_content_on_eviction_ = value;
  }

  bool ShouldShowStaleContentOnEviction(WebContents* source) override {
    return show_stale_content_on_eviction_;
  }

 private:
  bool show_stale_content_on_eviction_ = false;

  DISALLOW_COPY_AND_ASSIGN(FakeWebContentsDelegate);
};

}  // namespace

class RenderWidgetHostViewAuraBrowserTest : public ContentBrowserTest {
 public:
  RenderViewHost* GetRenderViewHost() const {
    RenderViewHost* const rvh = shell()->web_contents()->GetRenderViewHost();
    CHECK(rvh);
    return rvh;
  }

  RenderWidgetHostViewAura* GetRenderWidgetHostView() const {
    return static_cast<RenderWidgetHostViewAura*>(
        GetRenderViewHost()->GetWidget()->GetView());
  }

  DelegatedFrameHost* GetDelegatedFrameHost() const {
    return GetRenderWidgetHostView()->delegated_frame_host_.get();
  }
};

#if defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewAuraBrowserTest,
                       StaleFrameContentOnEvictionNormal) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kMinimalPageDataURL)));

  // Make sure the renderer submits at least one frame before hiding it.
  RenderFrameSubmissionObserver submission_observer(shell()->web_contents());
  if (!submission_observer.render_frame_count())
    submission_observer.WaitForAnyFrameSubmission();

  FakeWebContentsDelegate delegate;
  delegate.SetShowStaleContentOnEviction(true);
  shell()->web_contents()->SetDelegate(&delegate);

  // Initially there should be no stale content set.
  EXPECT_FALSE(
      GetDelegatedFrameHost()->stale_content_layer_->has_external_content());
  EXPECT_EQ(GetDelegatedFrameHost()->frame_eviction_state_,
            DelegatedFrameHost::FrameEvictionState::kNotStarted);

  // Hide the view and evict the frame. This should trigger a copy of the stale
  // frame content.
  GetRenderWidgetHostView()->Hide();
  static_cast<viz::FrameEvictorClient*>(GetDelegatedFrameHost())
      ->EvictDelegatedFrame();
  EXPECT_EQ(GetDelegatedFrameHost()->frame_eviction_state_,
            DelegatedFrameHost::FrameEvictionState::kPendingEvictionRequests);

  // Wait until the stale frame content is copied and set onto the layer.
  while (!GetDelegatedFrameHost()->stale_content_layer_->has_external_content())
    GiveItSomeTime();

  EXPECT_EQ(GetDelegatedFrameHost()->frame_eviction_state_,
            DelegatedFrameHost::FrameEvictionState::kNotStarted);

  // Unhidding the view should reset the stale content layer to show the new
  // frame content.
  GetRenderWidgetHostView()->Show();
  EXPECT_FALSE(
      GetDelegatedFrameHost()->stale_content_layer_->has_external_content());
}

IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewAuraBrowserTest,
                       StaleFrameContentOnEvictionRejected) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kMinimalPageDataURL)));

  // Wait for first frame activation when a surface is embedded.
  while (!GetDelegatedFrameHost()->HasSavedFrame())
    GiveItSomeTime();

  FakeWebContentsDelegate delegate;
  delegate.SetShowStaleContentOnEviction(true);
  shell()->web_contents()->SetDelegate(&delegate);

  // Initially there should be no stale content set.
  EXPECT_FALSE(
      GetDelegatedFrameHost()->stale_content_layer_->has_external_content());
  EXPECT_EQ(GetDelegatedFrameHost()->frame_eviction_state_,
            DelegatedFrameHost::FrameEvictionState::kNotStarted);

  // Hide the view and evict the frame. This should trigger a copy of the stale
  // frame content.
  GetRenderWidgetHostView()->Hide();
  static_cast<viz::FrameEvictorClient*>(GetDelegatedFrameHost())
      ->EvictDelegatedFrame();
  EXPECT_EQ(GetDelegatedFrameHost()->frame_eviction_state_,
            DelegatedFrameHost::FrameEvictionState::kPendingEvictionRequests);

  GetRenderWidgetHostView()->Show();
  EXPECT_EQ(GetDelegatedFrameHost()->frame_eviction_state_,
            DelegatedFrameHost::FrameEvictionState::kNotStarted);

  // Wait until the stale frame content is copied and the result callback is
  // complete.
  GiveItSomeTime();

  // This should however not set the stale content as the view is visible and
  // new frames are being submitted.
  EXPECT_FALSE(
      GetDelegatedFrameHost()->stale_content_layer_->has_external_content());
}

IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewAuraBrowserTest,
                       StaleFrameContentOnEvictionNone) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kMinimalPageDataURL)));

  // Wait for first frame activation when a surface is embedded.
  while (!GetDelegatedFrameHost()->HasSavedFrame())
    GiveItSomeTime();

  FakeWebContentsDelegate delegate;
  delegate.SetShowStaleContentOnEviction(false);
  shell()->web_contents()->SetDelegate(&delegate);

  // Initially there should be no stale content set.
  EXPECT_FALSE(
      GetDelegatedFrameHost()->stale_content_layer_->has_external_content());
  EXPECT_EQ(GetDelegatedFrameHost()->frame_eviction_state_,
            DelegatedFrameHost::FrameEvictionState::kNotStarted);

  // Hide the view and evict the frame. This should not trigger a copy of the
  // stale frame content as the WebContentDelegate returns false.
  GetRenderWidgetHostView()->Hide();
  static_cast<viz::FrameEvictorClient*>(GetDelegatedFrameHost())
      ->EvictDelegatedFrame();

  EXPECT_EQ(GetDelegatedFrameHost()->frame_eviction_state_,
            DelegatedFrameHost::FrameEvictionState::kNotStarted);

  // Wait for a while to ensure any copy requests that were sent out are not
  // completed. There shouldnt be any requests sent however.
  GiveItSomeTime();
  EXPECT_FALSE(
      GetDelegatedFrameHost()->stale_content_layer_->has_external_content());
}
#endif  // #if defined(OS_CHROMEOS)

}  // namespace content
