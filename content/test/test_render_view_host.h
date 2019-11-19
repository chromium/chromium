// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_RENDER_VIEW_HOST_H_
#define CONTENT_TEST_TEST_RENDER_VIEW_HOST_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/host/host_frame_sink_client.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/public/common/web_preferences.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_renderer_host.h"
#include "ui/base/ime/dummy_text_input_client.h"
#include "ui/base/layout.h"
#include "ui/base/page_transition_types.h"
#include "ui/gfx/geometry/vector2d_f.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

// This file provides a testing framework for mocking out the RenderProcessHost
// layer. It allows you to test RenderViewHost, WebContentsImpl,
// NavigationController, and other layers above that without running an actual
// renderer process.
//
// To use, derive your test base class from RenderViewHostImplTestHarness.

struct FrameHostMsg_DidCommitProvisionalLoad_Params;

namespace gfx {
class Rect;
}

namespace content {

class SiteInstance;
class TestRenderFrameHost;
class TestWebContents;
struct FrameReplicationState;

// Utility function to initialize FrameHostMsg_DidCommitProvisionalLoad_Params
// with given parameters.
void InitNavigateParams(FrameHostMsg_DidCommitProvisionalLoad_Params* params,
                        int nav_entry_id,
                        bool did_create_new_entry,
                        const GURL& url,
                        ui::PageTransition transition_type);

// TestRenderWidgetHostView ----------------------------------------------------

// Subclass the RenderViewHost's view so that we can call Show(), etc.,
// without having side-effects.
class TestRenderWidgetHostView : public RenderWidgetHostViewBase,
                                 public viz::HostFrameSinkClient {
 public:
  explicit TestRenderWidgetHostView(RenderWidgetHost* rwh);
  ~TestRenderWidgetHostView() override;

  // RenderWidgetHostView:
  void InitAsChild(gfx::NativeView parent_view) override {}
  void SetSize(const gfx::Size& size) override {}
  void SetBounds(const gfx::Rect& rect) override {}
  gfx::NativeView GetNativeView() override;
  gfx::NativeViewAccessible GetNativeViewAccessible() override;
  ui::TextInputClient* GetTextInputClient() override;
  bool HasFocus() override;
  void Show() override;
  void Hide() override;
  bool IsShowing() override;
  void WasUnOccluded() override;
  void WasOccluded() override;
  gfx::Rect GetViewBounds() override;
#if defined(OS_MACOSX)
  void SetActive(bool active) override;
  void ShowDefinitionForSelection() override {}
  void SpeakSelection() override;
#endif  // defined(OS_MACOSX)
  void DidCreateNewRendererCompositorFrameSink(
      viz::mojom::CompositorFrameSinkClient* renderer_compositor_frame_sink)
      override;
  void SubmitCompositorFrame(
      const viz::LocalSurfaceId& local_surface_id,
      viz::CompositorFrame frame,
      base::Optional<viz::HitTestRegionList> hit_test_region_list) override;

  // Advances the fallback surface to the first surface after navigation. This
  // ensures that stale surfaces are not presented to the user for an indefinite
  // period of time.
  void ResetFallbackToFirstNavigationSurface() override {}

  void SetNeedsBeginFrames(bool needs_begin_frames) override {}
  void SetWantsAnimateOnlyBeginFrames() override {}
  void TakeFallbackContentFrom(RenderWidgetHostView* view) override;
  void EnsureSurfaceSynchronizedForWebTest() override {}

  // RenderWidgetHostViewBase:
  void InitAsPopup(RenderWidgetHostView* parent_host_view,
                   const gfx::Rect& bounds) override {}
  void InitAsFullscreen(RenderWidgetHostView* reference_host_view) override {}
  void Focus() override {}
  void SetIsLoading(bool is_loading) override {}
  void UpdateCursor(const WebCursor& cursor) override {}
  void RenderProcessGone() override;
  void Destroy() override;
  void SetTooltipText(const base::string16& tooltip_text) override {}
  gfx::Rect GetBoundsInRootWindow() override;
  bool LockMouse(bool) override;
  void UnlockMouse() override;
  const viz::FrameSinkId& GetFrameSinkId() const override;
  const viz::LocalSurfaceIdAllocation& GetLocalSurfaceIdAllocation()
      const override;
  viz::SurfaceId GetCurrentSurfaceId() const override;
  std::unique_ptr<SyntheticGestureTarget> CreateSyntheticGestureTarget()
      override;

  bool is_showing() const { return is_showing_; }
  bool is_occluded() const { return is_occluded_; }
  bool did_swap_compositor_frame() const { return did_swap_compositor_frame_; }
  void reset_did_swap_compositor_frame() { did_swap_compositor_frame_ = false; }
  bool did_change_compositor_frame_sink() {
    return did_change_compositor_frame_sink_;
  }
  void reset_did_change_compositor_frame_sink() {
    did_change_compositor_frame_sink_ = false;
  }
  // viz::HostFrameSinkClient implementation.
  void OnFirstSurfaceActivation(const viz::SurfaceInfo& surface_info) override;
  void OnFrameTokenChanged(uint32_t frame_token) override;

 protected:
  // RenderWidgetHostViewBase:
  void UpdateBackgroundColor() override;

  viz::FrameSinkId frame_sink_id_;

 private:
  bool is_showing_;
  bool is_occluded_;
  bool did_swap_compositor_frame_;
  bool did_change_compositor_frame_sink_ = false;
  ui::DummyTextInputClient text_input_client_;

#if defined(USE_AURA)
  std::unique_ptr<aura::Window> window_;
#endif
};

#if defined(COMPILER_MSVC)
// See comment for same warning on RenderViewHostImpl.
#pragma warning(push)
#pragma warning(disable: 4250)
#endif

// TestRenderViewHost ----------------------------------------------------------

// TODO(brettw) this should use a TestWebContents which should be generalized
// from the WebContentsImpl test. We will probably also need that class' version
// of CreateRenderViewForRenderManager when more complicated tests start using
// this.
//
// Note that users outside of content must use this class by getting
// the separate RenderViewHostTester interface via
// RenderViewHostTester::For(rvh) on the RenderViewHost they want to
// drive tests on.
//
// Users within content may directly static_cast from a
// RenderViewHost* to a TestRenderViewHost*.
//
// The reasons we do it this way rather than extending the parallel
// inheritance hierarchy we have for RenderWidgetHost/RenderViewHost
// vs. RenderWidgetHostImpl/RenderViewHostImpl are:
//
// a) Extending the parallel class hierarchy further would require
// more classes to use virtual inheritance.  This is a complexity that
// is better to avoid, especially when it would be introduced in the
// production code solely to facilitate testing code.
//
// b) While users outside of content only need to drive tests on a
// RenderViewHost, content needs a test version of the full
// RenderViewHostImpl so that it can test all methods on that concrete
// class (e.g. overriding a method such as
// RenderViewHostImpl::CreateRenderView).  This would have complicated
// the dual class hierarchy even further.
//
// The reason we do it this way instead of using composition is
// similar to (b) above, essentially it gets very tricky.  By using
// the split interface we avoid complexity within content and maintain
// reasonable utility for embedders.
class TestRenderViewHost
    : public RenderViewHostImpl,
      public RenderViewHostTester {
 public:
  TestRenderViewHost(SiteInstance* instance,
                     std::unique_ptr<RenderWidgetHostImpl> widget,
                     RenderViewHostDelegate* delegate,
                     int32_t routing_id,
                     int32_t main_frame_routing_id,
                     bool swapped_out);
  // RenderViewHostTester implementation.  Note that CreateRenderView
  // is not specified since it is synonymous with the one from
  // RenderViewHostImpl, see below.
  void SimulateWasHidden() override;
  void SimulateWasShown() override;
  WebPreferences TestComputeWebPreferences() override;

  void TestOnUpdateStateWithFile(const base::FilePath& file_path);

  void TestOnStartDragging(const DropData& drop_data);

  // If set, *delete_counter is incremented when this object destructs.
  void set_delete_counter(int* delete_counter) {
    delete_counter_ = delete_counter;
  }

  // If set, *webkit_preferences_changed_counter is incremented when
  // OnWebkitPreferencesChanged() is called.
  void set_webkit_preferences_changed_counter(int* counter) {
    webkit_preferences_changed_counter_ = counter;
  }

  // The opener frame route id passed to CreateRenderView().
  int opener_frame_route_id() const { return opener_frame_route_id_; }

  // RenderWidgetHost overrides (same value, but in the Mock* type)
  MockRenderProcessHost* GetProcess() override;

  bool CreateTestRenderView(const base::string16& frame_name,
                            int opener_frame_route_id,
                            int proxy_route_id,
                            bool window_was_created_with_opener) override;

  // RenderViewHost:
  bool CreateRenderView(int opener_frame_route_id,
                        int proxy_route_id,
                        const base::UnguessableToken& devtools_frame_token,
                        const FrameReplicationState& replicated_frame_state,
                        bool window_was_created_with_opener) override;
  void OnWebkitPreferencesChanged() override;

  // RenderViewHostImpl:
  bool IsTestRenderViewHost() const override;

 private:
  FRIEND_TEST_ALL_PREFIXES(RenderViewHostTest, FilterNavigate);

  ~TestRenderViewHost() override;

  void SendNavigateWithTransitionAndResponseCode(const GURL& url,
                                                 ui::PageTransition transition,
                                                 int response_code);

  // Calls OnNavigate on the RenderViewHost with the given information.
  // Sets the rest of the parameters in the message to the "typical" values.
  // This is a helper function for simulating the most common types of loads.
  void SendNavigateWithParameters(
      const GURL& url,
      ui::PageTransition transition,
      const GURL& original_request_url,
      int response_code,
      const base::FilePath* file_path_for_history_item);

  // See set_delete_counter() above. May be NULL.
  int* delete_counter_;

  // See set_webkit_preferences_changed_counter() above. May be NULL.
  int* webkit_preferences_changed_counter_;

  // See opener_frame_route_id() above.
  int opener_frame_route_id_;

  DISALLOW_COPY_AND_ASSIGN(TestRenderViewHost);
};

#if defined(COMPILER_MSVC)
#pragma warning(pop)
#endif

// Adds methods to get straight at the impl classes.
class RenderViewHostImplTestHarness : public RenderViewHostTestHarness {
 public:
  RenderViewHostImplTestHarness();
  ~RenderViewHostImplTestHarness() override;

