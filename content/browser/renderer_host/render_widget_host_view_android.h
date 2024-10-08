// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_ANDROID_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_ANDROID_H_

#include <stddef.h>
#include <stdint.h>

#include <deque>
#include <map>
#include <memory>

#include "base/android/jni_android.h"
#include "base/callback_list.h"
#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "cc/mojom/render_frame_metadata.mojom-shared.h"
#include "cc/trees/render_frame_metadata.h"
#include "components/viz/common/quads/selection.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "content/browser/device_posture/device_posture_platform_provider.h"
#include "content/browser/renderer_host/input/mouse_wheel_phase_handler.h"
#include "content/browser/renderer_host/input/stylus_text_selector.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/renderer_host/screen_state.h"
#include "content/browser/renderer_host/text_input_manager.h"
#include "content/common/content_export.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/mojom/input/input_handler.mojom-forward.h"
#include "third_party/blink/public/mojom/widget/record_content_to_visible_time_request.mojom-forward.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/android/delegated_frame_host_android.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "ui/android/window_android_observer.h"
#include "ui/base/ui_base_types.h"
#include "ui/events/android/event_handler_android.h"
#include "ui/events/gesture_detection/filtered_gesture_provider.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/selection_bound.h"
#include "ui/touch_selection/touch_selection_controller.h"

namespace cc {
struct BrowserControlsOffsetTagsInfo;
}  // namespace cc

namespace cc::slim {
class SurfaceLayer;
}

namespace input {
struct NativeWebKeyboardEvent;
}  // namespace input

namespace ui {
class MotionEventAndroid;
class OverscrollRefreshHandler;
struct DidOverscrollParams;
}

