// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include <stdint.h>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/current_thread.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "cc/slim/features.h"
#include "cc/slim/layer_tree.h"
#include "cc/slim/surface_layer.h"
#include "components/viz/common/features.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/renderer_host/browser_compositor_ios.h"
#include "content/browser/renderer_host/dip_util.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/renderer_host/test_render_widget_host_view_ios_factory.h"
#include "content/browser/renderer_host/visible_time_request_trigger.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/content_navigation_policy.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/slow_http_response.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/did_commit_navigation_interceptor.h"
#include "content/test/render_document_feature.h"
#include "net/base/filename_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/page/content_to_visible_time_reporter.h"
#include "third_party/blink/public/mojom/page/page_visibility_state.mojom-shared.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/display/display_switches.h"
#include "ui/gfx/geometry/size_conversions.h"

#if defined(USE_AURA)
#include "content/browser/renderer_host/delegated_frame_host.h"
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "content/browser/renderer_host/compositor_impl_android.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "ui/android/delegated_frame_host_android.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "content/browser/renderer_host/browser_compositor_view_mac.h"
#include "content/browser/renderer_host/delegated_frame_host.h"
#include "content/browser/renderer_host/test_render_widget_host_view_mac_factory.h"
#include "content/public/browser/context_factory.h"
#include "third_party/blink/public/common/page/content_to_visible_time_reporter.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/recyclable_compositor_mac.h"
#endif

namespace content {

namespace {

// Convenience macro: Short-circuit a pass for the tests where platform support
// for forced-compositing mode (or disabled-compositing mode) is lacking.
#define SET_UP_SURFACE_OR_PASS_TEST(wait_message)  \
  if (!SetUpSourceSurface(wait_message)) {  \
    LOG(WARNING)  \
        << ("Blindly passing this test: This platform does not support "  \
            "forced compositing (or forced-disabled compositing) mode.");  \
    return;  \
  }

}  // namespace

// Common base class for browser tests.  This is subclassed three times: Once to
// test the browser in forced-compositing mode; once to test with compositing
// mode disabled; once with no surface creation for non-visual tests.
class RenderWidgetHostViewBrowserTest : public ContentBrowserTest {
 public:
  RenderWidgetHostViewBrowserTest()
      : frame_size_(400, 300),
        callback_invoke_count_(0),
        frames_captured_(0) {}

  void SetUpOnMainThread() override {
    ASSERT_TRUE(base::PathService::Get(DIR_TEST_DATA, &test_dir_));
  }

  // Attempts to set up the source surface.  Returns false if unsupported on the
  // current platform.
  virtual bool SetUpSourceSurface(const char* wait_message) = 0;

  int callback_invoke_count() const {
    return callback_invoke_count_;
  }

  int frames_captured() const {
    return frames_captured_;
  }

  const gfx::Size& frame_size() const {
    return frame_size_;
  }

  const base::FilePath& test_dir() const {
    return test_dir_;
  }

  RenderViewHost* GetRenderViewHost() const {
    RenderViewHost* const rvh =
        shell()->web_contents()->GetPrimaryMainFrame()->GetRenderViewHost();
    CHECK(rvh);
    return rvh;
  }

  RenderWidgetHostImpl* GetRenderWidgetHost() const {
    RenderWidgetHostImpl* const rwh = RenderWidgetHostImpl::From(
        shell()->web_contents()->GetRenderWidgetHostView()->
            GetRenderWidgetHost());
    CHECK(rwh);
    return rwh;
  }

  RenderWidgetHostViewBase* GetRenderWidgetHostView() const {
    return static_cast<RenderWidgetHostViewBase*>(
        GetRenderViewHost()->GetWidget()->GetView());
  }

  // Callback when using CopyFromSurface() API.
  void FinishCopyFromSurface(base::OnceClosure quit_closure,
                             const SkBitmap& bitmap) {
    ++callback_invoke_count_;
    if (!bitmap.drawsNothing())
      ++frames_captured_;
    std::move(quit_closure).Run();
  }

 protected:
  // Waits until the source is available for copying.
  void WaitForCopySourceReady() {
    while (!GetRenderWidgetHostView()->IsSurfaceAvailableForCopy())
      GiveItSomeTime();
  }

  // Run the current message loop for a short time without unwinding the current
  // call stack.
  static void GiveItSomeTime() {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(250));
    run_loop.Run();
  }

 private:
  const gfx::Size frame_size_;
  base::FilePath test_dir_;
  int callback_invoke_count_;
  int frames_captured_;
};

// Helps to ensure that a navigation is committed after a compositor frame was
// submitted by the renderer, but before corresponding ACK is sent back.
class CommitBeforeSwapAckSentHelper : public DidCommitNavigationInterceptor {
 public:
  explicit CommitBeforeSwapAckSentHelper(
      WebContents* web_contents,
      RenderFrameSubmissionObserver* frame_observer)
      : DidCommitNavigationInterceptor(web_contents),
        frame_observer_(frame_observer) {}

  CommitBeforeSwapAckSentHelper(const CommitBeforeSwapAckSentHelper&) = delete;
  CommitBeforeSwapAckSentHelper& operator=(
      const CommitBeforeSwapAckSentHelper&) = delete;

 private:
  // DidCommitNavigationInterceptor:
  bool WillProcessDidCommitNavigation(
      RenderFrameHost* render_frame_host,
      NavigationRequest* navigation_request,
      mojom::DidCommitProvisionalLoadParamsPtr* params,
      mojom::DidCommitProvisionalLoadInterfaceParamsPtr* interface_params)
      override {
    frame_observer_->WaitForAnyFrameSubmission();
    return true;
  }

  // Not owned.
  const raw_ptr<RenderFrameSubmissionObserver> frame_observer_;
};

class RenderWidgetHostViewBrowserTestBase : public ContentBrowserTest {
 public:
  ~RenderWidgetHostViewBrowserTestBase() override {}

 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
  }
};

// Base class for testing a RenderWidgetHostViewBase where visual output is not
// relevant. This class does not setup surfaces for compositing.
class NoCompositingRenderWidgetHostViewBrowserTest
    : public RenderWidgetHostViewBrowserTest {
 public:
  NoCompositingRenderWidgetHostViewBrowserTest() {}

  NoCompositingRenderWidgetHostViewBrowserTest(
      const NoCompositingRenderWidgetHostViewBrowserTest&) = delete;
  NoCompositingRenderWidgetHostViewBrowserTest& operator=(
      const NoCompositingRenderWidgetHostViewBrowserTest&) = delete;

  ~NoCompositingRenderWidgetHostViewBrowserTest() override {}

  bool SetUpSourceSurface(const char* wait_message) override {
    NOTIMPLEMENTED();
    return true;
  }
};

// Ensures that kBackForwardCache is always enabled to ensure that a new RWH is
// created on navigation.
class PaintHoldingRenderWidgetHostViewBrowserTest
    : public NoCompositingRenderWidgetHostViewBrowserTest {
 public:
  PaintHoldingRenderWidgetHostViewBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        GetBasicBackForwardCacheFeatureForTesting(),
        GetDefaultDisabledBackForwardCacheFeaturesForTesting());
  }

  ~PaintHoldingRenderWidgetHostViewBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// When creating the first RenderWidgetHostViewBase, the CompositorFrameSink can
// change. When this occurs we need to evict the current frame, and recreate
// surfaces. This tests that when frame eviction occurs while the
// RenderWidgetHostViewBase is visible, that we generate a new LocalSurfaceId.
// Simply invalidating can lead to displaying blank screens.
// (https://crbug.com/909903)
IN_PROC_BROWSER_TEST_F(NoCompositingRenderWidgetHostViewBrowserTest,
                       ValidLocalSurfaceIdAfterInitialNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  // Creates the initial RenderWidgetHostViewBase, and connects to a
  // CompositorFrameSink. This will trigger frame eviction.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/page_with_animation.html")));
  RenderWidgetHostViewBase* rwhvb = GetRenderWidgetHostView();
  // Eviction normally invalidates the LocalSurfaceId, however if the
  // RenderWidgetHostViewBase is visible, a new id must be allocated. Otherwise
  // blank content is shown.
  EXPECT_TRUE(rwhvb);
  // Mac does not initialize RenderWidgetHostViewBase as visible.
#if !BUILDFLAG(IS_MAC)
  EXPECT_TRUE(rwhvb->IsShowing());
#endif
  EXPECT_TRUE(rwhvb->GetLocalSurfaceId().is_valid());
  // TODO(jonross): Unify FrameEvictor into RenderWidgetHostViewBase so that we
  // can generically test all eviction paths. However this should only be for
  // top level renderers. Currently the FrameEvict implementations are platform
  // dependent so we can't have a single generic test.
}

// Tests that when navigating to a new page the old page content continues to be
// shown until the new page content is ready or content rendering timeout fires.
IN_PROC_BROWSER_TEST_F(PaintHoldingRenderWidgetHostViewBrowserTest,
                       PaintHoldingOnNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  // Creates the initial RenderWidgetHostViewBase, and connects to a
  // CompositorFrameSink.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/page_with_animation.html")));

  RenderWidgetHostViewBase* first_view = GetRenderWidgetHostView();
  EXPECT_TRUE(first_view);
  viz::SurfaceId first_surface_id = first_view->GetCurrentSurfaceId();
  EXPECT_TRUE(first_surface_id.is_valid());

  // Perform a navigation to a new page. This will use a new render widget when
  // BackForwardCache is enabled.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/page_with_blur.html")));

  RenderWidgetHostViewBase* second_view = GetRenderWidgetHostView();
  EXPECT_TRUE(second_view);
  viz::SurfaceId second_surface_id = second_view->GetCurrentSurfaceId();
  EXPECT_TRUE(second_surface_id.is_valid());

  // After navigation there should be a new view with a different FrameSinkId.
  EXPECT_NE(first_view, second_view);
  EXPECT_NE(first_surface_id.frame_sink_id(),
            second_surface_id.frame_sink_id());

#if defined(USE_AURA)
  DelegatedFrameHost* dfh = static_cast<RenderWidgetHostViewAura*>(second_view)
                                ->GetDelegatedFrameHost();
  EXPECT_TRUE(dfh->HasPrimarySurface());
  EXPECT_TRUE(dfh->HasFallbackSurface());

  // The view after navigation should have a fallback SurfaceId that corresponds
  // to the SurfaceId from before navigation. This shows the old content after
  // navigation until either new content is ready or content rending timeout
  // fires.
  viz::SurfaceId fallback_surface_id = dfh->GetFallbackSurfaceIdForTesting();
  EXPECT_TRUE(first_surface_id.IsSameOrNewerThan(fallback_surface_id));
  EXPECT_NE(fallback_surface_id.frame_sink_id(),
            second_surface_id.frame_sink_id());
#endif

  // The render widget should have it's content rendering timeout timer after
  // navigating to the new page so the fallback content is eventually cleared.
  EXPECT_TRUE(GetRenderWidgetHost()->IsContentRenderingTimeoutRunning());
}

