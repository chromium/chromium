// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_MAC_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_MAC_H_

#import <Cocoa/Cocoa.h>

#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "content/app_shim_remote_cocoa/render_widget_host_ns_view_host_helper.h"
#include "content/browser/renderer_host/browser_compositor_view_mac.h"
#include "content/browser/renderer_host/input/mouse_wheel_phase_handler.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/renderer_host/text_input_manager.h"
#include "content/common/content_export.h"
#include "content/common/render_widget_host_ns_view.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/mojom/webshare/webshare.mojom.h"
#include "third_party/blink/public/mojom/widget/record_content_to_visible_time_request.mojom-forward.h"
#include "ui/accelerated_widget_mac/accelerated_widget_mac.h"
#include "ui/base/cocoa/accessibility_focus_overrider.h"
#include "ui/base/cocoa/remote_layer_api.h"
#include "ui/base/mojom/attributed_string.mojom-forward.h"
#include "ui/display/display_list.h"
#include "ui/events/gesture_detection/filtered_gesture_provider.h"

namespace remote_cocoa {
namespace mojom {
class Application;
}  // namespace mojom
class RenderWidgetHostNSViewBridge;
}  // namespace remote_cocoa

namespace ui {
enum class DomCode : uint32_t;
class Layer;
class ScopedPasswordInputEnabler;
}

namespace input {
class CursorManager;
}  // namespace input

@protocol RenderWidgetHostViewMacDelegate;

@class NSAccessibilityRemoteUIElement;
@class RenderWidgetHostViewCocoa;