namespace content {
class GestureListenerManager;
class ImeAdapterAndroid;
class OverscrollControllerAndroid;
class SelectionPopupController;
class SynchronousCompositorHost;
class SynchronousCompositorClient;
class TextSuggestionHostAndroid;
class TouchSelectionControllerClientManagerAndroid;
class WebContentsAccessibilityAndroid;
struct ContextMenuParams;

// -----------------------------------------------------------------------------
// See comments in render_widget_host_view.h about this class and its members.
// -----------------------------------------------------------------------------
class CONTENT_EXPORT RenderWidgetHostViewAndroid
    : public RenderWidgetHostViewBase,
      public StylusTextSelectorClient,
      public TextInputManager::Observer,
      public RenderFrameMetadataProvider::Observer,
      public ui::EventHandlerAndroid,
      public ui::GestureProviderClient,
      public ui::TouchSelectionControllerClient,
      public ui::ViewAndroidObserver,
      public ui::WindowAndroidObserver,
      public DevicePosturePlatformProvider::Observer {
 public:
  static RenderWidgetHostViewAndroid* FromRenderWidgetHostView(
      RenderWidgetHostView* view);

  // Note: The tree of `gfx::NativeView` might not match the tree of
  // `cc::slim::Layer`.
  RenderWidgetHostViewAndroid(RenderWidgetHostImpl* widget,
                              gfx::NativeView parent_native_view,
                              cc::slim::Layer* parent_layer);

  RenderWidgetHostViewAndroid(const RenderWidgetHostViewAndroid&) = delete;
  RenderWidgetHostViewAndroid& operator=(const RenderWidgetHostViewAndroid&) =
      delete;

  // Interface used to observe the destruction of a RenderWidgetHostViewAndroid.
  class DestructionObserver {
   public:
    virtual void RenderWidgetHostViewDestroyed(
        RenderWidgetHostViewAndroid* rwhva) = 0;

   protected:
    virtual ~DestructionObserver() {}
  };

  void AddDestructionObserver(DestructionObserver* connector);
  void RemoveDestructionObserver(DestructionObserver* connector);

  ui::TouchSelectionController* touch_selection_controller() {
    return touch_selection_controller_.get();
  }

  using SurfaceIdChangedCallbackType = void(const viz::SurfaceId& new_id);
  using SurfaceIdChangedCallback =
      base::RepeatingCallback<SurfaceIdChangedCallbackType>;
  using SurfaceIdChangedCallbackList =
      base::RepeatingCallbackList<SurfaceIdChangedCallbackType>;
  [[nodiscard]] base::CallbackListSubscription SubscribeToSurfaceIdChanges(
      const SurfaceIdChangedCallback& callback);

  // Called by DelegatedFrameHostClientAndroid
  void OnSurfaceIdChanged();

  // RenderWidgetHostView implementation.
  void InitAsChild(gfx::NativeView parent_view) override;
  void InitAsPopup(RenderWidgetHostView* parent_host_view,
                   const gfx::Rect& pos,
                   const gfx::Rect& anchor_rect) override;
  void SetSize(const gfx::Size& size) override;
  void SetBounds(const gfx::Rect& rect) override;
  gfx::NativeView GetNativeView() override;
  gfx::NativeViewAccessible GetNativeViewAccessible() override;
  void Focus() override;
  bool HasFocus() override;
  void Hide() override;
  bool IsShowing() override;
  gfx::Rect GetViewBounds() override;
  gfx::Size GetVisibleViewportSize() override;
  void SetInsets(const gfx::Insets& insets) override;
  gfx::Size GetCompositorViewportPixelSize() override;
  bool IsSurfaceAvailableForCopy() override;
  void CopyFromSurface(
      const gfx::Rect& src_rect,
      const gfx::Size& output_size,
      base::OnceCallback<void(const SkBitmap&)> callback) override;
  void CopyFromExactSurface(
      const gfx::Rect& src_rect,
      const gfx::Size& output_size,
      base::OnceCallback<void(const SkBitmap&)> callback) override;
  void EnsureSurfaceSynchronizedForWebTest() override;
  uint32_t GetCaptureSequenceNumber() const override;
  int GetMouseWheelMinimumGranularity() const override;
  void UpdateCursor(const ui::Cursor& cursor) override;
  void SetIsLoading(bool is_loading) override;
  void FocusedNodeChanged(bool is_editable_node,
                          const gfx::Rect& node_bounds_in_screen) override;
  bool ShouldInitiateStylusWriting() override;
  void NotifyHoverActionStylusWritable(bool stylus_writable) override;
  void OnStartStylusWriting() override;
  void OnEditElementFocusedForStylusWriting(
      const gfx::Rect& focused_edit_bounds,
      const gfx::Rect& caret_bounds) override;
  void OnEditElementFocusClearedForStylusWriting() override;
  void RenderProcessGone() override;
  void ShowWithVisibility(PageVisibilityState page_visibility) final;
  void Destroy() override;
  void UpdateTooltipUnderCursor(const std::u16string& tooltip_text) override;
  void UpdateTooltipFromKeyboard(const std::u16string& tooltip_text,
                                 const gfx::Rect& bounds) override;
  void ClearKeyboardTriggeredTooltip() override;
  void TransformPointToRootSurface(gfx::PointF* point) override;
  gfx::Rect GetBoundsInRootWindow() override;
  const viz::LocalSurfaceId& IncrementSurfaceIdForNavigation() override;
  void ProcessAckedTouchEvent(
      const input::TouchEventWithLatencyInfo& touch,
      blink::mojom::InputEventResultState ack_result) override;
  blink::mojom::InputEventResultState FilterInputEvent(
      const blink::WebInputEvent& input_event) override;
  void GestureEventAck(const blink::WebGestureEvent& event,
                       blink::mojom::InputEventResultSource ack_source,
                       blink::mojom::InputEventResultState ack_result) override;
  blink::mojom::PointerLockResult LockPointer(
      bool request_unadjusted_movement) override;
  blink::mojom::PointerLockResult ChangePointerLock(
      bool request_unadjusted_movement) override;
  void UnlockPointer() override;
  void InvalidateLocalSurfaceIdAndAllocationGroup() override;
  void ClearFallbackSurfaceForCommitPending() override;
  void ResetFallbackToFirstNavigationSurface() override;
  bool RequestRepaintForTesting() override;
  void DidOverscroll(const ui::DidOverscrollParams& params) override;
  bool CanSynchronizeVisualProperties() override;
  std::unique_ptr<SyntheticGestureTarget> CreateSyntheticGestureTarget()
      override;
  void OnOldViewDidNavigatePreCommit() override;
  void OnNewViewDidNavigatePostCommit() override;
  void DidEnterBackForwardCache() override;
  const viz::FrameSinkId& GetFrameSinkId() const override;
  viz::FrameSinkId GetRootFrameSinkId() override;
  viz::SurfaceId GetCurrentSurfaceId() const override;
  bool TransformPointToCoordSpaceForView(
      const gfx::PointF& point,
      input::RenderWidgetHostViewInput* target_view,
      gfx::PointF* transformed_point) override;
  TouchSelectionControllerClientManager*
  GetTouchSelectionControllerClientManager() override;
  const viz::LocalSurfaceId& GetLocalSurfaceId() const override;
  void OnRendererWidgetCreated() override;
  void TakeFallbackContentFrom(RenderWidgetHostView* view) override;
  void OnSynchronizedDisplayPropertiesChanged(bool rotation) override;
  std::optional<SkColor> GetBackgroundColor() override;
  void DidNavigate() override;
  WebContentsAccessibility* GetWebContentsAccessibility() override;
  viz::ScopedSurfaceIdAllocator DidUpdateVisualProperties(
      const cc::RenderFrameMetadata& metadata) override;
  display::ScreenInfo GetScreenInfo() const override;
  ui::mojom::VirtualKeyboardMode GetVirtualKeyboardMode() override;
  viz::SurfaceId GetFallbackSurfaceIdForTesting() const override;
  void ResetGestureDetection() override;

  // ui::EventHandlerAndroid implementation.
  bool OnTouchEvent(const ui::MotionEventAndroid& m) override;
  bool OnMouseEvent(const ui::MotionEventAndroid& m) override;
  bool OnMouseWheelEvent(const ui::MotionEventAndroid& event) override;
  bool OnGestureEvent(const ui::GestureEventAndroid& event) override;
  void OnSizeChanged() override;
  void OnPhysicalBackingSizeChanged(
      std::optional<base::TimeDelta> deadline_override) override;
  void NotifyVirtualKeyboardOverlayRect(
      const gfx::Rect& keyboard_rect) override;

  // ui::ViewAndroidObserver implementation:
  void OnAttachedToWindow() override;
  void OnDetachedFromWindow() override;

  // ui::GestureProviderClient implementation.
  void OnGestureEvent(const ui::GestureEventData& gesture) override;
  bool RequiresDoubleTapGestureEvents() const override;

  // ui::WindowAndroidObserver implementation.
  void OnRootWindowVisibilityChanged(bool visible) override;
  void OnAttachCompositor() override;
  void OnDetachCompositor() override;
  void OnAnimate(base::TimeTicks begin_frame_time) override;
  void OnUnfoldStarted(base::TimeTicks unfold_begin_time) override;
  void OnActivityStopped() override;
  void OnActivityStarted() override;

  // StylusTextSelectorClient implementation.
  void OnStylusSelectBegin(float x0, float y0, float x1, float y1) override;
  void OnStylusSelectUpdate(float x, float y) override;
  void OnStylusSelectEnd(float x, float y) override;
  void OnStylusSelectTap(base::TimeTicks time, float x, float y) override;

  // ui::TouchSelectionControllerClient implementation.
  bool SupportsAnimation() const override;
  void SetNeedsAnimate() override;
  void MoveCaret(const gfx::PointF& position) override;
  void MoveRangeSelectionExtent(const gfx::PointF& extent) override;
  void SelectBetweenCoordinates(const gfx::PointF& base,
                                const gfx::PointF& extent) override;
  void OnSelectionEvent(ui::SelectionEventType event) override;
  void OnDragUpdate(const ui::TouchSelectionDraggable::Type type,
                    const gfx::PointF& position) override;
  std::unique_ptr<ui::TouchHandleDrawable> CreateDrawable() override;
  void DidScroll() override;
  void ShowTouchSelectionContextMenu(const gfx::Point& location) override;

  // Non-virtual methods
  void UpdateNativeViewTree(gfx::NativeView parent_native_view,
                            cc::slim::Layer* parent_layer);

  // Returns the temporary background color of the underlaying document, for
  // example, returns black during screen rotation.
  std::optional<SkColor> GetCachedBackgroundColor();
  void SendKeyEvent(const input::NativeWebKeyboardEvent& event);
  void SendMouseEvent(const blink::WebMouseEvent& event,
                      const ui::LatencyInfo& info);
  void SendMouseWheelEvent(const blink::WebMouseWheelEvent& event);
  void SendGestureEvent(const blink::WebGestureEvent& event);
  bool ShowSelectionMenu(RenderFrameHost* render_frame_host,
                         const ContextMenuParams& params);
  void set_ime_adapter(ImeAdapterAndroid* ime_adapter) {
    ime_adapter_android_ = ime_adapter;
  }
  void set_selection_popup_controller(SelectionPopupController* controller) {
    selection_popup_controller_ = controller;
  }
  SelectionPopupController* selection_popup_controller() const {
    return selection_popup_controller_.get();
  }
  void set_text_suggestion_host(
      TextSuggestionHostAndroid* text_suggestion_host) {
    text_suggestion_host_ = text_suggestion_host;
  }
  TextSuggestionHostAndroid* text_suggestion_host() const {
    return text_suggestion_host_;
  }
  void SetGestureListenerManager(GestureListenerManager* manager);

  // See
  // `RenderFrameMetadataProviderImpl::UpdateRootScrollOffsetUpdateFrequency()`.
  void UpdateRootScrollOffsetUpdateFrequency();

  base::WeakPtr<RenderWidgetHostViewAndroid> GetWeakPtrAndroid();

  bool OnTouchHandleEvent(const ui::MotionEvent& event);
  int GetTouchHandleHeight();
  void SetDoubleTapSupportEnabled(bool enabled);
  void SetMultiTouchZoomSupportEnabled(bool enabled);

  bool SynchronizeVisualProperties(
      const cc::DeadlinePolicy& deadline_policy,
      const std::optional<viz::LocalSurfaceId>& child_local_surface_id,
      bool reuse_current_local_surface_id = false,
      bool ignore_ack = false);

  bool HasValidFrame() const;

  void MoveCaret(const gfx::Point& point);
  void DismissTextHandles();
  void SetTextHandlesHiddenForDropdownMenu(bool hide_handles);
  void SetTextHandlesTemporarilyHidden(bool hide_handles);
  void SelectAroundCaretAck(int startOffset,
                            int endOffset,
                            int surroundingTextLength,
                            blink::mojom::SelectAroundCaretResultPtr result);

  void SetSynchronousCompositorClient(SynchronousCompositorClient* client);

  SynchronousCompositorClient* synchronous_compositor_client() const {
    return synchronous_compositor_client_;
  }

  void OnOverscrollRefreshHandlerAvailable();

  // TextInputManager::Observer overrides.
  void OnUpdateTextInputStateCalled(TextInputManager* text_input_manager,
                                    RenderWidgetHostViewBase* updated_view,
                                    bool did_change_state) override;
  void OnImeCompositionRangeChanged(
      TextInputManager* text_input_manager,
      RenderWidgetHostViewBase* updated_view,
      bool character_bounds_changed,
      const std::optional<std::vector<gfx::Rect>>& line_bounds) override;
  void OnImeCancelComposition(TextInputManager* text_input_manager,
                              RenderWidgetHostViewBase* updated_view) override;
  void OnTextSelectionChanged(TextInputManager* text_input_manager,
                              RenderWidgetHostViewBase* updated_view) override;

  ImeAdapterAndroid* ime_adapter_for_testing() { return ime_adapter_android_; }

  ui::TouchSelectionControllerClient*
  GetSelectionControllerClientManagerForTesting();
  void SetSelectionControllerClientForTesting(
      std::unique_ptr<ui::TouchSelectionControllerClient> client);

  void SetOverscrollControllerForTesting(
      ui::OverscrollRefreshHandler* overscroll_refresh_handler);

  void GotFocus();
  void LostFocus();

  // RenderFrameMetadataProvider::Observer implementation.
  void OnRenderFrameMetadataChangedBeforeActivation(
      const cc::RenderFrameMetadata& metadata) override;
  void OnRenderFrameMetadataChangedAfterActivation(
      base::TimeTicks activation_time) override;
  void OnRenderFrameSubmission() override {}
  void OnLocalSurfaceIdChanged(
      const cc::RenderFrameMetadata& metadata) override {}
  void OnRootScrollOffsetChanged(
      const gfx::PointF& root_scroll_offset) override;

  void WasEvicted();

  void SetWebContentsAccessibility(
      WebContentsAccessibilityAndroid* web_contents_accessibility);

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  // Methods called from Java
  bool IsReady(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  void DismissTextHandles(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj);

  // Returns an int equivalent to an Optional<SKColor>, with a value of 0
  // indicating SKTransparent for not set.
  jint GetBackgroundColor(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj);

  void ShowContextMenuAtTouchHandle(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jint x,
      jint y);

  // Notifies that the Visual Viewport's inset bottom has changed.
  void OnViewportInsetBottomChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  void WriteContentBitmapToDiskAsync(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jint width,
      jint height,
      const base::android::JavaParamRef<jstring>& jpath,
      const base::android::JavaParamRef<jobject>& jcallback);

  // Notifies that the parent activity has moved into the foreground.
  void OnResume(JNIEnv* env);

  ui::DelegatedFrameHostAndroid* delegated_frame_host_for_testing() {
    return delegated_frame_host_.get();
  }

  void SetNeedsBeginFrameForFlingProgress();

  const cc::slim::SurfaceLayer* GetSurfaceLayer() const;

  void RegisterOffsetTags(const cc::BrowserControlsOffsetTagsInfo& tags_info);
  void UnregisterOffsetTags(const cc::BrowserControlsOffsetTagsInfo& tags_info);

  void PassImeRenderWidgetHost(
      mojo::PendingRemote<blink::mojom::ImeRenderWidgetHost> pending_remote);

 protected:
  ~RenderWidgetHostViewAndroid() override;

  // RenderWidgetHostViewBase:
  void UpdateFrameSinkIdRegistration() override;
  void UpdateBackgroundColor() override;
  bool HasFallbackSurface() const override;
  std::optional<DisplayFeature> GetDisplayFeature() override;
  void SetDisplayFeatureForTesting(
      const DisplayFeature* display_feature) override;
  void NotifyHostAndDelegateOnWasShown(
      blink::mojom::RecordContentToVisibleTimeRequestPtr visible_time_request)
      final;
  void RequestSuccessfulPresentationTimeFromHostOrDelegate(
      blink::mojom::RecordContentToVisibleTimeRequestPtr visible_time_request)
      final;
  void CancelSuccessfulPresentationTimeRequestForHostAndDelegate() final;
  void EnterFullscreenMode(
      const blink::mojom::FullscreenOptions& options) override;
  void ExitFullscreenMode() override;
  void LockOrientation(
      device::mojom::ScreenOrientationLockType orientation) override;
  void UnlockOrientation() override;
  void SetHasPersistentVideo(bool has_persistent_video) override;

 private:
  friend class RenderWidgetHostViewAndroidTest;
  friend class RenderWidgetHostViewAndroidFullscreenRotationTest;
  friend class RenderWidgetHostViewAndroidRotationTest;
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           GestureManagerListensToChildFrames);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAndroidTest, DisplayFeature);