// TODO(jonross): Update Mac to also invalidate its viz::LocalSurfaceIds when
// performing navigations while hidden. https://crbug.com/935364
#if !BUILDFLAG(IS_MAC)
// When a navigation occurs while the RenderWidgetHostViewBase is hidden, it
// should invalidate it's viz::LocalSurfaceId. When subsequently being shown,
// a new surface should be generated with a new viz::LocalSurfaceId
IN_PROC_BROWSER_TEST_F(NoCompositingRenderWidgetHostViewBrowserTest,
                       ValidLocalSurfaceIdAfterHiddenNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  // Creates the initial RenderWidgetHostViewBase, and connects to a
  // CompositorFrameSink.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/page_with_animation.html")));
  RenderWidgetHostViewBase* rwhvb = GetRenderWidgetHostView();
  EXPECT_TRUE(rwhvb);
  viz::LocalSurfaceId rwhvb_local_surface_id = rwhvb->GetLocalSurfaceId();
  EXPECT_TRUE(rwhvb_local_surface_id.is_valid());

  // Hide the view before performing the next navigation.
  shell()->web_contents()->WasHidden();
#if BUILDFLAG(IS_ANDROID)
  // On Android we want to ensure that we maintain the currently embedded
  // surface. So that there is something to display when returning to the tab.
  RenderWidgetHostViewAndroid* rwhva =
      static_cast<RenderWidgetHostViewAndroid*>(rwhvb);
  ui::DelegatedFrameHostAndroid* dfh =
      rwhva->delegated_frame_host_for_testing();
  EXPECT_TRUE(dfh->HasPrimarySurface());
  EXPECT_FALSE(dfh->IsPrimarySurfaceEvicted());
  viz::LocalSurfaceId initial_local_surface_id =
      dfh->SurfaceId().local_surface_id();
  EXPECT_TRUE(initial_local_surface_id.is_valid());
#endif

  // Perform a navigation to the same content source. This will reuse the
  // existing RenderWidgetHostViewBase, except if we trigger a RenderWidgetHost
  // swap on the navigation (due to RenderDocument).
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/page_with_animation.html")));
  rwhvb = GetRenderWidgetHostView();
  EXPECT_FALSE(rwhvb->GetLocalSurfaceId().is_valid());

#if BUILDFLAG(IS_ANDROID)
  // Navigating while hidden should not generate a new surface. As the old one
  // is maintained as the fallback. The DelegatedFrameHost should have not have
  // a valid active viz::LocalSurfaceId until the first surface after navigation
  // has been embedded.
  EXPECT_TRUE(dfh->HasPrimarySurface());
  EXPECT_FALSE(dfh->IsPrimarySurfaceEvicted());
  EXPECT_EQ(initial_local_surface_id,
            dfh->content_layer()->surface_id().local_surface_id());
  EXPECT_FALSE(dfh->SurfaceId().local_surface_id().is_valid());
#endif

  // Showing the view should lead to a new surface being embedded.
  shell()->web_contents()->WasShown();
  viz::LocalSurfaceId new_rwhvb_local_surface_id = rwhvb->GetLocalSurfaceId();
  EXPECT_TRUE(new_rwhvb_local_surface_id.is_valid());
  EXPECT_NE(rwhvb_local_surface_id, new_rwhvb_local_surface_id);
#if BUILDFLAG(IS_ANDROID)
  EXPECT_TRUE(dfh->HasPrimarySurface());
  EXPECT_FALSE(dfh->IsPrimarySurfaceEvicted());
  viz::LocalSurfaceId new_local_surface_id =
      dfh->SurfaceId().local_surface_id();
  EXPECT_TRUE(new_local_surface_id.is_valid());
  EXPECT_NE(initial_local_surface_id, new_local_surface_id);
#endif
}

// Tests that if navigation fails, when re-using a RenderWidgetHostViewBase, and
// while it is hidden, that the fallback surface if invalidated. Then that when
// becoming visible, that a new valid surface is produced.
IN_PROC_BROWSER_TEST_F(NoCompositingRenderWidgetHostViewBrowserTest,
                       NoFallbackAfterHiddenNavigationFails) {
  ASSERT_TRUE(embedded_test_server()->Start());
  // Creates the initial RenderWidgetHostViewBase, and connects to a
  // CompositorFrameSink.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/page_with_animation.html")));
  RenderWidgetHostViewBase* rwhvb = GetRenderWidgetHostView();
  ASSERT_TRUE(rwhvb);
  viz::LocalSurfaceId rwhvb_local_surface_id = rwhvb->GetLocalSurfaceId();
  EXPECT_TRUE(rwhvb_local_surface_id.is_valid());

  // Hide the view before performing the next navigation.
  shell()->web_contents()->WasHidden();
#if BUILDFLAG(IS_ANDROID)
  // On Android we want to ensure that we maintain the currently embedded
  // surface. So that there is something to display when returning to the tab.
  RenderWidgetHostViewAndroid* rwhva =
      static_cast<RenderWidgetHostViewAndroid*>(rwhvb);
  ui::DelegatedFrameHostAndroid* dfh =
      rwhva->delegated_frame_host_for_testing();
  EXPECT_TRUE(dfh->HasPrimarySurface());
  EXPECT_FALSE(dfh->IsPrimarySurfaceEvicted());
  viz::LocalSurfaceId initial_local_surface_id =
      dfh->SurfaceId().local_surface_id();
  EXPECT_TRUE(initial_local_surface_id.is_valid());
#endif

  // Perform a navigation to the same content source. This will reuse the
  // existing RenderWidgetHostViewBase, except if we trigger a RenderWidgetHost
  // swap on the navigation (due to RenderDocument).
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/page_with_animation.html")));
  rwhvb = GetRenderWidgetHostView();
  EXPECT_FALSE(rwhvb->GetLocalSurfaceId().is_valid());

  // Surface Synchronization can lead to several different Surfaces being
  // embedded during a navigation. Ending once the Browser and Renderer have
  // agreed to a set of VisualProperties.
  //
  // If this takes too long we hit a timeout that attempts to reset us back to
  // the initial surface. So that some content state can be presented.
  //
  // If a navigation were to fail, then this would be invoked before any new
  // surface is embedded. For which we expect it to clear out the fallback
  // surfaces. As we cannot fallback to a surface from before navigation.
  rwhvb->ResetFallbackToFirstNavigationSurface();
  EXPECT_FALSE(rwhvb->HasFallbackSurface());

#if BUILDFLAG(IS_ANDROID)
  // Navigating while hidden should not generate a new surface.
  // The failed navigation above will lead to the primary surface being evicted.
  // The DelegatedFrameHost should have not have a valid active
  // viz::LocalSurfaceId until the first surface after navigation has been
  // embedded.
  EXPECT_FALSE(dfh->HasPrimarySurface());
  EXPECT_TRUE(dfh->IsPrimarySurfaceEvicted());
  EXPECT_FALSE(dfh->content_layer()->surface_id().is_valid());
  EXPECT_FALSE(dfh->SurfaceId().local_surface_id().is_valid());
#endif

  // Showing the view should lead to a new surface being embedded.
  shell()->web_contents()->WasShown();
  viz::LocalSurfaceId new_rwhvb_local_surface_id = rwhvb->GetLocalSurfaceId();
  EXPECT_TRUE(new_rwhvb_local_surface_id.is_valid());
  EXPECT_NE(rwhvb_local_surface_id, new_rwhvb_local_surface_id);
#if BUILDFLAG(IS_ANDROID)
  EXPECT_TRUE(dfh->HasPrimarySurface());
  EXPECT_FALSE(dfh->IsPrimarySurfaceEvicted());
  viz::LocalSurfaceId new_local_surface_id =
      dfh->SurfaceId().local_surface_id();
  EXPECT_TRUE(new_local_surface_id.is_valid());
  EXPECT_NE(initial_local_surface_id, new_local_surface_id);
#endif
}

#endif  // !BUILDFLAG(IS_MAC)

