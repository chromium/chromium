// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_RENDER_VIEW_HOST_H_
#define CONTENT_TEST_TEST_RENDER_VIEW_HOST_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/input/cursor_manager.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/host/host_frame_sink_client.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"
#include "content/public/common/page_visibility_state.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_renderer_host.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/ime/dummy_text_input_client.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/gfx/geometry/vector2d_f.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "third_party/blink/public/mojom/webshare/webshare.mojom.h"
#endif

// This file provides a testing framework for mocking out the RenderProcessHost
// layer. It allows you to test RenderViewHost, WebContentsImpl,
// NavigationController, and other layers above that without running an actual
// renderer process.
//
// To use, derive your test base class from RenderViewHostImplTestHarness.

namespace gfx {
class Rect;
}

namespace content {

class FrameTree;
class TestRenderFrameHost;
class TestPageBroadcast;
class TestWebContents;

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
  void Hide() override;
  bool IsShowing() override;
  void WasUnOccluded() override;
  void WasOccluded() override;
  gfx::Rect GetViewBounds() override;
#if BUILDFLAG(IS_MAC)
  void SetActive(bool active) override;
  void ShowDefinitionForSelection() override {}
  void SpeakSelection() override;
  void SetWindowFrameInScreen(const gfx::Rect& rect) override;
  void ShowSharePicker(
      const std::string& title,
      const std::string& text,
      const std::string& url,
      const std::vector<std::string>& file_paths,
      blink::mojom::ShareService::ShareCallback callback) override;
  uint64_t GetNSViewId() const override;
#endif  // BUILDFLAG(IS_MAC)

  // Notified in response to a CommitPending where there is no content for
  // TakeFallbackContentFrom to use.
  void ClearFallbackSurfaceForCommitPending() override;
  // Advances the fallback surface to the first surface after navigation. This
  // ensures that stale surfaces are not presented to the user for an indefinite
  // period of time.
  void ResetFallbackToFirstNavigationSurface() override {}

  void TakeFallbackContentFrom(RenderWidgetHostView* view) override;
  void EnsureSurfaceSynchronizedForWebTest() override;

  // RenderWidgetHostViewBase:
  uint32_t GetCaptureSequenceNumber() const override;
  void InitAsPopup(RenderWidgetHostView* parent_host_view,
                   const gfx::Rect& bounds,
                   const gfx::Rect& anchor_rect) override {}
  void Focus() override {}
  void SetIsLoading(bool is_loading) override {}
  void UpdateCursor(const ui::Cursor& cursor) override;
  void RenderProcessGone() override;
  void ShowWithVisibility(PageVisibilityState page_visibility) override;
  void Destroy() override;
  void UpdateTooltipUnderCursor(const std::u16string& tooltip_text) override {}
  void UpdateTooltipFromKeyboard(const std::u16string& tooltip_text,
                                 const gfx::Rect& bounds) override {}
  void ClearKeyboardTriggeredTooltip() override {}
  gfx::Rect GetBoundsInRootWindow() override;
  const viz::LocalSurfaceId& IncrementSurfaceIdForNavigation() override;
  blink::mojom::PointerLockResult LockPointer(bool) override;
  blink::mojom::PointerLockResult ChangePointerLock(bool) override;
  void UnlockPointer() override;
  const viz::FrameSinkId& GetFrameSinkId() const override;
  const viz::LocalSurfaceId& GetLocalSurfaceId() const override;
  viz::SurfaceId GetCurrentSurfaceId() const override;
  std::unique_ptr<SyntheticGestureTarget> CreateSyntheticGestureTarget()
      override;
  ui::Compositor* GetCompositor() override;
  input::CursorManager* GetCursorManager() override;
  void InvalidateLocalSurfaceIdAndAllocationGroup() override {}

  bool is_showing() const { return is_showing_; }
  bool is_occluded() const { return is_occluded_; }

  // viz::HostFrameSinkClient implementation.
  void OnFirstSurfaceActivation(const viz::SurfaceInfo& surface_info) override;
  void OnFrameTokenChanged(uint32_t frame_token,
                           base::TimeTicks activation_time) override;

  const ui::Cursor& last_cursor() const { return last_cursor_; }

  void SetCompositor(ui::Compositor* compositor) { compositor_ = compositor; }

  // Clears `clear_fallback_surface_for_commit_pending_called_` and
  // `take_fallback_content_from_called_`.
  void ClearFallbackSurfaceCalled();
  bool clear_fallback_surface_for_commit_pending_called() const {
    return clear_fallback_surface_for_commit_pending_called_;
  }

  bool take_fallback_content_from_called() const {
    return take_fallback_content_from_called_;
  }

 protected:
  // RenderWidgetHostViewBase:
  void UpdateBackgroundColor() override;
  std::optional<DisplayFeature> GetDisplayFeature() override;
  void SetDisplayFeatureForTesting(
      const DisplayFeature* display_feature) override;
  void NotifyHostAndDelegateOnWasShown(
      blink::mojom::RecordContentToVisibleTimeRequestPtr) override;
  void RequestSuccessfulPresentationTimeFromHostOrDelegate(
      blink::mojom::RecordContentToVisibleTimeRequestPtr) override;
  void CancelSuccessfulPresentationTimeRequestForHostAndDelegate() override;

  viz::FrameSinkId frame_sink_id_;

 private:
  bool is_showing_;
  bool is_occluded_;
  PageVisibilityState page_visibility_ = PageVisibilityState::kHidden;
#if !BUILDFLAG(IS_IOS)
  ui::DummyTextInputClient text_input_client_;
#endif
  ui::Cursor last_cursor_;