  class ScreenStateChangeHandler {
   public:
    explicit ScreenStateChangeHandler(RenderWidgetHostViewAndroid* rwhva);
    ~ScreenStateChangeHandler() = default;

    bool CanSynchronizeVisualProperties() const;

    // Visual Property updates.
    void OnVisibleViewportSizeChanged(const gfx::Size& visible_viewport_size);
    bool OnPhysicalBackingSizeChanged(const gfx::Size& physical_backing_size,
                                      int64_t deadline_in_frames);
    bool OnScreenInfoChanged(const display::ScreenInfo& screen_info);

    // State transitions.
    void EnterFullscreenMode();
    void ExitFullscreenMode();
    void LockOrientation(device::mojom::ScreenOrientationLockType orientation);
    void UnlockOrientation();
    void SetHasPersistentVideo(bool has_persistent_video);
    void WasEvicted();
    void WasShownAfterEviction();

   private:
    friend class RenderWidgetHostViewAndroidRotationTest;

    // Sets the `current_screen_state_` to be the current values. Clears
    // `pending_screen_state_` to begin tracking subsequent updates.
    void BeginScreenStateChange();

    // Reviews the states of `pending_screen_state_` to handle rotations,
    // fullscreen, and Picture-in-Picture mode.
    bool HandleScreenStateChanges(const cc::DeadlinePolicy& deadline_policy,
                                  bool force_fullscreen_sync = false);