namespace content {

class RenderWidgetHost;
class RenderWidgetHostViewMac;
class WebContents;

///////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewMac
//
//  An object representing the "View" of a rendered web page. This object is
//  responsible for displaying the content of the web page, and integrating with
//  the Cocoa view system. It is the implementation of the RenderWidgetHostView
//  that the cross-platform RenderWidgetHost object uses
//  to display the data.
//
//  Comment excerpted from render_widget_host.h:
//
//    "The lifetime of the RenderWidgetHost* is tied to the render process.
//     If the render process dies, the RenderWidgetHost* goes away and all
//     references to it must become NULL."
//
// RenderWidgetHostView class hierarchy described in render_widget_host_view.h.
class CONTENT_EXPORT RenderWidgetHostViewMac
    : public RenderWidgetHostViewBase,
      public remote_cocoa::RenderWidgetHostNSViewHostHelper,
      public remote_cocoa::mojom::RenderWidgetHostNSViewHost,
      public BrowserCompositorMacClient,
      public TextInputManager::Observer,
      public RenderFrameMetadataProvider::Observer,
      public ui::GestureProviderClient,
      public ui::AcceleratedWidgetMacNSView,
      public ui::AccessibilityFocusOverrider::Client {
 public:
  // The view will associate itself with the given widget. The native view must
  // be hooked up immediately to the view hierarchy, or else when it is
  // deleted it will delete this out from under the caller.
  RenderWidgetHostViewMac(RenderWidgetHost* widget);

  RenderWidgetHostViewMac(const RenderWidgetHostViewMac&) = delete;
  RenderWidgetHostViewMac& operator=(const RenderWidgetHostViewMac&) = delete;

  RenderWidgetHostViewCocoa* GetInProcessNSView() const;
  remote_cocoa::mojom::RenderWidgetHostNSView* GetNSView() const {
    return ns_view_;
  }

  // |delegate| is used to separate out the logic from the NSResponder delegate.
  // |delegate| is retained by this class.
  // |delegate| should be set at most once.
  CONTENT_EXPORT void SetDelegate(
    NSObject<RenderWidgetHostViewMacDelegate>* delegate);

  // RenderWidgetHostView implementation.
  void InitAsChild(gfx::NativeView parent_view) override;
  void SetSize(const gfx::Size& size) override;
  void SetBounds(const gfx::Rect& rect) override;
  gfx::NativeView GetNativeView() override;
  gfx::NativeViewAccessible GetNativeViewAccessible() override;
  bool HasFocus() override;
  void Hide() override;
  bool IsShowing() override;
  void WasUnOccluded() override;
  void WasOccluded() override;
  gfx::Rect GetViewBounds() override;
  bool IsPointerLocked() override;
  void SetActive(bool active) override;
  void ShowDefinitionForSelection() override;
  void SpeakSelection() override;
  void SetWindowFrameInScreen(const gfx::Rect& rect) override;
  void TakeFallbackContentFrom(RenderWidgetHostView* view) override;
  bool IsHTMLFormPopup() const override;
  uint64_t GetNSViewId() const override;

  // Implementation of RenderWidgetHostViewBase.
  void InitAsPopup(RenderWidgetHostView* parent_host_view,
                   const gfx::Rect& pos,
                   const gfx::Rect& anchor_rect) override;
  void Focus() override;
  void UpdateCursor(const ui::Cursor& cursor) override;
  void DisplayCursor(const ui::Cursor& cursor) override;
  input::CursorManager* GetCursorManager() override;
  void OnOldViewDidNavigatePreCommit() override;
  void OnNewViewDidNavigatePostCommit() override;
  void DidEnterBackForwardCache() override;
  void SetIsLoading(bool is_loading) override;
  void RenderProcessGone() override;
  void ShowWithVisibility(PageVisibilityState page_visibility) final;
  void Destroy() override;
  void UpdateTooltipUnderCursor(const std::u16string& tooltip_text) override;
  void UpdateTooltip(const std::u16string& tooltip_text) override;
  gfx::Size GetRequestedRendererSize() override;
  uint32_t GetCaptureSequenceNumber() const override;
  bool IsSurfaceAvailableForCopy() override;
  void CopyFromSurface(
      const gfx::Rect& src_rect,
      const gfx::Size& output_size,
      base::OnceCallback<void(const SkBitmap&)> callback) override;
  void EnsureSurfaceSynchronizedForWebTest() override;
  void FocusedNodeChanged(bool is_editable_node,
                          const gfx::Rect& node_bounds_in_screen) override;
  void InvalidateLocalSurfaceIdAndAllocationGroup() override;
  void ClearFallbackSurfaceForCommitPending() override;
  void ResetFallbackToFirstNavigationSurface() override;
  bool RequestRepaintForTesting() override;
  gfx::NativeViewAccessible AccessibilityGetNativeViewAccessible() override;
  gfx::NativeViewAccessible AccessibilityGetNativeViewAccessibleForWindow()
      override;
  std::optional<SkColor> GetBackgroundColor() override;
  viz::SurfaceId GetFallbackSurfaceIdForTesting() const override;

  void TransformPointToRootSurface(gfx::PointF* point) override;
  gfx::Rect GetBoundsInRootWindow() override;
  void UpdateScreenInfo() override;
  viz::ScopedSurfaceIdAllocator DidUpdateVisualProperties(
      const cc::RenderFrameMetadata& metadata) override;
  void DidNavigate() override;

  blink::mojom::PointerLockResult LockPointer(bool) override;
  blink::mojom::PointerLockResult ChangePointerLock(bool) override;
  void UnlockPointer() override;
  // Checks if the window is key, in addition to "focused".
  bool CanBePointerLocked() override;
  // Checks if the window is key, in addition to "focused".
  bool AccessibilityHasFocus() override;
  bool GetIsPointerLockedUnadjustedMovementForTesting() override;
  // Returns true when running on a recent enough OS for unaccelerated pointer
  // events.
  bool LockKeyboard(std::optional<base::flat_set<ui::DomCode>> codes) override;
  void UnlockKeyboard() override;
  bool IsKeyboardLocked() override;
  base::flat_map<std::string, std::string> GetKeyboardLayoutMap() override;
  void GestureEventAck(const blink::WebGestureEvent& event,
                       blink::mojom::InputEventResultSource ack_source,
                       blink::mojom::InputEventResultState ack_result) override;
  void ProcessAckedTouchEvent(
      const input::TouchEventWithLatencyInfo& touch,
      blink::mojom::InputEventResultState ack_result) override;

  void DidOverscroll(const ui::DidOverscrollParams& params) override;

  std::unique_ptr<SyntheticGestureTarget> CreateSyntheticGestureTarget()
      override;

  void UpdateFrameSinkIdRegistration() override;
  const viz::FrameSinkId& GetFrameSinkId() const override;
  const viz::LocalSurfaceId& GetLocalSurfaceId() const override;
  // Returns true when we can hit test input events with location data to be
  // sent to the targeted RenderWidgetHost.
  bool ShouldRouteEvents() const;
  // This method checks |event| to see if a GesturePinch or double tap event
  // can be routed according to ShouldRouteEvent, and if not, sends it directly
  // to the view's RenderWidgetHost.
  // By not just defaulting to sending events that change the page scale to the
  // main frame, we allow the events to be targeted to an oopif subframe, in
  // case some consumer, such as PDF or maps, wants to intercept them and
  // implement a custom behavior.
  void SendTouchpadZoomEvent(const blink::WebGestureEvent* event);

  // Inject synthetic touch events.
  void InjectTouchEvent(const blink::WebTouchEvent& event,
                        const ui::LatencyInfo& latency_info);

  bool TransformPointToCoordSpaceForView(
      const gfx::PointF& point,
      input::RenderWidgetHostViewInput* target_view,
      gfx::PointF* transformed_point) override;
  viz::FrameSinkId GetRootFrameSinkId() override;
  viz::SurfaceId GetCurrentSurfaceId() const override;

  // TextInputManager::Observer implementation.
  void OnUpdateTextInputStateCalled(TextInputManager* text_input_manager,
                                    RenderWidgetHostViewBase* updated_view,
                                    bool did_update_state) override;
  void OnImeCancelComposition(TextInputManager* text_input_manager,
                              RenderWidgetHostViewBase* updated_view) override;
  void OnImeCompositionRangeChanged(
      TextInputManager* text_input_manager,
      RenderWidgetHostViewBase* updated_view,
      bool character_bounds_changed,
      const std::optional<std::vector<gfx::Rect>>& line_bounds) override;
  void OnSelectionBoundsChanged(
      TextInputManager* text_input_manager,
      RenderWidgetHostViewBase* updated_view) override;
  void OnTextSelectionChanged(TextInputManager* text_input_manager,
                              RenderWidgetHostViewBase* updated_view) override;

  // ui::GestureProviderClient implementation.
  void OnGestureEvent(const ui::GestureEventData& gesture) override;

  // RenderFrameMetadataProvider::Observer
  void OnRenderFrameMetadataChangedBeforeActivation(
      const cc::RenderFrameMetadata& metadata) override {}
  void OnRenderFrameMetadataChangedAfterActivation(
      base::TimeTicks activation_time) override;
  void OnRenderFrameSubmission() override {}
  void OnLocalSurfaceIdChanged(
      const cc::RenderFrameMetadata& metadata) override {}

  void SetTextInputActive(bool active);

  // Returns true and stores first rectangle for character range if the
  // requested |range| is already cached, otherwise returns false.
  // Exposed for testing.
  CONTENT_EXPORT bool GetCachedFirstRectForCharacterRange(
      const gfx::Range& requested_range,
      gfx::Rect* rect,
      gfx::Range* actual_range);

  // Returns true if there is line break in |range| and stores line breaking
  // point to |line_breaking_point|. The |line_break_point| is valid only if
  // this function returns true.
  bool GetLineBreakIndex(const std::vector<gfx::Rect>& bounds,
                         const gfx::Range& range,
                         size_t* line_break_point);

  // Returns composition character boundary rectangle. The |range| is
  // composition based range. Also stores |actual_range| which is corresponding
  // to actually used range for returned rectangle.
  gfx::Rect GetFirstRectForCompositionRange(const gfx::Range& range,
                                            gfx::Range* actual_range);

  // Converts from given whole character range to composition oriented range.
  // If `request_range` is beyond the end of composition range, return an empty
  // range at the composition end as a heuristic result.
  // If the conversion failed, return gfx::Range::InvalidRange.
  gfx::Range ConvertCharacterRangeToCompositionRange(
      const gfx::Range& request_range);

  WebContents* GetWebContents();

  // Set the color of the background CALayer shown when no content is ready to
  // see.
  void SetBackgroundLayerColor(SkColor color);

  bool HasPendingWheelEndEventForTesting() {
    return mouse_wheel_phase_handler_.HasPendingWheelEndEvent();
  }

  // These member variables should be private, but the associated ObjC class
  // needs access to them and can't be made a friend.

  // Delegated frame management and compositor interface.
  std::unique_ptr<BrowserCompositorMac> browser_compositor_;
  BrowserCompositorMac* BrowserCompositor() const {
    return browser_compositor_.get();
  }

  // Set when the currently-displayed frame is the minimum scale. Used to
  // determine if pinch gestures need to be thresholded.
  bool page_at_minimum_scale_;

  MouseWheelPhaseHandler mouse_wheel_phase_handler_;

  // Used to set the mouse_wheel_phase_handler_ timer timeout for testing.
  void set_mouse_wheel_wheel_phase_handler_timeout(base::TimeDelta timeout) {
    mouse_wheel_phase_handler_.set_mouse_wheel_end_dispatch_timeout(timeout);
  }

  // Used to get the max amount of time to wait after a phase end event for a
  // momentum phase began event.
  const base::TimeDelta
  max_time_between_phase_ended_and_momentum_phase_began_for_test() {
    return mouse_wheel_phase_handler_
        .max_time_between_phase_ended_and_momentum_phase_began();
  }

  // RenderWidgetHostNSViewHostHelper implementation.
  id GetAccessibilityElement() override;
  id GetRootBrowserAccessibilityElement() override;
  id GetFocusedBrowserAccessibilityElement() override;
  void SetAccessibilityWindow(NSWindow* window) override;
  void ForwardKeyboardEvent(const input::NativeWebKeyboardEvent& key_event,
                            const ui::LatencyInfo& latency_info) override;
  void ForwardKeyboardEventWithCommands(
      const input::NativeWebKeyboardEvent& key_event,
      const ui::LatencyInfo& latency_info,
      std::vector<blink::mojom::EditCommandPtr> commands) override;
  void RouteOrProcessMouseEvent(const blink::WebMouseEvent& web_event) override;
  void RouteOrProcessTouchEvent(const blink::WebTouchEvent& web_event) override;
  void RouteOrProcessWheelEvent(
      const blink::WebMouseWheelEvent& web_event) override;
  void ForwardMouseEvent(const blink::WebMouseEvent& web_event) override;
  void ForwardWheelEvent(const blink::WebMouseWheelEvent& web_event) override;
  void GestureBegin(blink::WebGestureEvent begin_event,
                    bool is_synthetically_injected) override;
  void GestureUpdate(blink::WebGestureEvent update_event) override;
  void GestureEnd(blink::WebGestureEvent end_event) override;
  void SmartMagnify(const blink::WebGestureEvent& smart_magnify_event) override;

  // mojom::RenderWidgetHostNSViewHost implementation.
  void SyncIsWidgetForMainFrame(
      SyncIsWidgetForMainFrameCallback callback) override;
  bool SyncIsWidgetForMainFrame(bool* is_for_main_frame) override;
  void RequestShutdown() override;
  void OnFirstResponderChanged(bool is_first_responder) override;
  void OnWindowIsKeyChanged(bool is_key) override;
  void OnBoundsInWindowChanged(const gfx::Rect& view_bounds_in_window_dip,
                               bool attached_to_window) override;
  void OnWindowFrameInScreenChanged(
      const gfx::Rect& window_frame_in_screen_dip) override;
  void OnScreenInfosChanged(const display::ScreenInfos& screen_infos) override;
  void BeginKeyboardEvent() override;
  void EndKeyboardEvent() override;
  void ForwardKeyboardEventWithCommands(
      std::unique_ptr<blink::WebCoalescedInputEvent> event,
      const std::vector<uint8_t>& native_event_data,
      bool skip_if_unhandled,
      std::vector<blink::mojom::EditCommandPtr> commands) override;
  void RouteOrProcessMouseEvent(
      std::unique_ptr<blink::WebCoalescedInputEvent> event) override;
  void RouteOrProcessTouchEvent(
      std::unique_ptr<blink::WebCoalescedInputEvent> event) override;
  void RouteOrProcessWheelEvent(
      std::unique_ptr<blink::WebCoalescedInputEvent> event) override;
  void ForwardMouseEvent(
      std::unique_ptr<blink::WebCoalescedInputEvent> event) override;
  void ForwardWheelEvent(
      std::unique_ptr<blink::WebCoalescedInputEvent> event) override;
  void GestureBegin(std::unique_ptr<blink::WebCoalescedInputEvent> event,
                    bool is_synthetically_injected) override;
  void GestureUpdate(
      std::unique_ptr<blink::WebCoalescedInputEvent> event) override;
  void GestureEnd(
      std::unique_ptr<blink::WebCoalescedInputEvent> event) override;
  void SmartMagnify(
      std::unique_ptr<blink::WebCoalescedInputEvent> event) override;
  void ImeSetComposition(const std::u16string& text,
                         const std::vector<ui::ImeTextSpan>& ime_text_spans,
                         const gfx::Range& replacement_range,
                         int selection_start,
                         int selection_end) override;
  void ImeCommitText(const std::u16string& text,
                     const gfx::Range& replacement_range) override;
  void ImeFinishComposingText() override;
  void ImeCancelCompositionFromCocoa() override;
  void LookUpDictionaryOverlayAtPoint(
      const gfx::PointF& root_point_in_dips) override;
  void LookUpDictionaryOverlayFromRange(const gfx::Range& range) override;
  void SyncGetCharacterIndexAtPoint(
      const gfx::PointF& root_point,
      SyncGetCharacterIndexAtPointCallback callback) override;
  bool SyncGetCharacterIndexAtPoint(const gfx::PointF& root_point,
                                    uint32_t* index) override;
  void SyncGetFirstRectForRange(
      const gfx::Range& requested_range,
      SyncGetFirstRectForRangeCallback callback) override;
  bool SyncGetFirstRectForRange(const gfx::Range& requested_range,
                                gfx::Rect* out_rect,
                                gfx::Range* out_actual_range,
                                bool* out_success) override;
  void ExecuteEditCommand(const std::string& command) override;
  void Undo() override;
  void Redo() override;
  void Cut() override;
  void Copy() override;
  void CopyToFindPboard() override;
  void CenterSelection() override;
  void Paste() override;
  void PasteAndMatchStyle() override;
  void SelectAll() override;
  void StartSpeaking() override;
  void StopSpeaking() override;
  bool SyncIsSpeaking(bool* is_speaking) override;
  void GetRenderWidgetAccessibilityToken(
      GetRenderWidgetAccessibilityTokenCallback callback) override;
  void SyncIsSpeaking(SyncIsSpeakingCallback callback) override;
  void SetRemoteAccessibilityWindowToken(
      const std::vector<uint8_t>& window_token) override;

  // BrowserCompositorMacClient implementation.
  SkColor BrowserCompositorMacGetGutterColor() const override;
  void OnFrameTokenChanged(uint32_t frame_token,
                           base::TimeTicks activation_time) override;
  void DestroyCompositorForShutdown() override;
  bool OnBrowserCompositorSurfaceIdChanged() override;
  std::vector<viz::SurfaceId> CollectSurfaceIdsForEviction() override;
  display::ScreenInfo GetCurrentScreenInfo() const override;
  void SetCurrentDeviceScaleFactor(float device_scale_factor) override;

  // AcceleratedWidgetMacNSView implementation.
  void AcceleratedWidgetCALayerParamsUpdated() override;

  // ui::AccessibilityFocusOverrider::Client:
  id GetAccessibilityFocusedUIElement() override;

  void SetShowingContextMenu(bool showing) override;

  // Helper method to obtain ui::TextInputType for the active widget from the
  // TextInputManager.
  ui::TextInputType GetTextInputType();

  // Helper method to obtain the currently active widget from TextInputManager.
  // An active widget is a RenderWidget which is currently focused and has a
  // |TextInputState.type| which is not ui::TEXT_INPUT_TYPE_NONE.
  RenderWidgetHostImpl* GetActiveWidget();

  // Returns the composition range information for the active RenderWidgetHost
  // (accepting IME and keyboard input).
  const TextInputManager::CompositionRangeInfo* GetCompositionRangeInfo();

  // Returns the TextSelection information for the active widget.
  const TextInputManager::TextSelection* GetTextSelection();

  // Get the focused view that should be used for retrieving the text selection.
  RenderWidgetHostViewBase* GetFocusedViewForTextSelection();

  // Returns the RenderWidgetHostDelegate corresponding to the currently focused
  // RenderWidgetHost. It is different from |render_widget_host_->delegate()|
  // when there are focused inner WebContentses on the page. Also, this method
  // can return nullptr; for instance when |render_widget_host_| becomes nullptr
  // in the destruction path of the WebContentsImpl.
  RenderWidgetHostDelegate* GetFocusedRenderWidgetHostDelegate();

  // Returns the RenderWidgetHostImpl to which Ime messages from the NSView
  // should be targeted. This exists to preserve historical behavior, and may
  // not be the desired behavior.
  // https://crbug.com/831843
  RenderWidgetHostImpl* GetWidgetForIme();

  // When inside a block of handling a keyboard event, returns the
  // RenderWidgetHostImpl to which all keyboard and Ime messages from the NSView
  // should be forwarded. This exists to preserve historical behavior, and may
  // not be the desired behavior.
  // https://crbug.com/831843
  RenderWidgetHostImpl* GetWidgetForKeyboardEvent();

  // Migrate the NSView for this RenderWidgetHostView to be in the process at
  // the other end of |remote_cocoa_application|, and make it a child view of
  // the NSView referred to by |parent_ns_view_id|.
  void MigrateNSViewBridge(
      remote_cocoa::mojom::Application* remote_cocoa_application,
      uint64_t parent_ns_view_id);

  // Specify a ui::Layer into which the renderer's content should be
  // composited. If nullptr is specified, then this layer will create a
  // separate ui::Compositor as needed (e.g, for tab capture).
  void SetParentUiLayer(ui::Layer* parent_ui_layer);

  // Specify the element to return as the accessibility parent of the
  // |cocoa_view_|.
  void SetParentAccessibilityElement(id parent_accessibility_element);

  RenderWidgetHostViewMac* PopupChildHostView() {
    return popup_child_host_view_;
  }

  MouseWheelPhaseHandler* GetMouseWheelPhaseHandler() override;

  void ShowSharePicker(
      const std::string& title,
      const std::string& text,
      const std::string& url,
      const std::vector<std::string>& file_paths,
      blink::mojom::ShareService::ShareCallback callback) override;

 protected:
  // This class is to be deleted through the Destroy method.
  ~RenderWidgetHostViewMac() override;

 private:
  friend class RenderWidgetHostViewMacTest;
  friend class MockPointerLockRenderWidgetHostView;
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewMacTest, GetPageTextForSpeech);