  // Latest capture sequence number which is incremented when the caller
  // requests surfaces be synchronized via
  // EnsureSurfaceSynchronizedForWebTest().
  uint32_t latest_capture_sequence_number_ = 0u;

  bool clear_fallback_surface_for_commit_pending_called_ = false;
  bool take_fallback_content_from_called_ = false;

#if defined(USE_AURA)
  std::unique_ptr<aura::Window> window_;
#endif

  std::optional<DisplayFeature> display_feature_;

  raw_ptr<ui::Compositor, DanglingUntriaged> compositor_ = nullptr;

  input::CursorManager cursor_manager_;
};

// TestRenderWidgetHostViewChildFrame -----------------------------------------

// Test version of RenderWidgetHostViewChildFrame to use in unit tests.
class TestRenderWidgetHostViewChildFrame
    : public RenderWidgetHostViewChildFrame {
 public:
  explicit TestRenderWidgetHostViewChildFrame(RenderWidgetHost* rwh);
  ~TestRenderWidgetHostViewChildFrame() override = default;

  blink::WebInputEvent::Type last_gesture_seen() { return last_gesture_seen_; }

  void Reset();
  void SetCompositor(ui::Compositor* compositor);
  ui::Compositor* GetCompositor() override;

 private:
  void SetBounds(const gfx::Rect& rect) override {}
  void Hide() override {}
  void SetInsets(const gfx::Insets& insets) override {}

  void SendInitialPropertiesIfNeeded() override {}
  void ShowWithVisibility(PageVisibilityState) override {}
  void DidNavigate() override {}

  void ProcessGestureEvent(const blink::WebGestureEvent& event,
                           const ui::LatencyInfo&) override;

  blink::WebInputEvent::Type last_gesture_seen_ =
      blink::WebInputEvent::Type::kUndefined;
  raw_ptr<ui::Compositor> compositor_;
};

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
class TestRenderViewHost : public RenderViewHostImpl,
                           public RenderViewHostTester {
 public:
  TestRenderViewHost(
      FrameTree* frame_tree,
      SiteInstanceGroup* group,
      const StoragePartitionConfig& storage_partition_config,
      std::unique_ptr<RenderWidgetHostImpl> widget,
      RenderViewHostDelegate* delegate,
      int32_t routing_id,
      int32_t main_frame_routing_id,
      scoped_refptr<BrowsingContextState> main_browsing_context_state,
      CreateRenderViewHostCase create_case);

  TestRenderViewHost(const TestRenderViewHost&) = delete;
  TestRenderViewHost& operator=(const TestRenderViewHost&) = delete;

  // RenderViewHostImpl overrides.
  MockRenderProcessHost* GetProcess() const override;
  bool CreateRenderView(
      const std::optional<blink::FrameToken>& opener_frame_token,
      int proxy_route_id,
      bool window_was_created_with_opener) override;
  bool IsTestRenderViewHost() const override;

  // RenderViewHostTester implementation.
  void SimulateWasHidden() override;
  void SimulateWasShown() override;
  blink::web_pref::WebPreferences TestComputeWebPreferences() override;
  bool CreateTestRenderView() override;

  void TestOnUpdateStateWithFile(const base::FilePath& file_path);

  void TestStartDragging(const DropData& drop_data, SkBitmap bitmap = {});

  // If set, *delete_counter is incremented when this object destructs.
  void set_delete_counter(int* delete_counter) {
    delete_counter_ = delete_counter;
  }

  // The opener frame route id passed to CreateRenderView().
  const std::optional<blink::FrameToken>& opener_frame_token() const {
    return opener_frame_token_;
  }

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
  raw_ptr<int> delete_counter_;

  // See opener_frame_token() above.
  std::optional<blink::FrameToken> opener_frame_token_;

  std::unique_ptr<TestPageBroadcast> page_broadcast_;
};

// Adds methods to get straight at the impl classes.
class RenderViewHostImplTestHarness : public RenderViewHostTestHarness {
 public:
  RenderViewHostImplTestHarness();

  RenderViewHostImplTestHarness(const RenderViewHostImplTestHarness&) = delete;
  RenderViewHostImplTestHarness& operator=(
      const RenderViewHostImplTestHarness&) = delete;

  ~RenderViewHostImplTestHarness() override;

  // contents() is equivalent to static_cast<TestWebContents*>(web_contents())
  TestWebContents* contents();

  // RVH/RFH getters are shorthand for oft-used bits of web_contents().

  // test_rvh() is equivalent to any of the following:
  //   contents()->GetPrimaryMainFrame()->GetRenderViewHost()
  //   contents()->GetRenderViewHost()
  //   static_cast<TestRenderViewHost*>(rvh())
  //
  // Since most functionality will eventually shift from RVH to RFH, you may
  // prefer to use the GetPrimaryMainFrame() method in tests.
  TestRenderViewHost* test_rvh();

  // main_test_rfh() is equivalent to contents()->GetPrimaryMainFrame()
  // TODO(nick): Replace all uses with contents()->GetPrimaryMainFrame()
  TestRenderFrameHost* main_test_rfh();

 private:
  ui::test::ScopedSetSupportedResourceScaleFactors
      scoped_set_supported_scale_factors_{{ui::k100Percent}};
};

}  // namespace content

#endif  // CONTENT_TEST_TEST_RENDER_VIEW_HOST_H_