namespace {

#if BUILDFLAG(IS_ANDROID)
ui::DelegatedFrameHostAndroid* GetDelegatedFrameHost(
    RenderWidgetHostView* view) {
  return static_cast<RenderWidgetHostViewAndroid*>(view)
      ->delegated_frame_host_for_testing();
}
#else
DelegatedFrameHost* GetDelegatedFrameHost(RenderWidgetHostView* view) {
  DelegatedFrameHost* dfh = nullptr;
#if BUILDFLAG(IS_MAC)
  auto* compositor = GetBrowserCompositorMacForTesting(view);
  dfh = compositor->GetDelegatedFrameHost();
#elif BUILDFLAG(IS_IOS)
  auto* compositor = GetBrowserCompositorIOSForTesting(view);
  dfh = compositor->GetDelegatedFrameHost();
#elif defined(USE_AURA)
  dfh = static_cast<RenderWidgetHostViewAura*>(view)
            ->GetDelegatedFrameHostForTesting();
#endif
  return dfh;
}
#endif  // BUILDFLAG(IS_ANDROID)

viz::SurfaceId GetCurrentSurfaceIdOnDelegatedFrameHost(
    RenderWidgetHostView* view) {
  viz::SurfaceId surface_id;
#if BUILDFLAG(IS_ANDROID)
  ui::DelegatedFrameHostAndroid* dfh = GetDelegatedFrameHost(view);
  EXPECT_TRUE(dfh);
  surface_id = dfh->GetCurrentSurfaceIdForTesting();
#else
  DelegatedFrameHost* dfh = GetDelegatedFrameHost(view);
  EXPECT_TRUE(dfh);
  surface_id = dfh->GetCurrentSurfaceId();
#endif
  return surface_id;
}

viz::SurfaceId GetPreNavigationSurfaceIdOnDelegatedFrameHost(
    RenderWidgetHostView* view) {
  viz::SurfaceId surface_id;
#if BUILDFLAG(IS_ANDROID)
  ui::DelegatedFrameHostAndroid* dfh = GetDelegatedFrameHost(view);
  EXPECT_TRUE(dfh);
  surface_id = dfh->GetPreNavigationSurfaceIdForTesting();
#else
  DelegatedFrameHost* dfh = GetDelegatedFrameHost(view);
  EXPECT_TRUE(dfh);
  surface_id = dfh->GetPreNavigationSurfaceIdForTesting();
#endif
  return surface_id;
}

viz::SurfaceId GetFallbackSurfaceId(RenderWidgetHostView* view) {
  viz::SurfaceId surface_id;
#if BUILDFLAG(IS_ANDROID)
  ui::DelegatedFrameHostAndroid* dfh = GetDelegatedFrameHost(view);
  EXPECT_TRUE(dfh);
  surface_id = dfh->GetFallbackSurfaceIdForTesting();
#else
  DelegatedFrameHost* dfh = GetDelegatedFrameHost(view);
  EXPECT_TRUE(dfh);
  surface_id = dfh->GetFallbackSurfaceIdForTesting();
#endif
  return surface_id;
}

class BFCachedRenderWidgetHostViewBrowserTest
    : public NoCompositingRenderWidgetHostViewBrowserTest {
 public:
  BFCachedRenderWidgetHostViewBrowserTest() {
    std::vector<base::test::FeatureRefAndParams> enabled_features =
        GetDefaultEnabledBackForwardCacheFeaturesForTesting(
            /*ignore_outstanding_network_request=*/false);
    // To evict the main frame's `viz::SurfaceId` while in BFCache.
    enabled_features.push_back({features::kEvictSubtree, {{}}});

    scoped_feature_list_.InitWithFeaturesAndParameters(
        enabled_features,
        GetDefaultDisabledBackForwardCacheFeaturesForTesting());
  }
  ~BFCachedRenderWidgetHostViewBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};
}  // namespace

IN_PROC_BROWSER_TEST_F(BFCachedRenderWidgetHostViewBrowserTest,
                       BFCacheRestoredPageHasNewLocalSurfaceId) {
  if (!base::FeatureList::IsEnabled(
          features::kInvalidateLocalSurfaceIdPreCommit)) {
    return;
  }
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));
  RenderFrameHostWrapper rfh1(shell()->web_contents()->GetPrimaryMainFrame());

  const auto id_before_cached =
      GetCurrentSurfaceIdOnDelegatedFrameHost(rfh1->GetView());
  ASSERT_TRUE(id_before_cached.is_valid());

  // Navitate to title2.html. Title1.html is in the BFCache.
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title2.html")));
  const auto primary_id_for_title2 = GetCurrentSurfaceIdOnDelegatedFrameHost(
      shell()->web_contents()->GetPrimaryMainFrame()->GetView());

  ASSERT_TRUE(
      static_cast<RenderFrameHostImpl*>(rfh1.get())->IsInBackForwardCache());
  // `rfh1` is placed into BFCache. The LocalSurfaceId is preserved.
  const auto id_after_cached =
      GetCurrentSurfaceIdOnDelegatedFrameHost(rfh1->GetView());
  ASSERT_TRUE(id_after_cached.is_valid());
  ASSERT_EQ(id_before_cached, id_after_cached);
  // We shouldn't have a pre navigation ID. This is only used temporarily to
  // preserve the page's primary ID before it enters BFCache. It is reset after
  // the page enters the BFCache.
  const auto pre_nav_id_after_cached =
      GetPreNavigationSurfaceIdOnDelegatedFrameHost(rfh1->GetView());
  ASSERT_FALSE(pre_nav_id_after_cached.is_valid());

  // Restore `rfh1` from BFCache.
  ASSERT_TRUE(HistoryGoBack(shell()->web_contents()));

  const auto id_after_restore =
      GetCurrentSurfaceIdOnDelegatedFrameHost(rfh1->GetView());
  ASSERT_TRUE(id_after_restore.is_valid());

  // - When `rfh1` navigates away,
  //   `RenderWidgetHostViewBase::DidNavigateMainFramePreCommit`
  //   preserves the current `LocalSurfaceId` on the `DelegatedFrameHost`.
  // - When `rfh1` is restored from BFCache, the View will call
  //   `DelegatedFrameHost::WasShown()` with a new `LocalSurfaceId`.
  ASSERT_TRUE(id_after_restore.IsNewerThan(id_after_cached));

  const auto fallback_after_restore = GetFallbackSurfaceId(rfh1->GetView());
  if (viz::FrameEvictionManager::GetInstance()->GetMaxNumberOfSavedFrames() >
      1u) {
    // The last primary ID after the page before it entered BFCache now serves
    // as the fallback surface.
    ASSERT_EQ(fallback_after_restore, id_after_cached);
  } else {
    // If we can only have one frame at a time, the navigation from title1.html
    // to title2.html will evict the surfaces of title1.html. When we restore
    // title1.html from the BFCache, it will take the primary ID of title2.html
    // as the fallback.
    ASSERT_EQ(fallback_after_restore, primary_id_for_title2.ToSmallestId());
  }
}

// Same as the above test, except we resize the viewport while the page is in
// BFCache. The net effect is that we will NOT be using the last surface as
// the fallback for BFCache activation because resizing always regenerates a
// new ID as the fallback.
IN_PROC_BROWSER_TEST_F(
    BFCachedRenderWidgetHostViewBrowserTest,
    BFCachedPageResizedWhileHiddenShouldNotHavePreservedFallback) {
  if (!base::FeatureList::IsEnabled(
          features::kInvalidateLocalSurfaceIdPreCommit)) {
    return;
  }
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));
  RenderFrameHostWrapper rfh1(shell()->web_contents()->GetPrimaryMainFrame());

  const auto id_before_cached =
      GetCurrentSurfaceIdOnDelegatedFrameHost(rfh1->GetView());
  ASSERT_TRUE(id_before_cached.is_valid());

  // Navitate to title2.html. Title1.html is in the BFCache.
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title2.html")));
  ASSERT_TRUE(
      static_cast<RenderFrameHostImpl*>(rfh1.get())->IsInBackForwardCache());
  // `rfh1` is placed into BFCache. The LocalSurfaceId is preserved.
  const auto id_after_cached =
      GetCurrentSurfaceIdOnDelegatedFrameHost(rfh1->GetView());
  ASSERT_TRUE(id_after_cached.is_valid());
  ASSERT_EQ(id_before_cached, id_after_cached);
  const auto pre_nav_id_after_cached =
      GetPreNavigationSurfaceIdOnDelegatedFrameHost(rfh1->GetView());
  ASSERT_FALSE(pre_nav_id_after_cached.is_valid());

  // Resize.
#if BUILDFLAG(IS_ANDROID)
  auto new_size = shell()
                      ->web_contents()
                      ->GetRenderWidgetHostView()
                      ->GetVisibleViewportSize();
  new_size.set_height(new_size.height() / 2);
  auto* web_contents = static_cast<WebContentsImpl*>(shell()->web_contents());
  web_contents->GetNativeView()->OnSizeChanged(new_size.width(),
                                               new_size.height());
  web_contents->GetNativeView()->OnPhysicalBackingSizeChanged(
      gfx::ScaleToCeiledSize(
          web_contents->GetNativeView()->GetPhysicalBackingSize(), 0.5f, 1));
#else
  auto view_bounds = shell()->web_contents()->GetViewBounds();
  view_bounds.set_height(view_bounds.height() / 2);
  shell()->web_contents()->Resize(view_bounds);
#endif

  // Restore `rfh1` from BFCache.
  ASSERT_TRUE(HistoryGoBack(shell()->web_contents()));

  // Resize has given us a newer ID.
  const auto id_after_restore =
      GetCurrentSurfaceIdOnDelegatedFrameHost(rfh1->GetView());
  ASSERT_TRUE(id_after_restore.is_valid());
  ASSERT_TRUE(id_after_restore.IsNewerThan(id_after_cached));

  const auto fallback_after_restore = GetFallbackSurfaceId(rfh1->GetView());

  // The fallback is equal to the primary ID after the restore. This is due to
  // the resizing.
  ASSERT_EQ(fallback_after_restore, id_after_restore);
}

// Same as above, except that the resize operation is a no-op.
IN_PROC_BROWSER_TEST_F(BFCachedRenderWidgetHostViewBrowserTest,
                       BFCachedPageNoopResizedWhileHiddenHasPreservedFallback) {
  if (!base::FeatureList::IsEnabled(
          features::kInvalidateLocalSurfaceIdPreCommit)) {
    return;
  }
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));
  RenderFrameHostWrapper rfh1(shell()->web_contents()->GetPrimaryMainFrame());

  const auto id_before_cached =
      GetCurrentSurfaceIdOnDelegatedFrameHost(rfh1->GetView());
  ASSERT_TRUE(id_before_cached.is_valid());

  // Navitate to title2.html. Title1.html is in the BFCache.
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title2.html")));
  const auto primary_id_for_title2 = GetCurrentSurfaceIdOnDelegatedFrameHost(
      shell()->web_contents()->GetPrimaryMainFrame()->GetView());

  ASSERT_TRUE(
      static_cast<RenderFrameHostImpl*>(rfh1.get())->IsInBackForwardCache());
  // `rfh1` is placed into BFCache. The LocalSurfaceId is preserved.
  const auto id_after_cached =
      GetCurrentSurfaceIdOnDelegatedFrameHost(rfh1->GetView());
  ASSERT_TRUE(id_after_cached.is_valid());
  ASSERT_EQ(id_before_cached, id_after_cached);
  const auto pre_nav_id_after_cached =
      GetPreNavigationSurfaceIdOnDelegatedFrameHost(rfh1->GetView());
  ASSERT_FALSE(pre_nav_id_after_cached.is_valid());

  // No-op resize.