  // Shuts down the render_widget_host_.  This is a separate function so we can
  // invoke it from the message loop.
  void ShutdownHost();

  // Send updated vsync parameters to the top level display.
  void UpdateDisplayVSyncParameters();

  void SendSyntheticWheelEventWithPhaseEnded(
      blink::WebMouseWheelEvent wheel_event,
      bool should_route_event);

  void OnDidUpdateVisualPropertiesComplete(
      const cc::RenderFrameMetadata& metadata);

  void OnGotStringForDictionaryOverlay(
      int32_t targetWidgetProcessId,
      int32_t targetWidgetRoutingId,
      ui::mojom::AttributedStringPtr attributed_string,
      const gfx::Point& baseline_point_in_layout_space);

  // RenderWidgetHostViewBase:
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

  // Gets a textual view of the page's contents, and passes it to the callback
  // provided.
  using SpeechCallback = base::OnceCallback<void(const std::u16string&)>;
  void GetPageTextForSpeech(SpeechCallback callback);

  // Calls RenderWidgetHostNSView::SetTooltipText and call the observer's
  // OnTooltipTextUpdated() function, if not null.
  void SetTooltipText(const std::u16string& tooltip_text);

  void UpdateWindowsNow();

  // Interface through which the NSView is to be manipulated. This points either
  // to |in_process_ns_view_bridge_| or to |remote_ns_view_|.
  raw_ptr<remote_cocoa::mojom::RenderWidgetHostNSView> ns_view_ = nullptr;