    // Clears flags used to throttle SurfaceSync.
    void Unthrottle();

    // The ScreenState of the current world, the pending visual properties, or
    // the properties from before we entered Picture-in-Picture mode.
    ScreenState current_screen_state_;
    ScreenState pending_screen_state_;
    ScreenState pre_picture_in_picture_;

    base::OneShotTimer throttle_timeout_;

    raw_ptr<RenderWidgetHostViewAndroid> rwhva_;
  };

  cc::mojom::RootScrollOffsetUpdateFrequency RootScrollOffsetUpdateFrequency();

  MouseWheelPhaseHandler* GetMouseWheelPhaseHandler() override;

  bool ShouldRouteEvents() const;

  void UpdateTouchSelectionController(
      const viz::Selection<gfx::SelectionBound>& selection,
      float page_scale_factor,
      float top_controls_height,
      float top_controls_shown_ratio,
      const gfx::SizeF& scrollable_viewport_size_dip);
  bool UpdateControls(float dip_scale,
                      float top_controls_height,
                      float top_controls_shown_ratio,
                      float top_controls_min_height_offset,
                      float bottom_controls_height,
                      float bottom_controls_shown_ratio,
                      float bottom_controls_min_height_offset);
  void OnDidUpdateVisualPropertiesComplete(
      const cc::RenderFrameMetadata& metadata);