#if BUILDFLAG(IS_ANDROID)
  auto new_size = shell()
                      ->web_contents()
                      ->GetRenderWidgetHostView()
                      ->GetVisibleViewportSize();
  auto* web_contents = static_cast<WebContentsImpl*>(shell()->web_contents());
  web_contents->GetNativeView()->OnSizeChanged(new_size.width(),
                                               new_size.height());
  web_contents->GetNativeView()->OnPhysicalBackingSizeChanged(
      web_contents->GetNativeView()->GetPhysicalBackingSize());
#else
  shell()->web_contents()->Resize(shell()->web_contents()->GetViewBounds());
#endif

  // Restore `rfh1` from BFCache.
  ASSERT_TRUE(HistoryGoBack(shell()->web_contents()));

  const auto id_after_restore =
      GetCurrentSurfaceIdOnDelegatedFrameHost(rfh1->GetView());
  ASSERT_TRUE(id_after_restore.is_valid());
  const auto fallback_after_restore = GetFallbackSurfaceId(rfh1->GetView());

  if (viz::FrameEvictionManager::GetInstance()->GetMaxNumberOfSavedFrames() >
      1u) {
    // The expectation is the same as if the no-op resize isn't called - the
    // last
    // primary ID after the page before it entered BFCache now serves as the
    // fallback surface.
    ASSERT_EQ(fallback_after_restore, id_after_cached);
  } else {
    // If we can only have one frame at a time, the navigation from title1.html
    // to title2.html will evict the surfaces of title1.html. When we restore
    // title1.html from the BFCache, it will take the primary ID of title2.html
    // as the fallback.
    ASSERT_EQ(fallback_after_restore, primary_id_for_title2.ToSmallestId());
  }
}

IN_PROC_BROWSER_TEST_F(BFCachedRenderWidgetHostViewBrowserTest,
                       BFCachedViewShouldNotBeEvicted) {
  if (!base::FeatureList::IsEnabled(
          features::kInvalidateLocalSurfaceIdPreCommit)) {
    return;
  }
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));
  RenderFrameHostWrapper rfh1(shell()->web_contents()->GetPrimaryMainFrame());

  const auto id_before_cached =
      GetCurrentSurfaceIdOnDelegatedFrameHost(rfh1->GetView());
  ASSERT_TRUE(id_before_cached.is_valid());

  // Navitate to title2.html. Title1.html is in the BFCache.
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title2.html")));
  ASSERT_TRUE(
      static_cast<RenderFrameHostImpl*>(rfh1.get())->IsInBackForwardCache());
  // `rfh1` is placed into BFCache. The LocalSurfaceId is preserved.
  const auto id_after_cached =
      GetCurrentSurfaceIdOnDelegatedFrameHost(rfh1->GetView());
  ASSERT_TRUE(id_after_cached.is_valid());
  ASSERT_EQ(id_before_cached, id_after_cached);

  // If we only can save one frame - navigating to Title2.html will evict the
  // BFCached surface of title1. This should happen only on low memory devices,
  // or the memory pressure is high.
  if (viz::FrameEvictionManager::GetInstance()->GetMaxNumberOfSavedFrames() >
      1u) {
    // `ResetFallbackToFirstNavigationSurface()` should call
    // `RWHImpl::CollectSurfaceIdsForEviction()` which would mark the View as
    // evicted. It won't happen, however, because the View has entered BFCache
    // thus `ResetFallbackToFirstNavigationSurface()` won't be able to evict
    // anything.
    GetDelegatedFrameHost(rfh1->GetView())
        ->ResetFallbackToFirstNavigationSurface();
    ASSERT_FALSE(
        static_cast<RenderWidgetHostViewBase*>(rfh1->GetView())->is_evicted());

    // Even though `ResetFallbackToFirstNavigationSurface()` shouldn't evict
    // the BFCached surface, the surface should still be reachable via
    // `RWHImpl::CollectSurfaceIdsForEviction()`.
    //
    // Note: `RWHImpl::CollectSurfaceIdsForEviction()` has the side effect of
    // marking the View as evicted, so this assetion needs to be placed at the
    // end.
    const auto evicted_ids =
        static_cast<RenderWidgetHostImpl*>(rfh1->GetRenderWidgetHost())
            ->CollectSurfaceIdsForEviction();
    ASSERT_TRUE(base::Contains(evicted_ids, id_after_cached));
  }
}

// Tests that if a pending commit attempts to swap from a RenderFrameHost which
// has no Fallback Surface, that we clear pre-existing ones in a
// RenderWidgetHostViewBase that is being re-used. While still properly
// allocating a new Surface if Navigation eventually succeeds.
IN_PROC_BROWSER_TEST_F(NoCompositingRenderWidgetHostViewBrowserTest,
                       NoFallbackIfSwapFailedBeforeNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  // Creates the initial RenderWidgetHostViewBase, and connects to a
  // CompositorFrameSink.
  GURL url(embedded_test_server()->GetURL("/page_with_animation.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  RenderWidgetHostViewBase* rwhvb = GetRenderWidgetHostView();
  ASSERT_TRUE(rwhvb);
  viz::LocalSurfaceId initial_lsid = rwhvb->GetLocalSurfaceId();
  EXPECT_TRUE(initial_lsid.is_valid());

  // Actually set our Fallback Surface.
  rwhvb->ResetFallbackToFirstNavigationSurface();
  EXPECT_TRUE(rwhvb->HasFallbackSurface());

  // Perform a navigation to the same content source. This will reuse the
  // existing RenderWidgetHostViewBase, except if we trigger a RenderWidgetHost
  // swap on the navigation (due to RenderDocument).
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  // Actually complete a navigation once we've removed the Fallback Surface.
  // This should lead to a new viz::LocalSurfaceId.
  TestNavigationManager nav_manager(shell()->web_contents(), url);
  shell()->LoadURL(url);
  EXPECT_TRUE(nav_manager.WaitForResponse());
  // Notify that this pending commit has no RenderFrameHost with which to get a
  // Fallback Surface. This should evict the Fallback Surface.
  RenderFrameHostImpl* pending_rfh = static_cast<RenderFrameHostImpl*>(
      nav_manager.GetNavigationHandle()->GetRenderFrameHost());
  web_contents->NotifySwappedFromRenderManagerWithoutFallbackContent(
      pending_rfh);
  rwhvb = static_cast<RenderWidgetHostViewBase*>(pending_rfh->GetView());
  EXPECT_FALSE(rwhvb->HasFallbackSurface());
  EXPECT_TRUE(nav_manager.WaitForNavigationFinished());

  EXPECT_EQ(rwhvb, GetRenderWidgetHostView());
  EXPECT_TRUE(rwhvb->GetLocalSurfaceId().is_valid());
  viz::LocalSurfaceId post_nav_lsid = rwhvb->GetLocalSurfaceId();
  EXPECT_NE(initial_lsid, post_nav_lsid);

  // When RenderDocument is enabled, the RWHV will change as well so the
  // LocalSurfaceIds are not comparable.
  if (!ShouldCreateNewHostForAllFrames()) {
    EXPECT_TRUE(post_nav_lsid.IsNewerThan(initial_lsid));
  }
}

namespace {

std::unique_ptr<net::test_server::HttpResponse> HandleSlowStyleSheet(
    const net::test_server::HttpRequest& request) {
  // The CSS stylesheet we want to be slow will have this path.
  if (request.relative_url != "/slow-response")
    return nullptr;
  return std::make_unique<SlowHttpResponse>(SlowHttpResponse::NoResponse());
}

class DOMContentLoadedObserver : public WebContentsObserver {
 public:
  explicit DOMContentLoadedObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  bool Wait() {
    run_loop_.Run();
    return dom_content_loaded_ && !did_paint_;
  }

 private:
  // WebContentsObserver:
  void DOMContentLoaded(RenderFrameHost* render_frame_host) override {
    dom_content_loaded_ = true;
    run_loop_.Quit();
  }
  void DidFirstVisuallyNonEmptyPaint() override { did_paint_ = true; }

  base::RunLoop run_loop_;
  bool did_paint_{false};
  bool dom_content_loaded_{false};
};

}  // namespace

IN_PROC_BROWSER_TEST_F(NoCompositingRenderWidgetHostViewBrowserTest,
                       ColorSchemeMetaBackground) {
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(&HandleSlowStyleSheet));
  ASSERT_TRUE(embedded_test_server()->Start());
  DOMContentLoadedObserver observer(shell()->web_contents());
  shell()->LoadURL(
      embedded_test_server()->GetURL("/dark_color_scheme_meta_slow.html"));
  EXPECT_TRUE(observer.Wait());
  auto bg_color = GetRenderWidgetHostView()->content_background_color();
  ASSERT_TRUE(bg_color.has_value());
  EXPECT_EQ(SkColorSetRGB(18, 18, 18), bg_color.value());
}

IN_PROC_BROWSER_TEST_F(NoCompositingRenderWidgetHostViewBrowserTest,
                       NoColorSchemeMetaBackground) {
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(&HandleSlowStyleSheet));
  ASSERT_TRUE(embedded_test_server()->Start());
  DOMContentLoadedObserver observer(shell()->web_contents());
  shell()->LoadURL(
      embedded_test_server()->GetURL("/no_color_scheme_meta_slow.html"));
  EXPECT_TRUE(observer.Wait());
  auto bg_color = GetRenderWidgetHostView()->content_background_color();
  ASSERT_FALSE(bg_color.has_value());
}

IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewBrowserTestBase,
                       CompositorWorksWhenReusingRenderer) {
  ASSERT_TRUE(embedded_test_server()->Start());
  auto* web_contents = shell()->web_contents();
  // Load a page that draws new frames infinitely.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/page_with_animation.html")));
  std::unique_ptr<RenderFrameSubmissionObserver> frame_observer(
      std::make_unique<RenderFrameSubmissionObserver>(web_contents));

  // Open a new page in the same renderer to keep it alive.
  WebContents::CreateParams new_contents_params(
      web_contents->GetBrowserContext(), web_contents->GetSiteInstance());
  std::unique_ptr<WebContents> new_web_contents(
      WebContents::Create(new_contents_params));

  new_web_contents->GetController().LoadURLWithParams(
      NavigationController::LoadURLParams(GURL(url::kAboutBlankURL)));
  EXPECT_TRUE(WaitForLoadStop(new_web_contents.get()));

  // Start a cross-process navigation.
  shell()->LoadURL(embedded_test_server()->GetURL("foo.com", "/title1.html"));

  // When the navigation is about to commit, wait for the next frame to be
  // submitted by the renderer before proceeding with page load.
  {
    CommitBeforeSwapAckSentHelper commit_helper(web_contents,
                                                frame_observer.get());
    EXPECT_TRUE(WaitForLoadStop(web_contents));
    EXPECT_NE(web_contents->GetPrimaryMainFrame()->GetProcess(),
              new_web_contents->GetPrimaryMainFrame()->GetProcess());
  }

  // Go back and verify that the renderer continues to draw new frames.
  shell()->GoBackOrForward(-1);
  // Stop observing before we destroy |web_contents| in WaitForLoadStop.
  frame_observer.reset();
  EXPECT_TRUE(WaitForLoadStop(web_contents));
  EXPECT_EQ(web_contents->GetPrimaryMainFrame()->GetProcess(),
            new_web_contents->GetPrimaryMainFrame()->GetProcess());
  MainThreadFrameObserver observer(
      web_contents->GetPrimaryMainFrame()->GetRenderViewHost()->GetWidget());
  for (int i = 0; i < 5; ++i)
    observer.Wait();
}

enum CompositingMode {
  GL_COMPOSITING,
  SOFTWARE_COMPOSITING,
};

class CompositingRenderWidgetHostViewBrowserTest
    : public RenderWidgetHostViewBrowserTest,
      public testing::WithParamInterface<CompositingMode> {
 public:
  CompositingRenderWidgetHostViewBrowserTest()
      : compositing_mode_(GetParam()) {}

  CompositingRenderWidgetHostViewBrowserTest(
      const CompositingRenderWidgetHostViewBrowserTest&) = delete;
  CompositingRenderWidgetHostViewBrowserTest& operator=(
      const CompositingRenderWidgetHostViewBrowserTest&) = delete;

  void SetUp() override {
    if (compositing_mode_ == SOFTWARE_COMPOSITING)
      UseSoftwareCompositing();
    EnablePixelOutput(scale());
    RenderWidgetHostViewBrowserTest::SetUp();
  }

  virtual GURL TestUrl() {
    return net::FilePathToFileURL(
        test_dir().AppendASCII("rwhv_compositing_animation.html"));
  }

  bool SetUpSourceSurface(const char* wait_message) override {
    content::DOMMessageQueue message_queue(shell()->web_contents());
    EXPECT_TRUE(NavigateToURL(shell(), TestUrl()));
    if (wait_message != nullptr) {
      std::string result(wait_message);
      if (!message_queue.WaitForMessage(&result)) {
        EXPECT_TRUE(false) << "WaitForMessage " << result << " failed.";
        return false;
      }
    }

    // A frame might not be available yet. So, wait for it.
    WaitForCopySourceReady();
    return true;
  }

  virtual float scale() const { return 1.f; }

 private:
  const CompositingMode compositing_mode_;
};

// Disable tests for Android as it has an incomplete implementation.
#if !BUILDFLAG(IS_ANDROID)