  // contents() is equivalent to static_cast<TestWebContents*>(web_contents())
  TestWebContents* contents();

  // RVH/RFH getters are shorthand for oft-used bits of web_contents().

  // test_rvh() is equivalent to any of the following:
  //   contents()->GetMainFrame()->GetRenderViewHost()
  //   contents()->GetRenderViewHost()
  //   static_cast<TestRenderViewHost*>(rvh())
  //
  // Since most functionality will eventually shift from RVH to RFH, you may
  // prefer to use the GetMainFrame() method in tests.
  TestRenderViewHost* test_rvh();

  // pending_test_rvh() is equivalent to all of the following:
  //   contents()->GetPendingMainFrame()->GetRenderViewHost() [if frame exists]
  //   contents()->GetPendingRenderViewHost()
  //   static_cast<TestRenderViewHost*>(pending_rvh())
  //
  // Since most functionality will eventually shift from RVH to RFH, you may
  // prefer to use the GetPendingMainFrame() method in tests.
  TestRenderViewHost* pending_test_rvh();

  // active_test_rvh() is equivalent to:
  //   contents()->GetPendingRenderViewHost() ?
  //        contents()->GetPendingRenderViewHost() :
  //        contents()->GetRenderViewHost();
  TestRenderViewHost* active_test_rvh();

  // main_test_rfh() is equivalent to contents()->GetMainFrame()
  // TODO(nick): Replace all uses with contents()->GetMainFrame()
  TestRenderFrameHost* main_test_rfh();

 private:
  typedef std::unique_ptr<ui::test::ScopedSetSupportedScaleFactors>
      ScopedSetSupportedScaleFactors;
  ScopedSetSupportedScaleFactors scoped_set_supported_scale_factors_;
  DISALLOW_COPY_AND_ASSIGN(RenderViewHostImplTestHarness);
};

}  // namespace content

#endif  // CONTENT_TEST_TEST_RENDER_VIEW_HOST_H_
