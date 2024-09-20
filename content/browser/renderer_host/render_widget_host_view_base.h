// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_BASE_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_BASE_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/process/kill.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/input/event_with_latency_info.h"
#include "components/input/input_router_impl.h"
#include "components/input/render_input_router.h"
#include "components/input/render_widget_host_view_input.h"
#include "components/viz/common/hit_test/hit_test_query.h"
#include "components/viz/common/surfaces/scoped_surface_id_allocator.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "content/browser/renderer_host/display_feature.h"
#include "content/common/content_export.h"
#include "content/public/browser/render_frame_metadata_provider.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/common/page_visibility_state.h"
#include "content/public/common/widget_type.h"
#include "services/device/public/mojom/screen_orientation_lock_types.mojom.h"
#include "services/viz/public/mojom/hit_test/hit_test_region_list.mojom.h"
#include "third_party/blink/public/mojom/frame/fullscreen.mojom.h"
#include "third_party/blink/public/mojom/frame/intrinsic_sizing_info.mojom-forward.h"
#include "third_party/blink/public/mojom/input/input_event_result.mojom-shared.h"
#include "third_party/blink/public/mojom/widget/record_content_to_visible_time_request.mojom.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/accessibility/ax_action_handler_registry.h"
#include "ui/base/ime/mojom/text_input_state.mojom-forward.h"
#include "ui/base/ime/text_input_mode.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/display/display.h"
#include "ui/display/screen_infos.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/range/range.h"
#include "ui/surface/transport_dib.h"

namespace blink {
class WebMouseEvent;
class WebMouseWheelEvent;
}

namespace ui {
class Compositor;
class Cursor;
class LatencyInfo;
enum class DomCode : uint32_t;
}  // namespace ui