  // If |ns_view_| is hosted in this process, then this will be non-null,
  // and may be used to query the actual RenderWidgetHostViewCocoa that is being
  // used for |this|. Any functionality that uses |new_view_bridge_local_| will
  // not work when the RenderWidgetHostViewCocoa is hosted in an app process.
  std::unique_ptr<remote_cocoa::RenderWidgetHostNSViewBridge>
      in_process_ns_view_bridge_;

  // If the NSView is hosted in a remote process and accessed via mojo then
  // - |ns_view_| will point to |remote_ns_view_|
  // - |remote_ns_view_client_receiver_| is the receiver provided to the bridge.
  mojo::AssociatedRemote<remote_cocoa::mojom::RenderWidgetHostNSView>
      remote_ns_view_;
  mojo::AssociatedReceiver<remote_cocoa::mojom::RenderWidgetHostNSViewHost>
      remote_ns_view_client_receiver_{this};

  // State tracked by Show/Hide/IsShowing.
  bool is_visible_ = false;

  // The bounds of the view in its NSWindow's coordinate system (with origin
  // in the upper-left).
  gfx::Rect view_bounds_in_window_dip_;

  // The frame of the window in the global display::Screen coordinate system
  // (where the origin is the upper-left corner of Screen::GetPrimaryDisplay).
  gfx::Rect window_frame_in_screen_dip_;