  void OnFinishGetContentBitmap(const base::android::JavaRef<jobject>& obj,
                                const base::android::JavaRef<jobject>& callback,
                                const std::string& path,
                                const SkBitmap& bitmap);

  void ShowInternal();
  void HideInternal();
  void AttachLayers();
  void RemoveLayers();

  // Helper function to update background color for WebView on fullscreen
  // changes. See https://crbug.com/961223.
  void UpdateWebViewBackgroundColorIfNecessary();

  // DevTools ScreenCast support for Android WebView.
  void SynchronousCopyContents(
      const gfx::Rect& src_subrect_dip,
      const gfx::Size& dst_size_in_pixel,
      base::OnceCallback<void(const SkBitmap&)> callback);

  void MaybeCreateSynchronousCompositor();
  void ResetSynchronousCompositor();

  void StartObservingRootWindow();
  void StopObservingRootWindow();
  bool Animate(base::TimeTicks frame_time);
  void RequestDisallowInterceptTouchEvent();

  void ComputeEventLatencyOSTouchHistograms(const ui::MotionEvent& event);

  void CreateOverscrollControllerIfPossible();

  void UpdateMouseState(int action_button,
                        float mousedown_x,
                        float mouse_down_y);

  WebContentsAccessibilityAndroid* GetWebContentsAccessibilityAndroid() const;