namespace content {

class DevicePosturePlatformProvider;
class MouseWheelPhaseHandler;
class RenderWidgetHostImpl;
class ScopedViewTransitionResources;
class TextInputManager;
class TouchSelectionControllerClientManager;
class WebContentsAccessibility;
class DelegatedFrameHost;
class SyntheticGestureTarget;

// Basic implementation shared by concrete RenderWidgetHostView subclasses.
class CONTENT_EXPORT RenderWidgetHostViewBase
    : public RenderWidgetHostView,
      public input::RenderWidgetHostViewInput {
 public:
  // The TooltipObserver is used in browser tests only.
  class CONTENT_EXPORT TooltipObserver {
   public:
    virtual ~TooltipObserver() = default;

    virtual void OnTooltipTextUpdated(const std::u16string& tooltip_text) = 0;
  };

  RenderWidgetHostViewBase(const RenderWidgetHostViewBase&) = delete;
  RenderWidgetHostViewBase& operator=(const RenderWidgetHostViewBase&) = delete;

  // Returns the focused RenderWidgetHost inside this |view|'s RWH.
  RenderWidgetHostImpl* GetFocusedWidget() const;

  // Create a platform specific SyntheticGestureTarget implementation that will
  // be used to inject synthetic input events.
  virtual std::unique_ptr<SyntheticGestureTarget>
  CreateSyntheticGestureTarget() = 0;

  // RenderWidgetHostView implementation.
  RenderWidgetHost* GetRenderWidgetHost() final;
  ui::TextInputClient* GetTextInputClient() override;
  void Show() final;
  void WasUnOccluded() override {}
  void WasOccluded() override {}
  std::u16string GetSelectedText() override;
  bool GetIsPointerLockedUnadjustedMovementForTesting() override;
  bool CanBePointerLocked() override;
  bool AccessibilityHasFocus() override;
  bool LockKeyboard(std::optional<base::flat_set<ui::DomCode>> codes) override;
  void SetBackgroundColor(SkColor color) override;
  std::optional<SkColor> GetBackgroundColor() override;
  void CopyBackgroundColorIfPresentFrom(
      const RenderWidgetHostView& other) override;
  void UnlockKeyboard() override;
  bool IsKeyboardLocked() override;
  base::flat_map<std::string, std::string> GetKeyboardLayoutMap() override;
  gfx::Size GetVisibleViewportSize() override;
  void SetInsets(const gfx::Insets& insets) override;
  bool IsSurfaceAvailableForCopy() override;
  void CopyFromSurface(
      const gfx::Rect& src_rect,
      const gfx::Size& output_size,
      base::OnceCallback<void(const SkBitmap&)> callback) override;
  std::unique_ptr<viz::ClientFrameSinkVideoCapturer> CreateVideoCapturer()
      override;
  display::ScreenInfo GetScreenInfo() const override;
  display::ScreenInfos GetScreenInfos() const override;
  virtual void ResetGestureDetection();

  // RenderWidgetHostViewInput implementation
  base::WeakPtr<input::RenderWidgetHostViewInput> GetInputWeakPtr() override;
  input::RenderInputRouter* GetViewRenderInputRouter() override;
  void ProcessMouseEvent(const blink::WebMouseEvent& event,
                         const ui::LatencyInfo& latency) override;
  void ProcessMouseWheelEvent(const blink::WebMouseWheelEvent& event,
                              const ui::LatencyInfo& latency) override;
  void ProcessTouchEvent(const blink::WebTouchEvent& event,
                         const ui::LatencyInfo& latency) override;
  void ProcessGestureEvent(const blink::WebGestureEvent& event,
                           const ui::LatencyInfo& latency) override;
  RenderWidgetHostViewBase* GetRootView() override;
  void OnAutoscrollStart() override;
  const viz::DisplayHitTestQueryMap& GetDisplayHitTestQuery() const override;

  float GetDeviceScaleFactor() const final;
  bool IsPointerLocked() override;

  // Identical to `CopyFromSurface()`, except that this method issues the
  // `viz::CopyOutputRequest` against the exact `viz::Surface` currently
  // embedded by this View, while `CopyFromSurface()` may return a copy of any
  // Surface associated with this View, generated after the current Surface. The
  // caller is responsible for making sure that the target Surface is embedded
  // and available for copy when this API is called. This Surface can be removed
  // from the UI after this call.
  //
  // TODO(crbug.com/40276723): merge this API into `CopyFromSurface()`,
  // and enable it fully on Android.
  virtual void CopyFromExactSurface(
      const gfx::Rect& src_rect,
      const gfx::Size& output_size,
      base::OnceCallback<void(const SkBitmap&)> callback);

  // For HiDPI capture mode, allow applying a render scale multiplier
  // which modifies the effective device scale factor. Use a scale
  // of 1.0f (exactly) to disable the feature after it was used.
  void SetScaleOverrideForCapture(float scale);
  float GetScaleOverrideForCapture() const;

  void EnableAutoResize(const gfx::Size& min_size,
                        const gfx::Size& max_size) override;
  void DisableAutoResize(const gfx::Size& new_size) override;
  TouchSelectionControllerClientManager*
  GetTouchSelectionControllerClientManager() override;
  ui::mojom::VirtualKeyboardMode GetVirtualKeyboardMode() override;
  void NotifyVirtualKeyboardOverlayRect(
      const gfx::Rect& keyboard_rect) override {}
  bool IsHTMLFormPopup() const override;

  // This only needs to be overridden by RenderWidgetHostViewBase subclasses
  // that handle content embedded within other RenderWidgetHostViews.
  gfx::PointF TransformPointToRootCoordSpaceF(
      const gfx::PointF& point) override;

  // Returns the value for whether the auto-resize has been enabled or not.
  bool IsAutoResizeEnabled();

  virtual void UpdateIntrinsicSizingInfo(
      blink::mojom::IntrinsicSizingInfoPtr sizing_info);

  static void CopyMainAndPopupFromSurface(
      base::WeakPtr<RenderWidgetHostImpl> main_host,
      base::WeakPtr<DelegatedFrameHost> main_frame_host,
      base::WeakPtr<RenderWidgetHostImpl> popup_host,
      base::WeakPtr<DelegatedFrameHost> popup_frame_host,
      const gfx::Rect& src_subrect,
      const gfx::Size& dst_size,
      float scale_factor,
      base::OnceCallback<void(const SkBitmap&)> callback);

  void SetWidgetType(WidgetType widget_type);

  WidgetType GetWidgetType();

  virtual void SendInitialPropertiesIfNeeded() {}

  // Called when screen information or native widget bounds change.
  virtual void UpdateScreenInfo();

  // Called by the TextInputManager to notify the view about being removed from
  // the list of registered views, i.e., TextInputManager is no longer tracking
  // TextInputState from this view. The RWHV should reset |text_input_manager_|
  // to nullptr.
  void DidUnregisterFromTextInputManager(TextInputManager* text_input_manager);

  // Informs the view that the renderer's visual properties have been updated
  // and a new viz::LocalSurfaceId has been allocated.
  virtual viz::ScopedSurfaceIdAllocator DidUpdateVisualProperties(
      const cc::RenderFrameMetadata& metadata);

  base::WeakPtr<RenderWidgetHostViewBase> GetWeakPtr();

  //----------------------------------------------------------------------------
  // The following methods can be overridden by derived classes.

  // Notifies the View that the renderer text selection has changed.
  virtual void SelectionChanged(const std::u16string& text,
                                size_t offset,
                                const gfx::Range& range);

  // The requested size of the renderer. May differ from GetViewBounds().size()
  // when the view requires additional throttling.
  virtual gfx::Size GetRequestedRendererSize();

  // Returns the current capture sequence number.
  virtual uint32_t GetCaptureSequenceNumber() const;

  // The size of the view's backing surface in non-DPI-adjusted pixels.
  virtual gfx::Size GetCompositorViewportPixelSize();

  virtual gfx::AcceleratedWidget AccessibilityGetAcceleratedWidget();
  virtual gfx::NativeViewAccessible AccessibilityGetNativeViewAccessible();
  virtual gfx::NativeViewAccessible
  AccessibilityGetNativeViewAccessibleForWindow();
  virtual void SetMainFrameAXTreeID(ui::AXTreeID id) {}
  // Informs that the focused DOM node has changed.
  virtual void FocusedNodeChanged(bool is_editable_node,
                                  const gfx::Rect& node_bounds_in_screen) {}

  // StylusInterface overrides
  // Check if stylus writing can be started.
  bool ShouldInitiateStylusWriting() override;
  // Notify whether the hovered element action is stylus writable or not.
  void NotifyHoverActionStylusWritable(bool stylus_writable) override {}

  // Invalidates the `viz::SurfaceAllocationGroup` of this View. Also
  // invalidates `viz::SurfaceId` of it. This should be used when no previous
  // frame drawn by this view is preferred as a fallback.
  virtual void InvalidateLocalSurfaceIdAndAllocationGroup() = 0;

  // This method will clear any cached fallback surface. For use in response to
  // a CommitPending where there is no content for TakeFallbackContentFrom.
  virtual void ClearFallbackSurfaceForCommitPending() {}
  // This method will reset the fallback to the first surface after navigation.
  virtual void ResetFallbackToFirstNavigationSurface() = 0;

  // Requests a new CompositorFrame from the renderer. This is done by
  // allocating a new viz::LocalSurfaceId which forces a commit and draw.
  virtual bool RequestRepaintForTesting();

  // Subclass identifier for RenderWidgetHostViewChildFrames. This is useful
  // to be able to know if this RWHV is embedded within another RWHV. If
  // other kinds of embeddable RWHVs are created, this should be renamed to
  // a more generic term -- in which case, static casts to RWHVChildFrame will
  // need to also be resolved.
  virtual bool IsRenderWidgetHostViewChildFrame();

  // Returns true if this view's size have been initialized.
  virtual bool HasSize() const;

  // Returns true if the visual properties should be sent to the renderer at
  // this time. This function is intended for subclasses to suppress
  // synchronization, the default implementation returns true.
  virtual bool CanSynchronizeVisualProperties();

  // For an embedded widget, returns the cumulative effect of CSS zoom on the
  // embedding element (e.g. <iframe>) and its ancestors. For a top-level
  // widget, returns 1.0.
  virtual double GetCSSZoomFactor() const;

  //----------------------------------------------------------------------------
  // The following methods are related to IME.
  // TODO(ekaramad): Most of the IME methods should not stay virtual after IME
  // is implemented for OOPIF. After fixing IME, mark the corresponding methods
  // non-virtual (https://crbug.com/578168).

  // Updates the state of the input method attached to the view.
  virtual void TextInputStateChanged(
      const ui::mojom::TextInputState& text_input_state);

  // Cancel the ongoing composition of the input method attached to the view.
  virtual void ImeCancelComposition();

  // Notifies the view that the renderer selection bounds has changed.
  // Selection bounds are described as a focus bound which is the current
  // position of caret on the screen, as well as the anchor bound which is the
  // starting position of the selection. `bounding_box` is the bounds of the
  // rectangle enclosing the selection region. The coordinates are with respect
  // to RenderWidget's window's origin. Focus and anchor bound are represented
  // as gfx::Rect.
  virtual void SelectionBoundsChanged(const gfx::Rect& anchor_rect,
                                      base::i18n::TextDirection anchor_dir,
                                      const gfx::Rect& focus_rect,
                                      base::i18n::TextDirection focus_dir,
                                      const gfx::Rect& bounding_box,
                                      bool is_anchor_first);

  // Updates the range of the marked text in an IME composition, the visible
  // line bounds, or both.
  virtual void ImeCompositionRangeChanged(
      const gfx::Range& range,
      const std::optional<std::vector<gfx::Rect>>& character_bounds,
      const std::optional<std::vector<gfx::Rect>>& line_bounds);

  //----------------------------------------------------------------------------
  // The following pure virtual methods are implemented by derived classes.

  // Perform all the initialization steps necessary for this object to represent
  // a popup (such as a <select> dropdown), then shows the popup at |pos| using
  // |anchor_rect|. See OwnedWindowAnchor in //ui/base/ui_base_types.h for more
  // details.
  virtual void InitAsPopup(RenderWidgetHostView* parent_host_view,
                           const gfx::Rect& bounds,
                           const gfx::Rect& anchor_rect) = 0;

  // Indicates whether the page has finished loading.
  virtual void SetIsLoading(bool is_loading) = 0;

  // Notifies the View that the renderer has ceased to exist.
  virtual void RenderProcessGone() = 0;

  // `page_visibility` is kHiddenButPainting when the view is shown even though
  // the web contents should be hidden, e.g., because a background tab is
  // screen captured.
  virtual void ShowWithVisibility(PageVisibilityState page_visibility) = 0;

  // Tells the View to destroy itself.
  virtual void Destroy();

  // Updates the tooltip text and its position and displays the requested
  // tooltip on the screen. The |bounds| parameter corresponds to the bounds of
  // the renderer-side element (in widget-relative DIPS) on which the tooltip
  // should appear to be anchored.
  virtual void UpdateTooltipFromKeyboard(const std::u16string& tooltip_text,
                                         const gfx::Rect& bounds) {}

  // Hides tooltips that are still visible and were triggered from a keypress.
  // Doesn't impact tooltips that were triggered from the cursor.
  virtual void ClearKeyboardTriggeredTooltip() {}

  // Gets the bounds of the top-level window, in screen coordinates.
  virtual gfx::Rect GetBoundsInRootWindow() = 0;

  // Increments the LocalSurfaceId associated with this view when a commit IPC
  // is being sent to change the Document for the root RenderFrameHost rendering
  // to this view.
  // Note: Generally changing the SurfaceID is done using
  // SynchronizeVisualProperties which also sends the updated SurfaceID to the
  // renderer. However, for this API the caller is responsible for ensuring the
  // new ID is synchronized with the renderer.
  virtual const viz::LocalSurfaceId& IncrementSurfaceIdForNavigation();

  // Dispatched when the a cross-document navigation happens in the primary main
  // frame, and the old view is still visible. This API is called on the old
  // view.
  //
  // "DidNavigate" means this API (and its post commit counterpart) is part of
  // the browser's atomic "DidCommitNavigation" stack. "PreCommit" means the old
  // view is still visible and the new view is still invisible.
  //
  // The platform-specific overrides should prepare for the old view about to be
  // swapped out, which typically includes resetting the graphical states on the
  // old view.
  //
  // This API shouldn't be called for a same-doc navigations. For the cross-doc
  // navigations that don't swap the `RenderWidgetHostView`, the old and new
  // views are the same.
  virtual void OnOldViewDidNavigatePreCommit();

  // Dispatched when the new primary main frame's `RenderWidgetHostView` is
  // swapped in, and is made visible due to a cross-document navigation. This
  // API is called on the new view.
  //
  // The platform-specific overrides should prepare for the new view about to be
  // made visible, which typically includes cancelling any ongoing gesture
  // events.
  //
  // Same as its pre commit counterpart: this API shouldn't be called for
  // same-doc navigations, and the new view can be the same as the old view if
  // the navigation does not swap the `RenderWidgetHostView`.
  virtual void OnNewViewDidNavigatePostCommit();

  // Gives a chance to the caller to perform some task AFTER the page is
  // unloaded and stored in the BFCache.
  virtual void DidEnterBackForwardCache() {}

  // Called by WebContentsImpl to notify the view about a change in visibility
  // of context menu. The view can then perform platform specific tasks and
  // changes.
  virtual void SetShowingContextMenu(bool showing) {}

  // Gets the DisplayFeature whose offset and mask_length are expressed in DIPs
  // relative to the view. See display_feature.h for more details.
  virtual std::optional<DisplayFeature> GetDisplayFeature() = 0;

  virtual void SetDisplayFeatureForTesting(
      const DisplayFeature* display_feature) = 0;

  DevicePosturePlatformProvider* GetDevicePosturePlatformProvider();

  // Returns the associated RenderWidgetHostImpl.
  RenderWidgetHostImpl* host() const { return host_; }

  // Process swap messages sent before |frame_token| in RenderWidgetHostImpl.
  void OnFrameTokenChangedForView(uint32_t frame_token,
                                  base::TimeTicks activation_time);

  // Returns a reference to the current instance of TextInputManager. The
  // reference is obtained from RenderWidgetHostDelegate. The first time a non-
  // null reference is obtained, its value is cached in |text_input_manager_|
  // and this view is registered with it. The RWHV will unregister from the
  // TextInputManager if it is destroyed or if the TextInputManager itself is
  // destroyed. The unregistration of the RWHV from TextInputManager is
  // necessary and must be done by explicitly calling
  // TextInputManager::Unregister.
  // It is safer to use this method rather than directly dereferencing
  // |text_input_manager_|.
  TextInputManager* GetTextInputManager();

  virtual void DidNavigate();

  // Called when the RenderWidgetHostImpl establishes a connection to the
  // renderer process Widget.
  virtual void OnRendererWidgetCreated() {}

  virtual WebContentsAccessibility* GetWebContentsAccessibility();

  void set_is_evicted() { is_evicted_ = true; }
  void reset_is_evicted() { is_evicted_ = false; }
  bool is_evicted() { return is_evicted_; }

  // SetContentBackgroundColor is called when the renderer wants to update the
  // view's background color.
  void SetContentBackgroundColor(SkColor color);
  std::optional<SkColor> content_background_color() const {
    return content_background_color_;
  }

  void SetTooltipObserverForTesting(TooltipObserver* observer);

  virtual ui::Compositor* GetCompositor();

  virtual void EnterFullscreenMode(
      const blink::mojom::FullscreenOptions& options) {}
  virtual void ExitFullscreenMode() {}
  virtual void LockOrientation(
      device::mojom::ScreenOrientationLockType orientation) {}
  virtual void UnlockOrientation() {}
  virtual void SetHasPersistentVideo(bool has_persistent_video) {}

  bool HasFallbackSurfaceForTesting() const { return HasFallbackSurface(); }

  void SetIsFrameSinkIdOwner(bool is_owner);

  void SetViewTransitionResources(
      std::unique_ptr<ScopedViewTransitionResources> resources);
  bool HasViewTransitionResourcesForTesting() const {
    return !!view_transition_resources_;
  }

  virtual viz::SurfaceId GetFallbackSurfaceIdForTesting() const;

 protected:
  explicit RenderWidgetHostViewBase(RenderWidgetHost* host);
  ~RenderWidgetHostViewBase() override;

  bool is_frame_sink_id_owner() const { return is_frame_sink_id_owner_; }

  virtual MouseWheelPhaseHandler* GetMouseWheelPhaseHandler();

  // RenderWidgetHostViewInput implementations.
  void UpdateFrameSinkIdRegistration() override;

  // Applies background color without notifying the RenderWidget about
  // opaqueness changes. This allows us to, when navigating to a new page,
  // transfer this color to that page. This allows us to pass this background
  // color to new views on navigation.
  virtual void UpdateBackgroundColor() = 0;

  virtual bool HasFallbackSurface() const;

  // Checks the combination of RenderWidgetHostImpl hidden state and
  // `page_visibility` and calls NotifyHostAndDelegateOnWasShown,
  // RequestSuccessfulPresentationTimeFromHostOrDelegate or
  // CancelSuccessfulPresentationTimeRequestForHostAndDelegate as appropriate.
  //
  // This starts and stops tab switch latency measurements as needed so most
  // platforms should call this from ShowWithVisibility. Android does not
  // implement tab switch latency measurements so it calls
  // RenderWidgetHostImpl::WasShown and DelegatedFrameHost::WasShown directly
  // instead.
  void OnShowWithPageVisibility(PageVisibilityState page_visibility);

  void UpdateSystemCursorSize(const gfx::Size& cursor_size);

  // Updates the active state by replicating it to the renderer.
  void UpdateActiveState(bool active);

  // Each platform should override this to make sure its UI compositor is
  // visible.
  virtual void EnsurePlatformVisibility(PageVisibilityState page_visibility) {}

  // Each platform should override this to call RenderWidgetHostImpl::WasShown
  // and DelegatedFrameHost::WasShown, and do any platform-specific bookkeeping
  // needed.  The given `visible_time_request`, if any, should be passed to
  // DelegatedFrameHost::WasShown if there is a saved frame or
  // RenderWidgetHostImpl if not.
  virtual void NotifyHostAndDelegateOnWasShown(
      blink::mojom::RecordContentToVisibleTimeRequestPtr
          visible_time_request) = 0;

  // Each platform should override this to pass `visible_time_request`, which
  // will never be null, to
  // DelegatedFrameHost::RequestSuccessfulPresentationTimeForNextFrame if there
  // is a saved frame or
  // RenderWidgetHostImpl::RequestSuccessfulPresentationTimeForNextFrame if not,
  // after doing and platform-specific bookkeeping needed.
  virtual void RequestSuccessfulPresentationTimeFromHostOrDelegate(
      blink::mojom::RecordContentToVisibleTimeRequestPtr
          visible_time_request) = 0;

  // Each platform should override this to call
  // DelegatedFrameHost::CancelSuccessfulPresentationTimeRequest and
  // RenderWidgetHostImpl::CancelSuccessfulPresentationTimeRequest, after doing
  // and platform-specific bookkeeping needed.
  virtual void CancelSuccessfulPresentationTimeRequestForHostAndDelegate() = 0;

  // The model object. Access is protected to allow access to
  // RenderWidgetHostViewChildFrame.
  raw_ptr<RenderWidgetHostImpl, DanglingUntriaged> host_;

  // Whether this view is a frame or a popup.
  WidgetType widget_type_ = WidgetType::kFrame;

  // Cached information about the renderer's display environment.
  display::ScreenInfos screen_infos_;

  float scale_override_for_capture_ = 1.0f;

  // Indicates whether keyboard lock is active for this view.
  bool keyboard_locked_ = false;

  // Indicates whether the scroll offset of the root layer is at top, i.e.,
  // whether scroll_offset.y() == 0.
  bool is_scroll_offset_at_top_ = true;

  // A reference to current TextInputManager instance this RWHV is registered
  // with. This is initially nullptr until the first time the view calls
  // GetTextInputManager(). It also becomes nullptr when TextInputManager is
  // destroyed before the RWHV is destroyed.
  raw_ptr<TextInputManager> text_input_manager_ = nullptr;

  // The background color used in the current renderer.
  std::optional<SkColor> content_background_color_;

  // The default background color used before getting the
  // |content_background_color|.
  std::optional<SkColor> default_background_color_;

  raw_ptr<TooltipObserver> tooltip_observer_for_testing_ = nullptr;

  // Cursor size in logical pixels, obtained from the OS. This value is general
  // to all displays.
  gfx::Size system_cursor_size_;

 private:
  FRIEND_TEST_ALL_PREFIXES(
      BrowserSideFlingBrowserTest,
      EarlyTouchscreenFlingCancelationOnInertialGSUAckNotConsumed);
  FRIEND_TEST_ALL_PREFIXES(
      BrowserSideFlingBrowserTest,
      EarlyTouchpadFlingCancelationOnInertialGSUAckNotConsumed);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostDelegatedInkMetadataTest,
                           FlagGetsSetFromRenderFrameMetadata);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostInputEventRouterTest,
                           QueryResultAfterChildViewDead);
  FRIEND_TEST_ALL_PREFIXES(DelegatedInkPointTest, EventForwardedToCompositor);
  FRIEND_TEST_ALL_PREFIXES(DelegatedInkPointTest,
                           MojoInterfaceReboundOnDisconnect);
  FRIEND_TEST_ALL_PREFIXES(NoCompositingRenderWidgetHostViewBrowserTest,
                           NoFallbackAfterHiddenNavigationFails);
  FRIEND_TEST_ALL_PREFIXES(NoCompositingRenderWidgetHostViewBrowserTest,
                           NoFallbackIfSwapFailedBeforeNavigation);

  void SynchronizeVisualProperties();

  // Generates the most current set of ScreenInfos from the current set of
  // displays in the system for use in UpdateScreenInfo.
  display::ScreenInfos GetNewScreenInfosForUpdate();

  // Called when display properties that need to be synchronized with the
  // renderer process changes. This method is called before notifying
  // RenderWidgetHostImpl in order to allow the view to allocate a new
  // LocalSurfaceId.
  virtual void OnSynchronizedDisplayPropertiesChanged(bool rotation = false) {}

  // Helper function to return whether the current background color is fully
  // opaque.
  bool IsBackgroundColorOpaque();

  bool is_evicted_ = false;

  bool is_frame_sink_id_owner_ = false;

  std::unique_ptr<ScopedViewTransitionResources> view_transition_resources_;

  base::WeakPtrFactory<RenderWidgetHostViewBase> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_BASE_H_