  // Whether or not the NSView's NSWindow is the key window.
  bool is_window_key_ = false;

  // Whether or not the NSView is first responder.
  bool is_first_responder_ = false;

  // Whether Focus() is being called.
  bool is_getting_focus_ = false;

  // Indicates if the page is loading.
  bool is_loading_;

  // Our parent host view, if this is a popup.  NULL otherwise.
  raw_ptr<RenderWidgetHostViewMac> popup_parent_host_view_;

  // Our child popup host. NULL if we do not have a child popup.
  raw_ptr<RenderWidgetHostViewMac> popup_child_host_view_;

  // Whether or not the background is opaque as determined by calls to
  // SetBackgroundColor. The default value is opaque.
  bool background_is_opaque_ = true;

  // The color of the background CALayer, stored as a SkColor for efficient
  // comparison. Initially transparent so that the embedding NSView shows
  // through.
  SkColor background_layer_color_ = SK_ColorTRANSPARENT;

  // The background color of the last frame that was swapped. This is not
  // applied until the swap completes (see comments in
  // AcceleratedWidgetCALayerParamsUpdated).
  SkColor last_frame_root_background_color_ = SK_ColorTRANSPARENT;

  std::unique_ptr<input::CursorManager> cursor_manager_;

  // Observes macOS's accessibility pointer size user preference changes.
  id<NSObject> __strong cursor_scale_observer_;