  void OnFocusInternal();
  void LostFocusInternal();

  void SetTextHandlesHiddenForStylus(bool hide_handles);
  void SetTextHandlesHiddenInternal();

  void OnUpdateScopedSelectionHandles();

  void HandleSwipeToMoveCursorGestureAck(const blink::WebGestureEvent& event);

  void BeginRotationBatching();
  void EndRotationBatching();
  void BeginRotationEmbed();
  void EndRotationAndSyncIfNecessary();
  void EvictInternal();

  // DevicePosturePlatformProvider::Observer.
  void OnDisplayFeatureBoundsChanged(
      const gfx::Rect& display_feature_bounds) override;
  void ObserveDevicePosturePlatformProvider();
  void ComputeDisplayFeature();
  void SetDisplayFeatureBoundsForTesting(const gfx::Rect& bounds);

  bool is_showing_;

  // Window-specific bits that affect widget visibility.
  bool is_window_visible_;
  bool is_window_activity_started_;

  PageVisibilityState page_visibility_ = PageVisibilityState::kHidden;

  // Specifies whether touch selection handles are hidden due to the dropdown
  // menu.
  bool handles_hidden_by_dropdown_menu_ = false;

  // Specifies whether touch selection handles are hidden due to stylus.
  bool handles_hidden_by_stylus_ = false;