// The CopyFromSurface() API should work on all platforms when compositing is
// enabled.
IN_PROC_BROWSER_TEST_P(CompositingRenderWidgetHostViewBrowserTest,
                       CopyFromSurface) {
  SET_UP_SURFACE_OR_PASS_TEST(nullptr);

  // Repeatedly call CopyFromSurface() since, on some platforms (e.g., Windows),
  // the operation will fail until the first "present" has been made.
  int count_attempts = 0;
  while (true) {
    ++count_attempts;
    base::RunLoop run_loop;
    GetRenderWidgetHostView()->CopyFromSurface(
        gfx::Rect(), frame_size(),
        base::BindOnce(&RenderWidgetHostViewBrowserTest::FinishCopyFromSurface,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();

    if (frames_captured())
      break;
    else
      GiveItSomeTime();
  }

  EXPECT_EQ(count_attempts, callback_invoke_count());
  EXPECT_EQ(1, frames_captured());
}

// Tests that the callback passed to CopyFromSurface is always called, even
// when the RenderWidgetHostView is deleting in the middle of an async copy.
IN_PROC_BROWSER_TEST_P(CompositingRenderWidgetHostViewBrowserTest,
                       CopyFromSurface_CallbackDespiteDelete) {
  SET_UP_SURFACE_OR_PASS_TEST(nullptr);

  base::RunLoop run_loop;
  GetRenderWidgetHostView()->CopyFromSurface(
      gfx::Rect(), frame_size(),
      base::BindOnce(&RenderWidgetHostViewBrowserTest::FinishCopyFromSurface,
                     base::Unretained(this), run_loop.QuitClosure()));
  shell()->web_contents()->Close();
  run_loop.Run();
  while (callback_invoke_count() == 0)
    GiveItSomeTime();

  EXPECT_EQ(1, callback_invoke_count());
}

class CompositingRenderWidgetHostViewBrowserTestTabCapture
    : public CompositingRenderWidgetHostViewBrowserTest {
 public:
  CompositingRenderWidgetHostViewBrowserTestTabCapture()
      : readback_result_(READBACK_NO_RESPONSE),
        allowable_error_(0),
        test_url_("data:text/html,<!doctype html>") {}

  void VerifyResult(base::OnceClosure quit_callback, const SkBitmap& bitmap) {
    if (bitmap.drawsNothing()) {
      readback_result_ = READBACK_FAILED;
      std::move(quit_callback).Run();
      return;
    }
    readback_result_ = READBACK_SUCCESS;

    // Check that the |bitmap| contains cyan and/or yellow pixels.  This is
    // needed because the compositor will read back "blank" frames until the
    // first frame from the renderer is composited.  See comments in
    // PerformTestWithLeftRightRects() for more details about eliminating test
    // flakiness.
    bool contains_a_test_color = false;
    for (int i = 0; i < bitmap.width(); ++i) {
      for (int j = 0; j < bitmap.height(); ++j) {
        if (!exclude_rect_.IsEmpty() && exclude_rect_.Contains(i, j))
          continue;

        const unsigned high_threshold = 0xff - allowable_error_;
        const unsigned low_threshold = 0x00 + allowable_error_;
        const SkColor color = bitmap.getColor(i, j);
        const bool is_cyan = SkColorGetR(color) <= low_threshold &&
                             SkColorGetG(color) >= high_threshold &&
                             SkColorGetB(color) >= high_threshold;
        const bool is_yellow = SkColorGetR(color) >= high_threshold &&
                               SkColorGetG(color) >= high_threshold &&
                               SkColorGetB(color) <= low_threshold;
        if (is_cyan || is_yellow) {
          contains_a_test_color = true;
          break;
        }
      }
    }
    if (!contains_a_test_color) {
      readback_result_ = READBACK_NO_TEST_COLORS;
      std::move(quit_callback).Run();
      return;
    }

    // Compare the readback |bitmap| to the |expected_bitmap|, pixel-by-pixel.
    const SkBitmap& expected_bitmap =
        expected_copy_from_compositing_surface_bitmap_;
    EXPECT_EQ(expected_bitmap.width(), bitmap.width());
    EXPECT_EQ(expected_bitmap.height(), bitmap.height());
    if (expected_bitmap.width() != bitmap.width() ||
        expected_bitmap.height() != bitmap.height()) {
      readback_result_ = READBACK_INCORRECT_RESULT_SIZE;
      std::move(quit_callback).Run();
      return;
    }
    EXPECT_EQ(expected_bitmap.colorType(), bitmap.colorType());
    int fails = 0;
    // Note: The outermost 2 pixels are ignored because the scaling tests pick
    // up a little bleed-in from the surrounding content.
    for (int i = 2; i < bitmap.width() - 4 && fails < 10; ++i) {
      for (int j = 2; j < bitmap.height() - 4 && fails < 10; ++j) {
        if (!exclude_rect_.IsEmpty() && exclude_rect_.Contains(i, j))
          continue;

        SkColor expected_color = expected_bitmap.getColor(i, j);
        SkColor color = bitmap.getColor(i, j);
        int expected_alpha = SkColorGetA(expected_color);
        int alpha = SkColorGetA(color);
        int expected_red = SkColorGetR(expected_color);
        int red = SkColorGetR(color);
        int expected_green = SkColorGetG(expected_color);
        int green = SkColorGetG(color);
        int expected_blue = SkColorGetB(expected_color);
        int blue = SkColorGetB(color);
        EXPECT_NEAR(expected_alpha, alpha, allowable_error_)
            << "expected_color: " << std::hex << expected_color
            << " color: " <<  color
            << " Failed at " << std::dec << i << ", " << j
            << " Failure " << ++fails;
        EXPECT_NEAR(expected_red, red, allowable_error_)
            << "expected_color: " << std::hex << expected_color
            << " color: " <<  color
            << " Failed at " << std::dec << i << ", " << j
            << " Failure " << ++fails;
        EXPECT_NEAR(expected_green, green, allowable_error_)
            << "expected_color: " << std::hex << expected_color
            << " color: " <<  color
            << " Failed at " << std::dec << i << ", " << j
            << " Failure " << ++fails;
        EXPECT_NEAR(expected_blue, blue, allowable_error_)
            << "expected_color: " << std::hex << expected_color
            << " color: " <<  color
            << " Failed at " << std::dec << i << ", " << j
            << " Failure " << ++fails;
      }
    }
    EXPECT_LT(fails, 10);

    std::move(quit_callback).Run();
  }

  void SetAllowableError(int amount) { allowable_error_ = amount; }
  void SetExcludeRect(gfx::Rect exclude) { exclude_rect_ = exclude; }

  GURL TestUrl() override { return GURL(test_url_); }

  void SetTestUrl(const std::string& url) { test_url_ = url; }

  // Loads a page two boxes side-by-side, each half the width of
  // |html_rect_size|, and with different background colors. The test then
  // copies from |copy_rect| region of the page into a bitmap of size
  // |output_size|, and examines the resulting bitmap.
  // Note that |output_size| may not have the same size as |copy_rect| (e.g.
  // when the output is scaled).
  void PerformTestWithLeftRightRects(const gfx::Size& html_rect_size,
                                     const gfx::Rect& copy_rect,
                                     const gfx::Size& output_size) {
    const gfx::Size box_size(html_rect_size.width() / 2,
                             html_rect_size.height());
    SetTestUrl(base::StringPrintf(
        "data:text/html,<!doctype html>"
        "<div class='left'>"
        "  <div class='right'></div>"
        "</div>"
        "<style>"
        "body { padding: 0; margin: 0; }"
        ".left { position: absolute;"
        "        background: %%230ff;"
        "        width: %dpx;"
        "        height: %dpx;"
        "}"
        ".right { position: absolute;"
        "         left: %dpx;"
        "         background: %%23ff0;"
        "         width: %dpx;"
        "         height: %dpx;"
        "}"
        "</style>"
        "<script>"
        "  domAutomationController.send(\"DONE\");"
        "</script>",
        box_size.width(),
        box_size.height(),
        box_size.width(),
        box_size.width(),
        box_size.height()));

    SET_UP_SURFACE_OR_PASS_TEST("\"DONE\"");
    if (!ShouldContinueAfterTestURLLoad())
      return;

    RenderWidgetHostViewBase* rwhv = GetRenderWidgetHostView();

    SetupLeftRightBitmap(output_size,
                         &expected_copy_from_compositing_surface_bitmap_);

    // The page is loaded in the renderer.  Request frames from the renderer
    // until readback succeeds.  When readback succeeds, the resulting
    // SkBitmap is examined to ensure it matches the expected result.
    // This loop is needed because:
    //   1. Painting/Compositing is not synchronous with the Javascript engine,
    //      and so the "DONE" signal above could be received before the renderer
    //      provides a frame with the expected content.  http://crbug.com/405282
    //   2. Avoiding test flakiness: On some platforms, the readback operation
    //      is allowed to transiently fail.  The purpose of these tests is to
    //      confirm correct cropping/scaling behavior; and not that every
    //      readback must succeed.  http://crbug.com/444237
    int attempt_count = 0;
    do {
      // Wait a little before retrying again. This gives the most up-to-date
      // frame a chance to propagate from the renderer to the compositor.
      if (attempt_count > 0)
        GiveItSomeTime();
      ++attempt_count;

      // Request readback.  The callbacks will examine the pixels in the
      // SkBitmap result if readback was successful.
      readback_result_ = READBACK_NO_RESPONSE;
      SetAllowableError(2);
      // Scaling can cause blur/fuzz between color boundaries, particularly in
      // the middle columns for these tests.
      SetExcludeRect(
          gfx::Rect(output_size.width() / 2 - 1, 0, 2, output_size.height()));

      base::RunLoop run_loop;
      rwhv->CopyFromSurface(
          copy_rect, output_size,
          base::BindOnce(&CompositingRenderWidgetHostViewBrowserTestTabCapture::
                             VerifyResult,
                         base::Unretained(this), run_loop.QuitClosure()));
      run_loop.Run();

      // If the readback operation did not provide a frame, log the reason
      // to aid in future debugging.  This information will also help determine
      // whether the implementation is broken, or a test bot is in a bad state.
      // clang-format off
      switch (readback_result_) {
        case READBACK_SUCCESS:
          break;
        #define CASE_LOG_READBACK_WARNING(enum_value)                    \
          case enum_value:                                               \
            LOG(WARNING) << "Readback attempt failed (attempt #"         \
                         << attempt_count << ").  Reason: " #enum_value; \
            break
        CASE_LOG_READBACK_WARNING(READBACK_FAILED);
        CASE_LOG_READBACK_WARNING(READBACK_NO_TEST_COLORS);
        CASE_LOG_READBACK_WARNING(READBACK_INCORRECT_RESULT_SIZE);
        default:
          LOG(ERROR)
              << "Invalid readback response value: " << readback_result_;
          NOTREACHED();
      }
      // clang-format on
    } while (readback_result_ != READBACK_SUCCESS &&
             !testing::Test::HasFailure());
  }

  // Sets up |bitmap| to have size |copy_size|. It floods the left half with
  // #0ff and the right half with #ff0.
  void SetupLeftRightBitmap(const gfx::Size& copy_size, SkBitmap* bitmap) {
    bitmap->allocN32Pixels(copy_size.width(), copy_size.height());
    // Left half is #0ff.
    bitmap->eraseARGB(255, 0, 255, 255);
    // Right half is #ff0.
    for (int i = 0; i < copy_size.width() / 2; ++i) {
      for (int j = 0; j < copy_size.height(); ++j) {
        *(bitmap->getAddr32(copy_size.width() / 2 + i, j)) =
            SkColorSetARGB(255, 255, 255, 0);
      }
    }
  }

 protected:
  // An enum to distinguish between reasons for result verify failures.
  enum ReadbackResult {
    READBACK_NO_RESPONSE,
    READBACK_SUCCESS,
    READBACK_FAILED,
    READBACK_NO_TEST_COLORS,
    READBACK_INCORRECT_RESULT_SIZE,
  };

  virtual bool ShouldContinueAfterTestURLLoad() {
    return true;
  }

 private:
  ReadbackResult readback_result_;
  SkBitmap expected_copy_from_compositing_surface_bitmap_;
  int allowable_error_;
  gfx::Rect exclude_rect_;
  std::string test_url_;
};

IN_PROC_BROWSER_TEST_P(CompositingRenderWidgetHostViewBrowserTestTabCapture,
                       CopyFromSurface_Origin_Unscaled) {
  gfx::Rect copy_rect(400, 300);
  gfx::Size output_size = copy_rect.size();
  gfx::Size html_rect_size(400, 300);
  PerformTestWithLeftRightRects(html_rect_size, copy_rect, output_size);
}

IN_PROC_BROWSER_TEST_P(CompositingRenderWidgetHostViewBrowserTestTabCapture,
                       CopyFromSurface_Origin_Scaled) {
  gfx::Rect copy_rect(400, 300);
  gfx::Size output_size(200, 100);
  gfx::Size html_rect_size(400, 300);
  PerformTestWithLeftRightRects(html_rect_size, copy_rect, output_size);
}

IN_PROC_BROWSER_TEST_P(CompositingRenderWidgetHostViewBrowserTestTabCapture,
                       CopyFromSurface_Cropped_Unscaled) {
  // Grab 60x60 pixels from the center of the tab contents.
  gfx::Rect copy_rect(400, 300);
  copy_rect = gfx::Rect(copy_rect.CenterPoint() - gfx::Vector2d(30, 30),
                        gfx::Size(60, 60));
  gfx::Size output_size = copy_rect.size();
  gfx::Size html_rect_size(400, 300);
  PerformTestWithLeftRightRects(html_rect_size, copy_rect, output_size);
}

IN_PROC_BROWSER_TEST_P(CompositingRenderWidgetHostViewBrowserTestTabCapture,
                       CopyFromSurface_Cropped_Scaled) {
  // Grab 60x60 pixels from the center of the tab contents.
  gfx::Rect copy_rect(400, 300);
  copy_rect = gfx::Rect(copy_rect.CenterPoint() - gfx::Vector2d(30, 30),
                        gfx::Size(60, 60));
  gfx::Size output_size(20, 10);
  gfx::Size html_rect_size(400, 300);
  PerformTestWithLeftRightRects(html_rect_size, copy_rect, output_size);
}

class CompositingRenderWidgetHostViewBrowserTestTabCaptureHighDPI
    : public CompositingRenderWidgetHostViewBrowserTestTabCapture {
 public:
  CompositingRenderWidgetHostViewBrowserTestTabCaptureHighDPI() {}

  CompositingRenderWidgetHostViewBrowserTestTabCaptureHighDPI(
      const CompositingRenderWidgetHostViewBrowserTestTabCaptureHighDPI&) =
      delete;
  CompositingRenderWidgetHostViewBrowserTestTabCaptureHighDPI& operator=(
      const CompositingRenderWidgetHostViewBrowserTestTabCaptureHighDPI&) =
      delete;

 protected:
  bool ShouldContinueAfterTestURLLoad() override {
    // Short-circuit a pass for platforms where setting up high-DPI fails.
    const float actual_scale_factor =
        GetScaleFactorForView(GetRenderWidgetHostView());
    if (actual_scale_factor != scale()) {
      LOG(WARNING) << "Blindly passing this test; unable to force device scale "
                   << "factor: seems to be " << actual_scale_factor
                   << " but expected " << scale();
      return false;
    }
    VLOG(1) << ("Successfully forced device scale factor.  Moving forward with "
                "this test!  :-)");
    return true;
  }

  float scale() const override { return 2.0f; }
};

// NineImagePainter implementation crashes the process on Windows when this
// content_browsertest forces a device scale factor.  http://crbug.com/399349
#if BUILDFLAG(IS_WIN)
#define MAYBE_CopyToBitmap_EntireRegion DISABLED_CopyToBitmap_EntireRegion
#define MAYBE_CopyToBitmap_CenterRegion DISABLED_CopyToBitmap_CenterRegion
#define MAYBE_CopyToBitmap_ScaledResult DISABLED_CopyToBitmap_ScaledResult
#else
#define MAYBE_CopyToBitmap_EntireRegion CopyToBitmap_EntireRegion
#define MAYBE_CopyToBitmap_CenterRegion CopyToBitmap_CenterRegion
#define MAYBE_CopyToBitmap_ScaledResult CopyToBitmap_ScaledResult
#endif

IN_PROC_BROWSER_TEST_P(
    CompositingRenderWidgetHostViewBrowserTestTabCaptureHighDPI,
    MAYBE_CopyToBitmap_EntireRegion) {
  gfx::Size html_rect_size(200, 150);
  gfx::Rect copy_rect(200, 150);
  // Scale the output size so that, internally, scaling is not occurring.
  gfx::Size output_size = gfx::ScaleToRoundedSize(copy_rect.size(), scale());
  PerformTestWithLeftRightRects(html_rect_size, copy_rect, output_size);
}

IN_PROC_BROWSER_TEST_P(
    CompositingRenderWidgetHostViewBrowserTestTabCaptureHighDPI,
    MAYBE_CopyToBitmap_CenterRegion) {
  gfx::Size html_rect_size(200, 150);
  // Grab 90x60 pixels from the center of the tab contents.
  gfx::Rect copy_rect =
      gfx::Rect(gfx::Rect(html_rect_size).CenterPoint() - gfx::Vector2d(45, 30),
                gfx::Size(90, 60));
  // Scale the output size so that, internally, scaling is not occurring.
  gfx::Size output_size = gfx::ScaleToRoundedSize(copy_rect.size(), scale());
  PerformTestWithLeftRightRects(html_rect_size, copy_rect, output_size);
}

IN_PROC_BROWSER_TEST_P(
    CompositingRenderWidgetHostViewBrowserTestTabCaptureHighDPI,
    MAYBE_CopyToBitmap_ScaledResult) {
  gfx::Size html_rect_size(200, 100);
  gfx::Rect copy_rect(200, 100);
  // Output is being down-scaled since output_size is in phyiscal pixels.
  gfx::Size output_size(200, 100);
  PerformTestWithLeftRightRects(html_rect_size, copy_rect, output_size);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// On ChromeOS there is no software compositing.
static const auto kTestCompositingModes = testing::Values(GL_COMPOSITING);
#else
static const auto kTestCompositingModes =
    testing::Values(GL_COMPOSITING, SOFTWARE_COMPOSITING);
#endif

INSTANTIATE_TEST_SUITE_P(GLAndSoftwareCompositing,
                         CompositingRenderWidgetHostViewBrowserTest,
                         kTestCompositingModes);
INSTANTIATE_TEST_SUITE_P(GLAndSoftwareCompositing,
                         CompositingRenderWidgetHostViewBrowserTestTabCapture,
                         kTestCompositingModes);
INSTANTIATE_TEST_SUITE_P(
    GLAndSoftwareCompositing,
    CompositingRenderWidgetHostViewBrowserTestTabCaptureHighDPI,
    kTestCompositingModes);

class RenderWidgetHostViewPresentationFeedbackBrowserTest
    : public NoCompositingRenderWidgetHostViewBrowserTest {
 public:
  RenderWidgetHostViewPresentationFeedbackBrowserTest(
      const RenderWidgetHostViewPresentationFeedbackBrowserTest&) = delete;
  RenderWidgetHostViewPresentationFeedbackBrowserTest& operator=(
      const RenderWidgetHostViewPresentationFeedbackBrowserTest&) = delete;

 protected:
  using TabSwitchResult = blink::ContentToVisibleTimeReporter::TabSwitchResult;

  RenderWidgetHostViewPresentationFeedbackBrowserTest() = default;

  ~RenderWidgetHostViewPresentationFeedbackBrowserTest() override = default;

  void SetUpOnMainThread() override {
    NoCompositingRenderWidgetHostViewBrowserTest::SetUpOnMainThread();

    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(NavigateToURL(
        shell(), embedded_test_server()->GetURL("/page_with_animation.html")));

    RenderWidgetHostViewBase* rwhvb = GetRenderWidgetHostView();
    ASSERT_TRUE(rwhvb);

    // Start with the widget hidden.
    rwhvb->Hide();

#if BUILDFLAG(IS_MAC)
    // On Mac, DelegatedFrameHost only behaves the same as on other platforms
    // when it has no parent UI layer.
    ASSERT_FALSE(
        GetBrowserCompositor()->DelegatedFrameHostGetLayer()->parent());
#endif
  }

  // Set a VisibleTimeRequest that will be sent the first time the widget
  // becomes visible. The default parameters request a tab switch measurement.
  void CreateVisibleTimeRequest(bool show_reason_tab_switching = true,
                                bool show_reason_bfcache_restore = false) {
    if (show_reason_bfcache_restore) {
      GetRenderWidgetHostView()->DidEnterBackForwardCache();
    }
    GetRenderWidgetHostView()
        ->host()
        ->GetVisibleTimeRequestTrigger()
        .UpdateRequest(base::TimeTicks::Now(), /*destination_is_loaded=*/true,
                       show_reason_tab_switching, show_reason_bfcache_restore);
  }

  void ExpectPresentationFeedback(TabSwitchResult expected_result) {
    // Wait for the expected result (only) to be logged.
    const base::TimeTicks start_time = base::TimeTicks::Now();
    while (histogram_tester_.GetAllSamples("Browser.Tabs.TabSwitchResult3")
               .empty()) {
      ASSERT_LT(base::TimeTicks::Now() - start_time,
                TestTimeouts::action_timeout())
          << "Timed out waiting for Browser.Tabs.TabSwitchResult3.";
      GiveItSomeTime();
    }
    histogram_tester_.ExpectUniqueSample("Browser.Tabs.TabSwitchResult3",
                                         expected_result, 1);
  }

  void ExpectNoPresentationFeedback() {
    const base::TimeTicks start_time = base::TimeTicks::Now();
    // The full action_timeout is excessively long when expecting nothing to be
    // logged.
    while (base::TimeTicks::Now() - start_time < base::Seconds(1)) {
      GiveItSomeTime();
      ASSERT_TRUE(
          histogram_tester_.GetAllSamples("Browser.Tabs.TabSwitchResult3")
              .empty());
    }
  }

#if BUILDFLAG(IS_MAC)
  // Helpers for parent layer tests.

  // Holds a ui::Layer with its own compositor to be set as parent layer during
  // tests. This must be destroyed before tearing down the test harness so that
  // the ContentBrowserTest environment doesn't have any references to the
  // ui::Layer during destruction.
  class ScopedParentLayer {
   public:
    ScopedParentLayer(BrowserCompositorMac* browser_compositor)
        : browser_compositor_(browser_compositor) {
      recyclable_compositor_ = std::make_unique<ui::RecyclableCompositorMac>(
          content::GetContextFactory());
      layer_.SetCompositorForTesting(recyclable_compositor_->compositor());
    }

    ~ScopedParentLayer() {
      browser_compositor_->SetParentUiLayer(nullptr);
      layer_.ResetCompositor();
      recyclable_compositor_.reset();
    }

    ui::Layer* layer() { return &layer_; }

   private:
    raw_ptr<BrowserCompositorMac> browser_compositor_;
    ui::Layer layer_{ui::LAYER_SOLID_COLOR};
    std::unique_ptr<ui::RecyclableCompositorMac> recyclable_compositor_;
  };

  BrowserCompositorMac* GetBrowserCompositor() const {
    return GetBrowserCompositorMacForTesting(GetRenderWidgetHostView());
  }
#endif

  base::HistogramTester histogram_tester_;
};

IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewPresentationFeedbackBrowserTest,
                       Show) {
  CreateVisibleTimeRequest();
  GetRenderWidgetHostView()->ShowWithVisibility(PageVisibilityState::kVisible);
  ExpectPresentationFeedback(TabSwitchResult::kSuccess);
}

IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewPresentationFeedbackBrowserTest,
                       ShowThenHide) {
  // An incomplete tab switch is logged when the widget is hidden before
  // presenting a frame.
  CreateVisibleTimeRequest();
  GetRenderWidgetHostView()->ShowWithVisibility(PageVisibilityState::kVisible);
  GetRenderWidgetHostView()->Hide();
  ExpectPresentationFeedback(TabSwitchResult::kIncomplete);
}

IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewPresentationFeedbackBrowserTest,
                       HiddenButPainting) {
  // Browser.Tabs.* is not logged if the page becomes "visible" due to a hidden
  // capturer.
  CreateVisibleTimeRequest();
  GetRenderWidgetHostView()->ShowWithVisibility(
      PageVisibilityState::kHiddenButPainting);
  ExpectNoPresentationFeedback();
}

IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewPresentationFeedbackBrowserTest,
                       ShowWhileCapturing) {
  // Frame is captured and then becomes visible.
  CreateVisibleTimeRequest();
  GetRenderWidgetHostView()->ShowWithVisibility(
      PageVisibilityState::kHiddenButPainting);
  GetRenderWidgetHostView()->ShowWithVisibility(PageVisibilityState::kVisible);
  ExpectPresentationFeedback(TabSwitchResult::kSuccess);
}

IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewPresentationFeedbackBrowserTest,
                       HideWhileCapturing) {
  // Capture starts and frame becomes "hidden" before a render frame is
  // presented.
  CreateVisibleTimeRequest();
  GetRenderWidgetHostView()->ShowWithVisibility(PageVisibilityState::kVisible);
  GetRenderWidgetHostView()->ShowWithVisibility(
      PageVisibilityState::kHiddenButPainting);
  ExpectPresentationFeedback(TabSwitchResult::kIncomplete);
}

IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewPresentationFeedbackBrowserTest,
                       ShowWithoutTabSwitchRequest) {
  CreateVisibleTimeRequest(/*show_reason_tab_switching=*/false,
                           /*show_reason_bfcache_restore=*/true);
  // Browser.Tabs.* is not logged if not requested.
  GetRenderWidgetHostView()->ShowWithVisibility(PageVisibilityState::kVisible);
  ExpectNoPresentationFeedback();
}

IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewPresentationFeedbackBrowserTest,
                       ShowThenHideWithoutTabSwitchRequest) {
  CreateVisibleTimeRequest(/*show_reason_tab_switching=*/false,
                           /*show_reason_bfcache_restore=*/true);
  // Browser.Tabs.* is not logged if not requested.
  GetRenderWidgetHostView()->ShowWithVisibility(PageVisibilityState::kVisible);
  GetRenderWidgetHostView()->Hide();
  ExpectNoPresentationFeedback();
}

#if BUILDFLAG(IS_MAC)

// The default tests do not set a parent UI layer, so the BrowserCompositorMac
// state is always HasNoCompositor when the RWHV is hidden, or HasOwnCompositor
// when the RWHV is visible. These tests add a parent layer to make sure that
// presentation feedback is logged when the state is UseParentLayerCompositor.

// TODO(https://crbug.com/1164477): These tests don't match the behaviour of the
// browser. In production the Browser.Tabs.* histograms are logged but in this
// test, the presentation time request is swallowed during the
// UseParentLayerCompositor state. Need to find out what's wrong with the test
// setup.

IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewPresentationFeedbackBrowserTest,
                       DISABLED_ShowWithParentLayer) {
  CreateVisibleTimeRequest();
  ScopedParentLayer parent_layer(GetBrowserCompositor());
  GetBrowserCompositor()->SetParentUiLayer(parent_layer.layer());
  GetRenderWidgetHostView()->ShowWithVisibility(PageVisibilityState::kVisible);
  ExpectPresentationFeedback(TabSwitchResult::kSuccess);
}

IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewPresentationFeedbackBrowserTest,
                       DISABLED_ShowThenAddParentLayer) {
  CreateVisibleTimeRequest();
  GetRenderWidgetHostView()->ShowWithVisibility(PageVisibilityState::kVisible);
  ScopedParentLayer parent_layer(GetBrowserCompositor());
  GetBrowserCompositor()->SetParentUiLayer(parent_layer.layer());
  ExpectPresentationFeedback(TabSwitchResult::kSuccess);
}

IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewPresentationFeedbackBrowserTest,
                       DISABLED_ShowThenRemoveParentLayer) {
  CreateVisibleTimeRequest();
  ScopedParentLayer parent_layer(GetBrowserCompositor());
  GetBrowserCompositor()->SetParentUiLayer(parent_layer.layer());
  GetRenderWidgetHostView()->ShowWithVisibility(PageVisibilityState::kVisible);
  GetBrowserCompositor()->SetParentUiLayer(nullptr);
  ExpectPresentationFeedback(TabSwitchResult::kSuccess);
}

#endif  // BUILDFLAG(IS_MAC)

#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
void CheckSurfaceRangeRemovedAfterCopy(viz::SurfaceRange range,
                                       CompositorImpl* compositor,
                                       base::RepeatingClosure resume_test,
                                       const SkBitmap& btimap) {
  ASSERT_FALSE(!compositor->GetLayerTreeForTesting()
                    ->GetSurfaceRangesForTesting()
                    .contains(range));
  std::move(resume_test).Run();
}

class RenderWidgetHostViewCopyFromSurfaceBrowserTest
    : public RenderWidgetHostViewBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  RenderWidgetHostViewCopyFromSurfaceBrowserTest() {
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(features::kSlimCompositor);
    } else {
      scoped_feature_list_.InitAndDisableFeature(features::kSlimCompositor);
    }

    // Enable `RenderDocument` to guarantee renderer/RFH swap for cross-site
    // navigations.
    InitAndEnableRenderDocumentFeature(&scoped_feature_list_render_document_,
                                       RenderDocumentFeatureFullyEnabled()[0]);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromSourceDirectory(
        GetTestDataFilePath());
    net::test_server::RegisterDefaultHandlers(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
    RenderWidgetHostViewBrowserTest::SetUpOnMainThread();
  }

  ~RenderWidgetHostViewCopyFromSurfaceBrowserTest() override = default;

  bool SetUpSourceSurface(const char* wait_message) override { return false; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::ScopedFeatureList scoped_feature_list_render_document_;
};

IN_PROC_BROWSER_TEST_P(RenderWidgetHostViewCopyFromSurfaceBrowserTest,
                       AsyncCopyFromSurface) {
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/empty.html")));

  auto* rwhv_android = static_cast<RenderWidgetHostViewAndroid*>(
      GetRenderViewHost()->GetWidget()->GetView());
  auto* compositor = static_cast<CompositorImpl*>(
      rwhv_android->GetNativeView()->GetWindowAndroid()->GetCompositor());

  const viz::SurfaceRange range_for_copy(rwhv_android->GetCurrentSurfaceId(),
                                         rwhv_android->GetCurrentSurfaceId());
  const viz::SurfaceRange range_for_mainframe(
      absl::nullopt, rwhv_android->GetCurrentSurfaceId());
  base::RunLoop run_loop;
  GetRenderViewHost()->GetWidget()->GetView()->CopyFromSurface(
      gfx::Rect(), gfx::Size(),
      base::BindOnce(&CheckSurfaceRangeRemovedAfterCopy, range_for_copy,
                     compositor, run_loop.QuitClosure()));
  EXPECT_THAT(
      compositor->GetLayerTreeForTesting()->GetSurfaceRangesForTesting(),
      testing::UnorderedElementsAre(std::make_pair(range_for_copy, 1),
                                    std::make_pair(range_for_mainframe, 1)));
  run_loop.Run(FROM_HERE);
}

namespace {

void AssertSnapshotIsPureWhite(base::RepeatingClosure resume_test,
                               const SkBitmap& snapshot) {
  for (int r = 0; r < snapshot.height(); ++r) {
    for (int c = 0; c < snapshot.width(); ++c) {
      ASSERT_EQ(snapshot.getColor(c, r), SK_ColorWHITE);
    }
  }
  std::move(resume_test).Run();
}

void WaitForSurfaceAvailableForCopy(WebContents* web_contents) {
  {
    MainThreadFrameObserver obs(
        web_contents->GetRenderWidgetHostView()->GetRenderWidgetHost());
    obs.Wait();
  }
  // `InsertVisualStateCallback` replies when a CompositorFrame is submitted.
  // However, we want to wait until the Viz process has received the new
  // `CompositorFrame` so that the new frame is available for copy. Waiting for
  // a second frame to be submitted guarantees this, since the second frame
  // cannot be sent until the first frame was ACKed by Viz.
  {
    MainThreadFrameObserver obs(
        web_contents->GetRenderWidgetHostView()->GetRenderWidgetHost());
    obs.Wait();
  }
  ASSERT_TRUE(
      web_contents->GetRenderWidgetHostView()->IsSurfaceAvailableForCopy());
}

class ScopedSnapshotWaiter : public WebContentsObserver {
 public:
  ScopedSnapshotWaiter(WebContents* wc, const GURL& destination)
      : WebContentsObserver(wc), destination_(destination) {}

  ScopedSnapshotWaiter(const ScopedSnapshotWaiter&) = delete;
  ScopedSnapshotWaiter& operator=(const ScopedSnapshotWaiter&) = delete;
  ~ScopedSnapshotWaiter() override = default;

  void Wait() { run_loop_.Run(); }

 private:
  void DidStartNavigation(NavigationHandle* handle) override {
    if (handle->GetURL() != destination_) {
      return;
    }

    auto* request = NavigationRequest::From(handle);
    request->set_ready_to_commit_callback_for_testing(base::BindOnce(
        [](RenderWidgetHostView* old_view,
           base::OnceCallback<bool()> renderer_swapped,
           base::RepeatingClosure resume) {
          ASSERT_TRUE(std::move(renderer_swapped).Run());
          ASSERT_TRUE(old_view);
          static_cast<RenderWidgetHostViewBase*>(old_view)
              ->CopyFromExactSurface(gfx::Rect(), gfx::Size(),
                                     base::BindOnce(&AssertSnapshotIsPureWhite,
                                                    std::move(resume)));
        },
        request->frame_tree_node()->current_frame_host()->GetView(),
        // The request must outlive its own callback.
        base::BindOnce(
            base::BindLambdaForTesting([](NavigationRequest* request) {
              return request->GetRenderFrameHost() !=
                     request->frame_tree_node()
                         ->render_manager()
                         ->current_frame_host();
            }),
            base::Unretained(request)),
        run_loop_.QuitClosure()));
  }

  const GURL destination_;
  base::RunLoop run_loop_;
};
}  // namespace

// A "best effort" browser test: issue an exact `CopyOutputRequest` during a
// cross-renderer navigation, when the navigation is about to commit in the
// browser. We should always be able to get a desired snapshot back.
IN_PROC_BROWSER_TEST_P(RenderWidgetHostViewCopyFromSurfaceBrowserTest,
                       CopyExactSurfaceDuringCrossRendererNavigations) {
  ASSERT_TRUE(
      NavigateToURL(shell()->web_contents(),
                    embedded_test_server()->GetURL("a.com", "/empty.html")));
  // Makes sure "empty.html" is in a steady state and ready to be copied.
  WaitForSurfaceAvailableForCopy(shell()->web_contents());

  const auto cross_renderer_url =
      embedded_test_server()->GetURL("b.com", "/title1.html");
  ScopedSnapshotWaiter waiter(shell()->web_contents(), cross_renderer_url);
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(), cross_renderer_url));
  // Force the new renderer for "title1.html" to submit a new compositor frame
  // and ack by viz, such that our `CopyOutputRequest` is fulfilled.
  WaitForSurfaceAvailableForCopy(shell()->web_contents());
  // Blocks until we get the desired snapshot of "empty.html".
  waiter.Wait();
}

INSTANTIATE_TEST_SUITE_P(EnableDisableSlim,
                         RenderWidgetHostViewCopyFromSurfaceBrowserTest,
                         ::testing::Bool());
#endif

}  // namespace content