  // Used to track active password input sessions.
  std::unique_ptr<ui::ScopedPasswordInputEnabler> password_input_enabler_;

  // Provides gesture synthesis given a stream of touch events and touch event
  // acks. This is for generating gesture events from injected touch events.
  ui::FilteredGestureProvider gesture_provider_;

  // Used to ensure that a consistent RenderWidgetHost is targeted throughout
  // the duration of a keyboard event.
  bool in_keyboard_event_ = false;
  int32_t keyboard_event_widget_process_id_ = 0;
  int32_t keyboard_event_widget_routing_id_ = 0;

  // When a gesture starts, the system does not inform the view of which type
  // of gesture is happening (magnify, rotate, etc), rather, it just informs
  // the view that some as-yet-undefined gesture is starting. Capture the
  // information about the gesture's beginning event here. It will be used to
  // create a specific gesture begin event later.
  std::unique_ptr<blink::WebGestureEvent> gesture_begin_event_;

  // This is set if a GesturePinchBegin event has been sent in the lifetime of
  // |gesture_begin_event__|. If set, a GesturePinchEnd will be sent when the
  // gesture ends.
  bool gesture_begin_pinch_sent_ = false;

  // To avoid accidental pinches, require that a certain zoom threshold be
  // reached before forwarding it to the browser. Use |pinch_unused_amount_| to
  // hold this value. If the user reaches this value, don't re-require the
  // threshold be reached until the page has been zoomed back to page scale of
  // one.
  bool pinch_has_reached_zoom_threshold_ = false;
  float pinch_unused_amount_ = 1.f;