  // Specifies whether touch selection handles are hidden due to text selection.
  bool handles_hidden_by_selection_ui_ = false;

  raw_ptr<ImeAdapterAndroid> ime_adapter_android_;
  raw_ptr<SelectionPopupController> selection_popup_controller_;
  raw_ptr<TextSuggestionHostAndroid> text_suggestion_host_;
  raw_ptr<GestureListenerManager> gesture_listener_manager_;

  mutable ui::ViewAndroid view_;

  std::unique_ptr<ui::DelegatedFrameHostAndroid::Client>
      delegated_frame_host_client_;

  // Manages the Compositor Frames received from the renderer.
  std::unique_ptr<ui::DelegatedFrameHostAndroid> delegated_frame_host_;

  // The most recent surface size that was pushed to the surface layer.
  gfx::Size current_surface_size_;

  // Used to control and render overscroll-related effects.
  std::unique_ptr<OverscrollControllerAndroid> overscroll_controller_;

  // Provides gesture synthesis given a stream of touch events (derived from
  // Android MotionEvent's) and touch event acks.
  ui::FilteredGestureProvider gesture_provider_;

  // Handles gesture based text selection
  StylusTextSelector stylus_text_selector_;

  // Manages selection handle rendering and manipulation.
  // This will always be NULL if |content_view_core_| is NULL.
  std::unique_ptr<ui::TouchSelectionController> touch_selection_controller_;
  std::unique_ptr<ui::TouchSelectionControllerClient>
      touch_selection_controller_client_for_test_;
  // Keeps track of currently active touch selection controller clients (some
  // may be representing out-of-process iframes).
  std::unique_ptr<TouchSelectionControllerClientManagerAndroid>
      touch_selection_controller_client_manager_;
  // Notifies the WindowAndroid when the page has active selection handles.
  std::unique_ptr<ui::WindowAndroid::ScopedSelectionHandles>
      scoped_selection_handles_;