  // Tracks whether keyboard lock is active.
  bool is_keyboard_locked_ = false;

  // While the mouse is locked, the cursor is hidden from the user. Mouse events
  // are still generated. However, the position they report is the last known
  // mouse position just as mouse lock was entered; the movement they report
  // indicates what the change in position of the mouse would be had it not been
  // locked.
  bool pointer_locked_ = false;

  // Tracks whether unaccelerated mouse motion events are sent while the mouse
  // is locked.
  bool pointer_lock_unadjusted_movement_ = false;

  // Latest capture sequence number which is incremented when the caller
  // requests surfaces be synchronized via
  // EnsureSurfaceSynchronizedForWebTest().
  uint32_t latest_capture_sequence_number_ = 0u;

  // Remote accessibility objects corresponding to the NSWindow that this is
  // displayed to the user in.
  NSAccessibilityRemoteUIElement* __strong remote_window_accessible_;

  // Used to force the NSApplication's focused accessibility element to be the
  // content::BrowserAccessibilityCocoa accessibility tree when the NSView for
  // this is focused.
  ui::AccessibilityFocusOverrider accessibility_focus_overrider_;

  // Holds the latest ScreenInfos sent from the remote process to be used
  // in UpdateScreenInfo.  Other platforms check display::Screen for the current
  // set of displays, but Mac has this info delivered explicitly and so can't do
  // that.  This is therefore an out-of-band parameter to UpdateScreenInfo.
  // This also allows the screen_infos_ to only be updated outside of resize by
  // holding any updates temporarily in this variable.
  std::optional<display::ScreenInfos> new_screen_infos_from_shim_;
  display::ScreenInfos original_screen_infos_;

  // Represents a feature of the physical display whose offset and mask_length
  // are expressed in DIPs relative to the view. See display_feature.h for more
  // details.
  std::optional<DisplayFeature> display_feature_;

  const uint64_t ns_view_id_;

  // See description of `kDelayUpdateWindowsAfterTextInputStateChanged` for
  // details.
  base::OneShotTimer update_windows_timer_;

  // Factory used to safely scope delayed calls to ShutdownHost().
  base::WeakPtrFactory<RenderWidgetHostViewMac> weak_factory_;
};

// RenderWidgetHostViewCocoa is not exported outside of content. This helper
// method provides the tests with the class object so that they can override the
// methods according to the tests' requirements.
CONTENT_EXPORT Class GetRenderWidgetHostViewCocoaClassForTesting();

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_MAC_H_