  // Bounds to use if we have no backing WebContents.
  gfx::Rect default_bounds_;

  const bool using_browser_compositor_;
  std::unique_ptr<SynchronousCompositorHost> sync_compositor_;

  raw_ptr<SynchronousCompositorClient> synchronous_compositor_client_;

  bool observing_root_window_;

  bool controls_initialized_ = false;

  float prev_top_shown_pix_;
  float prev_top_controls_pix_;
  float prev_top_controls_translate_;
  float prev_top_controls_min_height_offset_pix_;
  float prev_bottom_shown_pix_;
  float prev_bottom_controls_translate_;
  float prev_bottom_controls_min_height_offset_pix_;
  float page_scale_;
  float min_page_scale_;
  float max_page_scale_;

  base::TimeTicks prev_mousedown_timestamp_;
  gfx::Point prev_mousedown_point_;
  int left_click_count_ = 0;

  base::ObserverList<DestructionObserver>::Unchecked destruction_observers_;

  MouseWheelPhaseHandler mouse_wheel_phase_handler_;
  uint32_t latest_capture_sequence_number_ = 0u;

  viz::ParentLocalSurfaceIdAllocator local_surface_id_allocator_;
  bool in_rotation_ = false;
  // Tracks whether rotation was due to a fullscreen transition. Upon exiting
  // fullscren the subsequent rotation order will be different.
  bool fullscreen_rotation_ = false;
  // Tracks the time at which rotation started, along with the targeted
  // viz::LocalSurfaceId which would first embed the new rotation. This is a
  // deque because it is possible that one rotation may be interrupted by
  // another before the first has displayed. This can occur on pages that have
  // long layout and rendering time.
  std::deque<std::pair<base::TimeTicks, viz::LocalSurfaceId>> rotation_metrics_;
  // In case we do not get signaled of all the layout changes, we will use a
  // timeout. At which point we will begin SurfaceSync again. To prevent ever
  // getting stuck in a state where the Renderer cannot produce frames.
  base::OneShotTimer rotation_timeout_;

  // If true, then content was displayed before the completion of the initial
  // navigation. After any content has been displayed, we need to allocate a new
  // surface for all subsequent navigations.
  bool pre_navigation_content_ = false;
  // If true, then the next allocated surface should be embedded.
  bool navigation_while_hidden_ = false;

  // False at creation time until the connection to the renderer process is
  // established. If the connection is lost (ie. renderer process crash) then
  // this object will be destroyed and recreated for the new process.
  // NOTE: Due to unfortunate circumstances, the RenderWidgetHost and the
  // RenderWidgetHostView will outlive the renderer-side object if a
  // cross-process navigation occurs and the main frame moves out of the
  // process. At that time this value would remain true though there is no
  // Widget anymore associated with it. See https://crbug.com/419087.
  bool renderer_widget_created_ = false;

  // Whether swipe-to-move-cursor gesture is activated.
  bool swipe_to_move_cursor_activated_ = false;

  raw_ptr<WebContentsAccessibilityAndroid> web_contents_accessibility_ =
      nullptr;

  // Represents a feature of the physical display whose offset and mask_length
  // are expressed in DIPs relative to the view. See display_feature.h for more
  // details.
  std::optional<DisplayFeature> display_feature_;
  bool display_feature_overridden_for_testing_ = false;
  // Display feature bounds returned by the OS.
  gfx::Rect display_feature_bounds_;

  base::ScopedObservation<DevicePosturePlatformProvider,
                          DevicePosturePlatformProvider::Observer>
      device_posture_observation_{this};

  SurfaceIdChangedCallbackList surface_id_changed_callbacks_;

  base::android::ScopedJavaGlobalRef<jobject> obj_;

  ScreenStateChangeHandler screen_state_change_handler_;

  base::WeakPtrFactory<RenderWidgetHostViewAndroid> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_ANDROID_H_
