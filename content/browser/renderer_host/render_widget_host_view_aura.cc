// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_aura.h"

#include <limits>
#include <memory>
#include <set>
#include <string_view>
#include <utility>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "cc/layers/layer.h"
#include "cc/trees/layer_tree_settings.h"
#include "components/input/cursor_manager.h"
#include "components/input/render_widget_host_input_event_router.h"
#include "components/stylus_handwriting/win/features.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "content/browser/bad_message.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/renderer_host/delegated_frame_host_client_aura.h"
#include "content/browser/renderer_host/dip_util.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/input/synthetic_gesture_target_aura.h"
#include "content/browser/renderer_host/input/touch_selection_controller_client_aura.h"
#include "content/browser/renderer_host/overscroll_controller.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_delegate_view.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_event_handler.h"
#include "content/browser/renderer_host/visible_time_request_trigger.h"
#include "content/common/input/events_helper.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/device_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/page_visibility_state.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/mojom/widget/record_content_to_visible_time_request.mojom.h"
#include "ui/accessibility/aura/aura_window_properties.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/accessibility/platform/browser_accessibility_manager.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/cursor_client_observer.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/client/transient_window_client.h"
#include "ui/aura/client/window_parenting_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_tree_host.h"
#include "ui/aura_extra/window_position_in_root_monitor.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/hit_test.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/mojom/text_input_state.mojom.h"
#include "ui/base/owned_window_anchor.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches.h"
#include "ui/base/ui_base_types.h"
#include "ui/display/screen.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/blink/did_overscroll_params.h"
#include "ui/events/blink/web_input_event.h"
#include "ui/events/event.h"
#include "ui/events/event_observer.h"
#include "ui/events/event_utils.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/events/gestures/gesture_recognizer.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/touch_selection/touch_selection_controller.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/public/activation_client.h"
#include "ui/wm/public/scoped_tooltip_disabler.h"
#include "ui/wm/public/tooltip_client.h"

#if BUILDFLAG(IS_WIN)
#include "base/time/time.h"
#include "content/browser/renderer_host/legacy_render_widget_host_win.h"
#include "ui/accessibility/platform/ax_fragment_root_win.h"
#include "ui/accessibility/platform/browser_accessibility_manager_win.h"
#include "ui/accessibility/platform/browser_accessibility_win.h"
#include "ui/base/ime/virtual_keyboard_controller.h"
#include "ui/base/ime/virtual_keyboard_controller_observer.h"
#include "ui/base/ime/win/tsf_input_scope.h"
#include "ui/base/win/hidden_window.h"
#include "ui/display/win/screen_win.h"
#include "ui/gfx/gdi_util.h"
#endif

#if BUILDFLAG(IS_LINUX)
#include "ui/accessibility/platform/browser_accessibility_auralinux.h"
#include "ui/base/ime/linux/text_edit_command_auralinux.h"
#include "ui/base/ime/text_input_flags.h"
#include "ui/linux/linux_ui.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ui/wm/core/ime_util_chromeos.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "ui/base/ime/mojom/virtual_keyboard_types.mojom.h"
#endif

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

using gfx::RectToSkIRect;
using gfx::SkIRectToRect;

using blink::WebInputEvent;
using blink::WebGestureEvent;
using blink::WebTouchEvent;

namespace content {

namespace {
// Guards CHECKing that the UI compositor LSI is only ever invalid when
// `RenderWidgetHost` is hidden.
// TODO(crbug.com/330301468): Remove this once we determine the cause of failure
// to reallocate an LSI for the UI compositor.
BASE_FEATURE(kRenderWidgetHostHiddenCheck,
             "RenderWidgetHostHiddenCheck",
// TODO(b/338354134): LaCrOs video is triggering the associated CHECK. Disable
// for that configuration.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
             base::FEATURE_DISABLED_BY_DEFAULT);
#else
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif
}  // namespace

// We need to watch for mouse events outside a Web Popup or its parent
// and dismiss the popup for certain events.
class RenderWidgetHostViewAura::EventObserverForPopupExit
    : public ui::EventObserver {
 public:
  explicit EventObserverForPopupExit(RenderWidgetHostViewAura* rwhva)
      : rwhva_(rwhva) {
    aura::Env* env = aura::Env::GetInstance();
    env->AddEventObserver(
        this, env,
        {ui::EventType::kMousePressed, ui::EventType::kTouchPressed});
  }

  EventObserverForPopupExit(const EventObserverForPopupExit&) = delete;
  EventObserverForPopupExit& operator=(const EventObserverForPopupExit&) =
      delete;

  ~EventObserverForPopupExit() override {
    aura::Env::GetInstance()->RemoveEventObserver(this);
  }

  // ui::EventObserver:
  void OnEvent(const ui::Event& event) override {
    rwhva_->ApplyEventObserverForPopupExit(*event.AsLocatedEvent());
  }

 private:
  raw_ptr<RenderWidgetHostViewAura> rwhva_;
};

void RenderWidgetHostViewAura::ApplyEventObserverForPopupExit(
    const ui::LocatedEvent& event) {
  CHECK(event.type() == ui::EventType::kMousePressed ||
        event.type() == ui::EventType::kTouchPressed);

  if (in_shutdown_)
    return;

  // |target| may be null.
  aura::Window* target = static_cast<aura::Window*>(event.target());
  if (target != window_ &&
      (!popup_parent_host_view_ ||
       target != popup_parent_host_view_->window_)) {
    // If we enter this code path it means that we did not receive any focus
    // lost notifications for the popup window. Ensure that blink is aware
    // of the fact that focus was lost for the host window by sending a Blur
    // notification. We also set a flag in the view indicating that we need
    // to force a Focus notification on the next mouse down.
    if (popup_parent_host_view_ && popup_parent_host_view_->host()) {
      popup_parent_host_view_->event_handler()
          ->set_focus_on_mouse_down_or_key_event(true);
      popup_parent_host_view_->host()->Blur();
    }
    // Note: popup_parent_host_view_ may be NULL when there are multiple
    // popup children per view. See: RenderWidgetHostViewAura::InitAsPopup().
    Shutdown();
  }
}

// We have to implement the WindowObserver interface on a separate object
// because clang doesn't like implementing multiple interfaces that have
// methods with the same name. This object is owned by the
// RenderWidgetHostViewAura.
class RenderWidgetHostViewAura::WindowObserver : public aura::WindowObserver {
 public:
  explicit WindowObserver(RenderWidgetHostViewAura* view)
      : view_(view) {
    view_->window_->AddObserver(this);
  }

  WindowObserver(const WindowObserver&) = delete;
  WindowObserver& operator=(const WindowObserver&) = delete;

  ~WindowObserver() override { view_->window_->RemoveObserver(this); }

  // Overridden from aura::WindowObserver:
  void OnWindowAddedToRootWindow(aura::Window* window) override {
    if (window == view_->window_)
      view_->AddedToRootWindow();
  }

  void OnWindowRemovingFromRootWindow(aura::Window* window,
                                      aura::Window* new_root) override {
    if (window == view_->window_)
      view_->RemovingFromRootWindow();
  }

  void OnWindowHierarchyChanged(const HierarchyChangeParams& params) override {
    view_->ParentHierarchyChanged();
  }

  void OnWindowTitleChanged(aura::Window* window) override {
    if (window == view_->window_)
      view_->WindowTitleChanged();
  }

 private:
  raw_ptr<RenderWidgetHostViewAura> view_;
};

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, public:

RenderWidgetHostViewAura::RenderWidgetHostViewAura(
    RenderWidgetHost* widget_host)
    : RenderWidgetHostViewBase(widget_host),
      window_(nullptr),
      in_shutdown_(false),
      in_bounds_changed_(false),
      popup_parent_host_view_(nullptr),
      popup_child_host_view_(nullptr),
      is_loading_(false),
      has_composition_text_(false),
      added_frame_observer_(false),
      cursor_visibility_state_in_renderer_(UNKNOWN),
      device_scale_factor_(0.0f),
      event_handler_(new RenderWidgetHostViewEventHandler(host(), this, this)),
      frame_sink_id_(host()->GetFrameSinkId()),
      visibility_(host()->is_hidden() ? Visibility::HIDDEN
                                      : Visibility::VISIBLE) {
  // CreateDelegatedFrameHostClient() and CreateAuraWindow() assume that the
  // FrameSinkId is valid. RenderWidgetHostImpl::GetFrameSinkId() always returns
  // a valid FrameSinkId.
  CHECK(frame_sink_id_.is_valid());

  CreateDelegatedFrameHostClient();

  host()->SetView(this);

  // We should start observing the TextInputManager for IME-related events as
  // well as monitoring its lifetime.
  if (GetTextInputManager())
    GetTextInputManager()->AddObserver(this);

  cursor_manager_ = std::make_unique<input::CursorManager>(this);

  selection_controller_client_ =
      std::make_unique<TouchSelectionControllerClientAura>(this);
  CreateSelectionController();

  RenderWidgetHostOwnerDelegate* owner_delegate = host()->owner_delegate();
  if (owner_delegate) {
    // NOTE: This will not be run for child frame widgets, which do not have
    // an owner delegate and won't get a RenderViewHost here.
    double_tap_to_zoom_enabled_ =
        owner_delegate->GetWebkitPreferencesForWidget()
            .double_tap_to_zoom_enabled;
  }

  host()->render_frame_metadata_provider()->AddObserver(this);
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, RenderWidgetHostView implementation:

void RenderWidgetHostViewAura::InitAsChild(gfx::NativeView parent_view) {
  CHECK_EQ(widget_type_, WidgetType::kFrame);
  CreateAuraWindow(aura::client::WINDOW_TYPE_CONTROL);

  if (parent_view)
    parent_view->AddChild(GetNativeView());

  device_scale_factor_ = GetDeviceScaleFactor();

  aura::Window* root = window_->GetRootWindow();
  if (root) {
    auto* cursor_client = aura::client::GetCursorClient(root);
    if (cursor_client)
      UpdateSystemCursorSize(cursor_client->GetSystemCursorSize());
  }

#if BUILDFLAG(IS_WIN)
  // This will fetch and set the display features.
  ObserveDevicePosturePlatformProvider();

  // We want to set input scope once after the window is created
  // and before any focus change happens. We want to set it only
  // once for each hwnd.
  if (window_->GetHost() && GetInputMethod()) {
    InputScope input_scope = ShouldDoLearning() ? IS_DEFAULT : IS_PRIVATE;
    ui::tsf_inputscope::SetInputScope(
        RenderWidgetHostViewAura::GetHostWindowHWND(), input_scope);
  }
#endif
}

void RenderWidgetHostViewAura::InitAsPopup(
    RenderWidgetHostView* parent_host_view,
    const gfx::Rect& bounds_in_screen,
    const gfx::Rect& anchor_rect) {
  CHECK_EQ(widget_type_, WidgetType::kPopup);
  CHECK(!static_cast<RenderWidgetHostViewBase*>(parent_host_view)
             ->IsRenderWidgetHostViewChildFrame());

  popup_parent_host_view_ =
      static_cast<RenderWidgetHostViewAura*>(parent_host_view);

  // TransientWindowClient may be NULL during tests.
  aura::client::TransientWindowClient* transient_window_client =
      aura::client::GetTransientWindowClient();
  RenderWidgetHostViewAura* old_child =
      popup_parent_host_view_->popup_child_host_view_;
  if (old_child) {
    // TODO(jhorwich): Allow multiple popup_child_host_view_ per view, or
    // similar mechanism to ensure a second popup doesn't cause the first one
    // to never get a chance to filter events. See crbug.com/160589.
    CHECK(old_child->popup_parent_host_view_ == popup_parent_host_view_);
    if (transient_window_client) {
      transient_window_client->RemoveTransientChild(
        popup_parent_host_view_->window_, old_child->window_);
    }
    old_child->popup_parent_host_view_ = nullptr;
  }
  popup_parent_host_view_->SetPopupChild(this);
  CreateAuraWindow(aura::client::WINDOW_TYPE_MENU);
  // Use transparent background color for the popup in order to avoid flashing
  // the white background on popup open when dark color-scheme is used.
  SetContentBackgroundColor(SK_ColorTRANSPARENT);

  // Setting the transient child allows for the popup to get mouse events when
  // in a system modal dialog. Do this before calling ParentWindowWithContext
  // below so that the transient parent is visible to WindowTreeClient.
  // This fixes crbug.com/328593.
  if (transient_window_client) {
    transient_window_client->AddTransientChild(
        popup_parent_host_view_->window_, window_);
  }

  ui::OwnedWindowAnchor owned_window_anchor = {
      anchor_rect, ui::OwnedWindowAnchorPosition::kBottomLeft,
      ui::OwnedWindowAnchorGravity::kBottomRight,
      ui::OwnedWindowConstraintAdjustment::kAdjustmentFlipY};
  window_->SetProperty(aura::client::kOwnedWindowAnchor, owned_window_anchor);

  aura::Window* root = popup_parent_host_view_->window_->GetRootWindow();
  aura::client::ParentWindowWithContext(window_, root, bounds_in_screen,
                                        display::kInvalidDisplayId);

  SetBounds(bounds_in_screen);
  Show();
  if (NeedsMouseCapture())
    window_->SetCapture();

  event_observer_for_popup_exit_ =
      std::make_unique<EventObserverForPopupExit>(this);

  device_scale_factor_ = GetDeviceScaleFactor();

  // If HiDPI capture mode is active for the parent, propagate the scale
  // override to the popup window also. Its content was created assuming
  // that the new window will share the parent window's scale. See
  // https://crbug.com/1354703 .
  SetScaleOverrideForCapture(
      popup_parent_host_view_->GetScaleOverrideForCapture());

  auto* cursor_client = aura::client::GetCursorClient(root);
  if (cursor_client)
    UpdateSystemCursorSize(cursor_client->GetSystemCursorSize());

#if BUILDFLAG(IS_WIN)
  // This will fetch and set the display features.
  ObserveDevicePosturePlatformProvider();
#endif
}

void RenderWidgetHostViewAura::Hide() {
  window_->Hide();
  visibility_ = Visibility::HIDDEN;
  HideImpl();
}

void RenderWidgetHostViewAura::SetSize(const gfx::Size& size) {
  // For a SetSize operation, we don't care what coordinate system the origin
  // of the window is in, it's only important to make sure that the origin
  // remains constant after the operation.
  InternalSetBounds(gfx::Rect(window_->bounds().origin(), size));
}

void RenderWidgetHostViewAura::SetBounds(const gfx::Rect& rect) {
  gfx::Point relative_origin(rect.origin());

  // RenderWidgetHostViewAura::SetBounds() takes screen coordinates, but
  // Window::SetBounds() takes parent coordinates, so do the conversion here.
  aura::Window* root = window_->GetRootWindow();
  if (root) {
    aura::client::ScreenPositionClient* screen_position_client =
        aura::client::GetScreenPositionClient(root);
    if (screen_position_client) {
      screen_position_client->ConvertPointFromScreen(window_->parent(),
                                                     &relative_origin);
    }
  }

  InternalSetBounds(gfx::Rect(relative_origin, rect.size()));
}

gfx::NativeView RenderWidgetHostViewAura::GetNativeView() {
  return window_;
}

#if BUILDFLAG(IS_WIN)
HWND RenderWidgetHostViewAura::GetHostWindowHWND() const {
  aura::WindowTreeHost* host = window_->GetHost();
  return host ? host->GetAcceleratedWidget() : nullptr;
}
#endif

gfx::NativeViewAccessible RenderWidgetHostViewAura::GetNativeViewAccessible() {
#if BUILDFLAG(IS_WIN)
  aura::WindowTreeHost* window_host = window_->GetHost();
  if (!window_host)
    return static_cast<gfx::NativeViewAccessible>(NULL);

  ui::BrowserAccessibilityManager* manager =
      host()->GetOrCreateRootBrowserAccessibilityManager();
  if (manager)
    return ToBrowserAccessibilityWin(manager->GetBrowserAccessibilityRoot())
        ->GetCOM();

#elif BUILDFLAG(IS_LINUX)
  ui::BrowserAccessibilityManager* manager =
      host()->GetOrCreateRootBrowserAccessibilityManager();
  if (manager && manager->GetBrowserAccessibilityRoot())
    return manager->GetBrowserAccessibilityRoot()->GetNativeViewAccessible();
#endif

  NOTIMPLEMENTED_LOG_ONCE();
  return static_cast<gfx::NativeViewAccessible>(nullptr);
}

ui::TextInputClient* RenderWidgetHostViewAura::GetTextInputClient() {
  return this;
}

RenderFrameHostImpl* RenderWidgetHostViewAura::GetFocusedFrame() const {
  FrameTreeNode* focused_frame = host()->frame_tree()->GetFocusedFrame();
  if (!focused_frame)
    return nullptr;
  return focused_frame->current_frame_host();
}

void RenderWidgetHostViewAura::HandleBoundsInRootChanged() {
#if BUILDFLAG(IS_WIN)
  if (legacy_render_widget_host_HWND_) {
    legacy_render_widget_host_HWND_->SetBounds(
        window_->GetBoundsInRootWindow());
  }
#endif
  if (!in_shutdown_) {
    // Send screen rects through the delegate if there is one. Not every
    // RenderWidgetHost has a delegate (for example, drop-down widgets).
    if (host_->delegate())
      host_->delegate()->SendScreenRects();
    else
      host_->SendScreenRects();
  }

  UpdateInsetsWithVirtualKeyboardEnabled();
}

void RenderWidgetHostViewAura::ParentHierarchyChanged() {
  if (window_->parent()) {
    // Track changes of the window relative to the root. This is done to snap
    // `window_` to a pixel boundary, which could change when the bounds
    // relative to the root changes. An example where this happens:
    // The fast resize code path for bookmarks where in the parent of RWHVA
    // which is WCV has its bounds changed before the bookmark is hidden. This
    // results in the traditional bounds change notification for the WCV
    // reporting the old bounds as the bookmark is still around. Observing all
    // the ancestors of the RWHVA window enables us to know when the bounds of
    // the window relative to root changes and allows us to snap accordingly.
    position_in_root_observer_ =
        std::make_unique<aura_extra::WindowPositionInRootMonitor>(
            window_->parent(),
            base::BindRepeating(
                &RenderWidgetHostViewAura::HandleBoundsInRootChanged,
                base::Unretained(this)));
  } else {
    position_in_root_observer_.reset();
  }
  // Snap when we receive a hierarchy changed. http://crbug.com/388908.
  HandleBoundsInRootChanged();
}

void RenderWidgetHostViewAura::Focus() {
  // Make sure we have a FocusClient before attempting to Focus(). In some
  // situations we may not yet be in a valid Window hierarchy (such as reloading
  // after out of memory discarded the tab).
  aura::client::FocusClient* client = aura::client::GetFocusClient(window_);
  if (client)
    window_->Focus();
}

bool RenderWidgetHostViewAura::HasFocus() {
  return window_->HasFocus();
}

bool RenderWidgetHostViewAura::IsSurfaceAvailableForCopy() {
  CHECK(delegated_frame_host_) << "Cannot be invoked during destruction.";
  return delegated_frame_host_->CanCopyFromCompositingSurface();
}

void RenderWidgetHostViewAura::EnsureSurfaceSynchronizedForWebTest() {
  ++latest_capture_sequence_number_;
  SynchronizeVisualProperties(cc::DeadlinePolicy::UseInfiniteDeadline(),
                              std::nullopt);
}

bool RenderWidgetHostViewAura::IsShowing() {
  return window_->IsVisible();
}

void RenderWidgetHostViewAura::WasUnOccluded() {
  ShowImpl(PageVisibilityState::kVisible);
}

void RenderWidgetHostViewAura::ShowImpl(PageVisibilityState page_visibility) {
  // OnShowWithPageVisibility will not call NotifyHostAndDelegateOnWasShown,
  // which updates `visibility_`, unless the host is hidden. Make sure no update
  // is needed.
  CHECK(host_->is_hidden() || visibility_ == Visibility::VISIBLE);
  OnShowWithPageVisibility(page_visibility);
}

void RenderWidgetHostViewAura::EnsurePlatformVisibility(
    PageVisibilityState page_visibility) {
  // TODO(crbug.com/330301468): Remove this once we determine the cause of
  // failure to reallocate an LSI for the UI compositor.
  auto* wth = window()->GetHost();
  if (wth && !wth->window()->GetLocalSurfaceId().is_valid() &&
      base::FeatureList::IsEnabled(kRenderWidgetHostHiddenCheck)) {
    CHECK(host()->is_hidden());
  }
}

void RenderWidgetHostViewAura::NotifyHostAndDelegateOnWasShown(
    blink::mojom::RecordContentToVisibleTimeRequestPtr tab_switch_start_state) {
  CHECK(delegated_frame_host_) << "Cannot be invoked during destruction.";
  CHECK(host_->is_hidden());
  CHECK_NE(visibility_, Visibility::VISIBLE);

  visibility_ = Visibility::VISIBLE;

  bool has_saved_frame = delegated_frame_host_->HasSavedFrame();

  bool show_reason_bfcache_restore =
      tab_switch_start_state
          ? tab_switch_start_state->show_reason_bfcache_restore
          : false;

  // No need to check for saved frames for the case of bfcache restore.
  if (show_reason_bfcache_restore) {
    host()->WasShown(tab_switch_start_state.Clone());
  } else {
    host()->WasShown(has_saved_frame
                         ? blink::mojom::RecordContentToVisibleTimeRequestPtr()
                         : tab_switch_start_state.Clone());
  }
  aura::Window* root = window_->GetRootWindow();
  if (root) {
    aura::client::CursorClient* cursor_client =
        aura::client::GetCursorClient(root);
    if (cursor_client) {
      NotifyRendererOfCursorVisibilityState(cursor_client->IsCursorVisible());
    }
  }

  // If the frame for the renderer is already available, then the
  // tab-switching time is the presentation time for the browser-compositor.
  delegated_frame_host_->WasShown(
      GetLocalSurfaceId(), window_->bounds().size(),
      has_saved_frame ? std::move(tab_switch_start_state)
                      : blink::mojom::RecordContentToVisibleTimeRequestPtr());

#if BUILDFLAG(IS_WIN)
  UpdateLegacyWin();
#endif
}

void RenderWidgetHostViewAura::HideImpl() {
  CHECK(delegated_frame_host_) << "Cannot be invoked during destruction.";
  CHECK(visibility_ == Visibility::HIDDEN ||
        visibility_ == Visibility::OCCLUDED);

  if (!host()->is_hidden()) {
    host()->WasHidden();
    aura::WindowTreeHost* host = window_->GetHost();
      aura::Window* parent = window_->parent();
      aura::Window::OcclusionState parent_occl_state =
          parent ? parent->GetOcclusionState()
                 : aura::Window::OcclusionState::UNKNOWN;
      aura::Window::OcclusionState native_win_occlusion_state =
          host ? host->GetNativeWindowOcclusionState()
               : aura::Window::OcclusionState::UNKNOWN;
      DelegatedFrameHost::HiddenCause cause;
      if (parent_occl_state == aura::Window::OcclusionState::OCCLUDED &&
          native_win_occlusion_state ==
              aura::Window::OcclusionState::OCCLUDED) {
        cause = DelegatedFrameHost::HiddenCause::kOccluded;
      } else {
        cause = DelegatedFrameHost::HiddenCause::kOther;
      }
      delegated_frame_host_->WasHidden(cause);
#if BUILDFLAG(IS_WIN)
      if (host && legacy_render_widget_host_HWND_) {
        // We reparent the legacy Chrome_RenderWidgetHostHWND window to the
        // global hidden window on the same lines as Windowed plugin windows.
        legacy_render_widget_host_HWND_->UpdateParent(ui::GetHiddenWindow());
      }
#endif
  }

#if BUILDFLAG(IS_WIN)
  if (legacy_render_widget_host_HWND_)
    legacy_render_widget_host_HWND_->Hide();
#endif
}

void RenderWidgetHostViewAura::WasOccluded() {
  visibility_ = Visibility::OCCLUDED;
  HideImpl();
}

void RenderWidgetHostViewAura::
    RequestSuccessfulPresentationTimeFromHostOrDelegate(
        blink::mojom::RecordContentToVisibleTimeRequestPtr
            visible_time_request) {
  CHECK(delegated_frame_host_) << "Cannot be invoked during destruction.";
  CHECK(!host_->is_hidden());
  CHECK_EQ(visibility_, Visibility::VISIBLE);
  CHECK(visible_time_request);

  bool has_saved_frame = delegated_frame_host_->HasSavedFrame();

  // No need to check for saved frames for the case of bfcache restore.
  if (visible_time_request->show_reason_bfcache_restore || !has_saved_frame) {
    host()->RequestSuccessfulPresentationTimeForNextFrame(
        visible_time_request.Clone());
  }

  // If the frame for the renderer is already available, then the
  // tab-switching time is the presentation time for the browser-compositor.
  if (has_saved_frame) {
    delegated_frame_host_->RequestSuccessfulPresentationTimeForNextFrame(
        std::move(visible_time_request));
  }
}

void RenderWidgetHostViewAura::
    CancelSuccessfulPresentationTimeRequestForHostAndDelegate() {
  CHECK(delegated_frame_host_) << "Cannot be invoked during destruction.";
  CHECK(!host_->is_hidden());
  CHECK_EQ(visibility_, Visibility::VISIBLE);

  host()->CancelSuccessfulPresentationTimeRequest();
  delegated_frame_host_->CancelSuccessfulPresentationTimeRequest();
}

viz::SurfaceId RenderWidgetHostViewAura::GetFallbackSurfaceIdForTesting()
    const {
  return delegated_frame_host_->GetFallbackSurfaceIdForTesting();  // IN-TEST
}

bool RenderWidgetHostViewAura::ShouldSkipCursorUpdate() const {
  aura::Window* root_window = window_->GetRootWindow();
  CHECK(root_window);
  display::Screen* screen = display::Screen::GetScreen();
  CHECK(screen);

  // Ignore cursor update messages if the window under the cursor is not us.
#if BUILDFLAG(IS_WIN)
  gfx::Point cursor_screen_point = screen->GetCursorScreenPoint();
  aura::Window* window = screen->GetWindowAtScreenPoint(cursor_screen_point);
  if (!window || window->GetRootWindow() != root_window) {
    return true;
  }
#elif !BUILDFLAG(IS_CHROMEOS_ASH)
  if (!screen->IsWindowUnderCursor(root_window))
    return true;
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
  return false;
}

bool RenderWidgetHostViewAura::ShouldShowStaleContentOnEviction() {
  return host() && host()->ShouldShowStaleContentOnEviction();
}

gfx::Rect RenderWidgetHostViewAura::GetViewBounds() {
  return window_->GetBoundsInScreen();
}

void RenderWidgetHostViewAura::UpdateBackgroundColor() {
  CHECK(GetBackgroundColor());

  SkColor color = *GetBackgroundColor();
  bool opaque = SkColorGetA(color) == SK_AlphaOPAQUE;
  window_->layer()->SetFillsBoundsOpaquely(opaque);
  window_->layer()->SetColor(color);
}

#if BUILDFLAG(IS_WIN)
void RenderWidgetHostViewAura::ObserveDevicePosturePlatformProvider() {
  if (device_posture_observation_.IsObserving()) {
    return;
  }

  DevicePosturePlatformProvider* platform_provider =
      GetDevicePosturePlatformProvider();
  if (!platform_provider) {
    return;
  }

  device_posture_observation_.Observe(platform_provider);
  OnDisplayFeatureBoundsChanged(platform_provider->GetDisplayFeatureBounds());
}
#endif

void RenderWidgetHostViewAura::OnDisplayFeatureBoundsChanged(
    const gfx::Rect& display_feature_bounds) {
  if (display_feature_overridden_for_testing_) {
    return;
  }

  display_feature_ = std::nullopt;
  display_feature_bounds_ = gfx::Rect();
  if (display_feature_bounds.IsEmpty()) {
    SynchronizeVisualProperties(cc::DeadlinePolicy::UseDefaultDeadline(),
                                window_->GetLocalSurfaceId());
    return;
  }

  display_feature_bounds_ = display_feature_bounds;
  ComputeDisplayFeature();

  SynchronizeVisualProperties(cc::DeadlinePolicy::UseDefaultDeadline(),
                              window_->GetLocalSurfaceId());
}

void RenderWidgetHostViewAura::ComputeDisplayFeature() {
  if (display_feature_bounds_.IsEmpty() ||
      display_feature_overridden_for_testing_) {
    return;
  }

  display_feature_ = std::nullopt;
  if (!window_->GetRootWindow()) {
    return;
  }

  const display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window_);
  // Set the display feature only if the browser window is maximized or
  // fullscreen.
  if (window_->GetRootWindow()->GetBoundsInScreen() != display.work_area() &&
      window_->GetRootWindow()->GetBoundsInScreen() != display.bounds()) {
    return;
  }

  float dip_scale = 1 / device_scale_factor_;
  // Segments coming from the platform are in native resolution.
  gfx::Rect transformed_display_feature =
      gfx::ScaleToRoundedRect(display_feature_bounds_, dip_scale);
  transformed_display_feature.Offset(-GetViewBounds().x(),
                                     -GetViewBounds().y());
  transformed_display_feature.Intersect(gfx::Rect(GetVisibleViewportSize()));
  if (transformed_display_feature.x() == 0) {
    display_feature_ = {DisplayFeature::Orientation::kHorizontal,
                        transformed_display_feature.y(),
                        transformed_display_feature.height()};
  } else if (transformed_display_feature.y() == 0) {
    display_feature_ = {DisplayFeature::Orientation::kVertical,
                        transformed_display_feature.x(),
                        transformed_display_feature.width()};
  }
}

std::optional<DisplayFeature> RenderWidgetHostViewAura::GetDisplayFeature() {
  return display_feature_;
}

void RenderWidgetHostViewAura::SetDisplayFeatureForTesting(
    const DisplayFeature* display_feature) {
  if (display_feature) {
    display_feature_ = *display_feature;
  } else {
    display_feature_ = std::nullopt;
  }
  display_feature_overridden_for_testing_ = true;
}

void RenderWidgetHostViewAura::WindowTitleChanged() {
  CHECK(delegated_frame_host_) << "Cannot be invoked during destruction.";
  delegated_frame_host_->WindowTitleChanged(
      base::UTF16ToUTF8(window_->GetTitle()));
}

bool RenderWidgetHostViewAura::IsPointerLocked() {
  return event_handler_->mouse_locked();
}

gfx::Size RenderWidgetHostViewAura::GetVisibleViewportSize() {
  gfx::Rect requested_rect(GetRequestedRendererSize());
  requested_rect.Inset(insets_);
  return requested_rect.size();
}

void RenderWidgetHostViewAura::SetInsets(const gfx::Insets& insets) {
  TRACE_EVENT0("vk", "RenderWidgetHostViewAura::SetInsets");
  if (insets != insets_) {
    insets_ = insets;
    window_->AllocateLocalSurfaceId();
    if (!insets.IsEmpty()) {
      inset_surface_id_ = window_->GetLocalSurfaceId();
    } else {
      inset_surface_id_ = viz::LocalSurfaceId();
    }
    SynchronizeVisualProperties(cc::DeadlinePolicy::UseDefaultDeadline(),
                                window_->GetLocalSurfaceId());
  }
}

void RenderWidgetHostViewAura::UpdateCursor(const ui::Cursor& cursor) {
  GetCursorManager()->UpdateCursor(this, cursor);
}

void RenderWidgetHostViewAura::DisplayCursor(const ui::Cursor& cursor) {
  current_cursor_ = WebCursor(cursor);
  current_cursor_.UpdateDisplayInfoForWindow(window_);
  UpdateCursorIfOverSelf();
}

input::CursorManager* RenderWidgetHostViewAura::GetCursorManager() {
  return cursor_manager_.get();
}

void RenderWidgetHostViewAura::SetIsLoading(bool is_loading) {
  is_loading_ = is_loading;
  UpdateCursorIfOverSelf();
}

void RenderWidgetHostViewAura::RenderProcessGone() {
  UpdateCursorIfOverSelf();
  Destroy();
}

void RenderWidgetHostViewAura::ShowWithVisibility(
    PageVisibilityState page_visibility) {
  // Make sure we grab updated ScreenInfos before synchronizing visual
  // properties, in case they have changed or this is the initial show.
  UpdateScreenInfo();

  // If the viz::LocalSurfaceId is invalid, we may have been evicted,
  // and no other visual properties have since been changed. Allocate a new id
  // and start synchronizing.
  if (!window_->GetLocalSurfaceId().is_valid()) {
    window_->AllocateLocalSurfaceId();
    SynchronizeVisualProperties(cc::DeadlinePolicy::UseDefaultDeadline(),
                                window_->GetLocalSurfaceId());
  }

  window_->Show();
  ShowImpl(page_visibility);
#if BUILDFLAG(IS_WIN)
  if (page_visibility != PageVisibilityState::kVisible &&
      legacy_render_widget_host_HWND_) {
    legacy_render_widget_host_HWND_->Hide();
  }
#endif  // BUILDFLAG(IS_WIN)
}

void RenderWidgetHostViewAura::Destroy() {
  // Beware, this function is not called on all destruction paths. If |window_|
  // has been created, then it will implicitly end up calling
  // ~RenderWidgetHostViewAura when |window_| is destroyed. Otherwise, The
  // destructor is invoked directly from here. So all destruction/cleanup code
  // should happen there, not here.
  in_shutdown_ = true;
  // Call this here in case any observers need access to `this` before we
  // destruct the derived class.
  NotifyObserversAboutShutdown();

  if (window_)
    delete window_;
  else
    delete this;
}

void RenderWidgetHostViewAura::UpdateTooltipUnderCursor(
    const std::u16string& tooltip_text) {
  if (GetCursorManager()->IsViewUnderCursor(this))
    UpdateTooltip(tooltip_text);
}

void RenderWidgetHostViewAura::UpdateTooltip(
    const std::u16string& tooltip_text) {
  SetTooltipText(tooltip_text);

  wm::TooltipClient* tooltip_client =
      wm::GetTooltipClient(window_->GetRootWindow());
  if (tooltip_client) {
    // Content tooltips should be visible indefinitely.
    tooltip_client->SetHideTooltipTimeout(window_, {});
    tooltip_client->UpdateTooltip(window_);
  }
}

void RenderWidgetHostViewAura::UpdateTooltipFromKeyboard(
    const std::u16string& tooltip_text,
    const gfx::Rect& bounds) {
  SetTooltipText(tooltip_text);

  wm::TooltipClient* tooltip_client =
      wm::GetTooltipClient(window_->GetRootWindow());
  if (tooltip_client) {
    // Content tooltips should be visible indefinitely.
    tooltip_client->SetHideTooltipTimeout(window_, {});
    tooltip_client->UpdateTooltipFromKeyboard(bounds, window_);
  }
}

void RenderWidgetHostViewAura::ClearKeyboardTriggeredTooltip() {
  if (!window_ || !window_->GetHost())
    return;

  wm::TooltipClient* tooltip_client =
      wm::GetTooltipClient(window_->GetRootWindow());
  if (!tooltip_client || !tooltip_client->IsTooltipSetFromKeyboard(window_))
    return;

  SetTooltipText(std::u16string());
  tooltip_client->UpdateTooltipFromKeyboard(gfx::Rect(), window_);
}

uint32_t RenderWidgetHostViewAura::GetCaptureSequenceNumber() const {
  return latest_capture_sequence_number_;
}

void RenderWidgetHostViewAura::CopyFromSurface(
    const gfx::Rect& src_subrect,
    const gfx::Size& dst_size,
    base::OnceCallback<void(const SkBitmap&)> callback) {
  base::WeakPtr<RenderWidgetHostImpl> popup_host;
  base::WeakPtr<DelegatedFrameHost> popup_frame_host;
  if (popup_child_host_view_) {
    popup_host = popup_child_host_view_->host()->GetWeakPtr();
    popup_frame_host =
        popup_child_host_view_->GetDelegatedFrameHost()->GetWeakPtr();
  }
  RenderWidgetHostViewBase::CopyMainAndPopupFromSurface(
      host()->GetWeakPtr(), delegated_frame_host_->GetWeakPtr(), popup_host,
      popup_frame_host, src_subrect, dst_size, device_scale_factor_,
      std::move(callback));
}

#if BUILDFLAG(IS_WIN)
void RenderWidgetHostViewAura::UpdateMouseLockRegion() {
  RECT window_rect =
      display::Screen::GetScreen()
          ->DIPToScreenRectInWindow(window_, window_->GetBoundsInScreen())
          .ToRECT();
  ::ClipCursor(&window_rect);
}

void RenderWidgetHostViewAura::OnLegacyWindowDestroyed() {
  legacy_render_widget_host_HWND_ = nullptr;
  legacy_window_destroyed_ = true;
}
#endif

gfx::NativeViewAccessible
RenderWidgetHostViewAura::GetParentNativeViewAccessible() {
  // If a popup_parent_host_view_ exists, that means we are in a popup (such as
  // datetime) and our accessible parent window is popup_parent_host_view_
  if (popup_parent_host_view_) {
    CHECK_EQ(widget_type_, WidgetType::kPopup);
    return popup_parent_host_view_->GetParentNativeViewAccessible();
  }

  if (window_->parent()) {
    return window_->parent()->GetProperty(
        aura::client::kParentNativeViewAccessibleKey);
  }

  return nullptr;
}

void RenderWidgetHostViewAura::ClearFallbackSurfaceForCommitPending() {
  delegated_frame_host_->ClearFallbackSurfaceForCommitPending();
  window_->InvalidateLocalSurfaceId();
}

void RenderWidgetHostViewAura::ResetFallbackToFirstNavigationSurface() {
  CHECK(delegated_frame_host_) << "Cannot be invoked during destruction.";
  delegated_frame_host_->ResetFallbackToFirstNavigationSurface();
}

bool RenderWidgetHostViewAura::RequestRepaintForTesting() {
  return SynchronizeVisualProperties(cc::DeadlinePolicy::UseDefaultDeadline(),
                                     std::nullopt);
}

void RenderWidgetHostViewAura::DidStopFlinging() {
  selection_controller_client_->OnScrollCompleted();
}

void RenderWidgetHostViewAura::TransformPointToRootSurface(gfx::PointF* point) {
  aura::Window* root = window_->GetRootWindow();
  aura::Window::ConvertPointToTarget(window_, root, point);
  *point = root->GetRootWindow()->transform().MapPoint(*point);
}

gfx::Rect RenderWidgetHostViewAura::GetBoundsInRootWindow() {
  aura::Window* top_level = window_->GetToplevelWindow();
  gfx::Rect bounds(top_level->GetBoundsInScreen());

#if BUILDFLAG(IS_WIN)
  // TODO(zturner,iyengar): This will break when we remove the legacy hwnd, so a
  // better fix will need to be decided when that happens.
  if (legacy_render_widget_host_HWND_) {
    // aura::Window doesn't take into account non-client area of native windows
    // (e.g. HWNDs), so for that case ask Windows directly what the bounds are.
    aura::WindowTreeHost* host = top_level->GetHost();
    if (!host)
      return top_level->GetBoundsInScreen();

    // If this is a headless window return the headless window bounds stored in
    // Aura window properties instead of the actual platform window bounds which
    // may be different.
    if (gfx::Rect* headless_bounds =
            host->window()->GetProperty(aura::client::kHeadlessBoundsKey)) {
      return *headless_bounds;
    }

    RECT window_rect = {0};
    HWND hwnd = host->GetAcceleratedWidget();
    ::GetWindowRect(hwnd, &window_rect);
    bounds = gfx::Rect(window_rect);

    // Maximized windows are outdented from the work area by the frame thickness
    // even though this "frame" is not painted.  This confuses code (and people)
    // that think of a maximized window as corresponding exactly to the work
    // area.  Correct for this by subtracting the frame thickness back off.
    if (::IsZoomed(hwnd)) {
      bounds.Inset(gfx::Insets::VH(GetSystemMetrics(SM_CYSIZEFRAME),
                                   GetSystemMetrics(SM_CXSIZEFRAME)));
      bounds.Inset(GetSystemMetrics(SM_CXPADDEDBORDER));
    }

    // Pixels come back from GetWindowHost, so we need to convert those back to
    // DIPs here.
    bounds = display::Screen::GetScreen()->ScreenToDIPRectInWindow(top_level,
                                                                   bounds);
  }

#endif

  return bounds;
}

void RenderWidgetHostViewAura::WheelEventAck(
    const blink::WebMouseWheelEvent& event,
    blink::mojom::InputEventResultState ack_result) {
  if (overscroll_controller_) {
    overscroll_controller_->ReceivedEventACK(
        event, (blink::mojom::InputEventResultState::kConsumed == ack_result));
  }
}

void RenderWidgetHostViewAura::DidOverscroll(
    const ui::DidOverscrollParams& params) {
  if (overscroll_controller_)
    overscroll_controller_->OnDidOverscroll(params);
}

void RenderWidgetHostViewAura::GestureEventAck(
    const blink::WebGestureEvent& event,
    blink::mojom::InputEventResultSource ack_source,
    blink::mojom::InputEventResultState ack_result) {
  TRACE_EVENT1("input", "RenderWidgetHostViewAura::GestureEventAck", "type",
               blink::WebInputEvent::GetName(event.GetType()));
  const blink::WebInputEvent::Type event_type = event.GetType();
  if (event_type == blink::WebGestureEvent::Type::kGestureScrollBegin ||
      event_type == blink::WebGestureEvent::Type::kGestureScrollEnd) {
    if (host()->delegate()) {
      host()->delegate()->SetTopControlsGestureScrollInProgress(
          event_type == blink::WebGestureEvent::Type::kGestureScrollBegin);
    }
  }

  if (overscroll_controller_) {
    overscroll_controller_->ReceivedEventACK(
        event, (blink::mojom::InputEventResultState::kConsumed == ack_result));
    // Terminate an active fling when the ACK for a GSU generated from the fling
    // progress (GSU with inertial state) is consumed and the overscrolling mode
    // is not |OVERSCROLL_NONE|. The early fling termination generates a GSE
    // which completes the overscroll action. Without this change the overscroll
    // action would complete at the end of the active fling progress which
    // causes noticeable delay in cases that the fling velocity is large.
    // https://crbug.com/797855
    if (event_type == blink::WebInputEvent::Type::kGestureScrollUpdate &&
        event.data.scroll_update.inertial_phase ==
            blink::WebGestureEvent::InertialPhaseState::kMomentum &&
        overscroll_controller_->overscroll_mode() != OVERSCROLL_NONE) {
      StopFling();
    }
  }

  // Stop flinging if a GSU event with momentum phase is sent to the renderer
  // but not consumed.
  StopFlingingIfNecessary(event, ack_result);

  event_handler_->GestureEventAck(event, ack_result);

  ForwardTouchpadZoomEventIfNecessary(event, ack_result);
}

void RenderWidgetHostViewAura::ProcessAckedTouchEvent(
    const input::TouchEventWithLatencyInfo& touch,
    blink::mojom::InputEventResultState ack_result) {
  aura::WindowTreeHost* window_host = window_->GetHost();
  // |host| is NULL during tests.
  if (!window_host)
    return;

  // The TouchScrollStarted event is generated & consumed downstream from the
  // TouchEventQueue. So we don't expect an ACK up here.
  CHECK(touch.event.GetType() !=
        blink::WebInputEvent::Type::kTouchScrollStarted);

  ui::EventResult result =
      (ack_result == blink::mojom::InputEventResultState::kConsumed)
          ? ui::ER_HANDLED
          : ui::ER_UNHANDLED;

  blink::WebTouchPoint::State required_state;
  switch (touch.event.GetType()) {
    case blink::WebInputEvent::Type::kTouchStart:
      required_state = blink::WebTouchPoint::State::kStatePressed;
      break;
    case blink::WebInputEvent::Type::kTouchEnd:
      required_state = blink::WebTouchPoint::State::kStateReleased;
      break;
    case blink::WebInputEvent::Type::kTouchMove:
      required_state = blink::WebTouchPoint::State::kStateMoved;
      break;
    case blink::WebInputEvent::Type::kTouchCancel:
      required_state = blink::WebTouchPoint::State::kStateCancelled;
      break;
    default:
      required_state = blink::WebTouchPoint::State::kStateUndefined;
      NOTREACHED_IN_MIGRATION();
      break;
  }

  // Only send acks for one changed touch point.
  bool sent_ack = false;
  for (size_t i = 0; i < touch.event.touches_length; ++i) {
    if (touch.event.touches[i].state == required_state) {
      CHECK(!sent_ack);
      window_host->dispatcher()->ProcessedTouchEvent(
          touch.event.unique_touch_event_id, window_, result,
          InputEventResultStateIsSetBlocking(ack_result));
      if (touch.event.touch_start_or_first_touch_move &&
          result == ui::ER_HANDLED && host()->delegate() &&
          host()->delegate()->GetInputEventRouter()) {
        host()
            ->delegate()
            ->GetInputEventRouter()
            ->OnHandledTouchStartOrFirstTouchMove(
                touch.event.unique_touch_event_id);
      }
      sent_ack = true;
    }
  }
}

std::unique_ptr<SyntheticGestureTarget>
RenderWidgetHostViewAura::CreateSyntheticGestureTarget() {
  return std::unique_ptr<SyntheticGestureTarget>(
      new SyntheticGestureTargetAura(host()));
}

blink::mojom::InputEventResultState RenderWidgetHostViewAura::FilterInputEvent(
    const blink::WebInputEvent& input_event) {
  bool consumed = false;
  if (input_event.GetType() == WebInputEvent::Type::kGestureFlingStart) {
    const WebGestureEvent& gesture_event =
        static_cast<const WebGestureEvent&>(input_event);
    // Zero-velocity touchpad flings are an Aura-specific signal that the
    // touchpad scroll has ended, and should not be forwarded to the renderer.
    if (gesture_event.SourceDevice() == blink::WebGestureDevice::kTouchpad &&
        !gesture_event.data.fling_start.velocity_x &&
        !gesture_event.data.fling_start.velocity_y) {
      consumed = true;
    }
  }

  if (overscroll_controller_)
    consumed |= overscroll_controller_->WillHandleEvent(input_event);

  // Touch events should always propagate to the renderer.
  if (WebTouchEvent::IsTouchEventType(input_event.GetType()))
    return blink::mojom::InputEventResultState::kNotConsumed;

  if (consumed &&
      input_event.GetType() == blink::WebInputEvent::Type::kGestureFlingStart) {
    // Here we indicate that there was no consumer for this event, as
    // otherwise the fling animation system will try to run an animation
    // and will also expect a notification when the fling ends. Since
    // CrOS just uses the GestureFlingStart with zero-velocity as a means
    // of indicating that touchpad scroll has ended, we don't actually want
    // a fling animation. Note: Similar code exists in
    // RenderWidgetHostViewChildFrame::FilterInputEvent()
    return blink::mojom::InputEventResultState::kNoConsumerExists;
  }

  return consumed ? blink::mojom::InputEventResultState::kConsumed
                  : blink::mojom::InputEventResultState::kNotConsumed;
}

gfx::AcceleratedWidget
RenderWidgetHostViewAura::AccessibilityGetAcceleratedWidget() {
#if BUILDFLAG(IS_WIN)
  if (legacy_render_widget_host_HWND_)
    return legacy_render_widget_host_HWND_->hwnd();
#endif
  return gfx::kNullAcceleratedWidget;
}

gfx::NativeViewAccessible
RenderWidgetHostViewAura::AccessibilityGetNativeViewAccessible() {
#if BUILDFLAG(IS_WIN)
  if (legacy_render_widget_host_HWND_) {
    return legacy_render_widget_host_HWND_->window_accessible();
  }
#endif

  if (window_->parent()) {
    return window_->parent()->GetProperty(
        aura::client::kParentNativeViewAccessibleKey);
  }

  return nullptr;
}

void RenderWidgetHostViewAura::SetMainFrameAXTreeID(ui::AXTreeID id) {
  window_->SetProperty(ui::kChildAXTreeID, id.ToString());
}

blink::mojom::PointerLockResult RenderWidgetHostViewAura::LockPointer(
    bool request_unadjusted_movement) {
  return event_handler_->LockPointer(request_unadjusted_movement);
}

blink::mojom::PointerLockResult RenderWidgetHostViewAura::ChangePointerLock(
    bool request_unadjusted_movement) {
  return event_handler_->ChangePointerLock(request_unadjusted_movement);
}

void RenderWidgetHostViewAura::UnlockPointer() {
  event_handler_->UnlockPointer();
}

bool RenderWidgetHostViewAura::
    GetIsPointerLockedUnadjustedMovementForTesting() {
  return event_handler_->mouse_locked_unadjusted_movement();
}

bool RenderWidgetHostViewAura::LockKeyboard(
    std::optional<base::flat_set<ui::DomCode>> codes) {
  return event_handler_->LockKeyboard(std::move(codes));
}

void RenderWidgetHostViewAura::UnlockKeyboard() {
  event_handler_->UnlockKeyboard();
}

bool RenderWidgetHostViewAura::IsKeyboardLocked() {
  return event_handler_->IsKeyboardLocked();
}

base::flat_map<std::string, std::string>
RenderWidgetHostViewAura::GetKeyboardLayoutMap() {
  aura::WindowTreeHost* host = window_->GetHost();
  if (host)
    return host->GetKeyboardLayoutMap();
  return {};
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, ui::TextInputClient implementation:
base::WeakPtr<ui::TextInputClient> RenderWidgetHostViewAura::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void RenderWidgetHostViewAura::SetCompositionText(
    const ui::CompositionText& composition) {
  if (!text_input_manager_ || !text_input_manager_->GetActiveWidget())
    return;

  // Historically we haven't supported selection range with composition string
  // due to prior bugs. Introducing support for it can reveal issues as seen
  // with Korean IME in Windows for instance. See crbug.com/363266897
  text_input_manager_->GetActiveWidget()->ImeSetComposition(
      composition.text, composition.ime_text_spans, gfx::Range::InvalidRange(),
      composition.selection.end(), composition.selection.end());

  has_composition_text_ = !composition.text.empty();
}

size_t RenderWidgetHostViewAura::ConfirmCompositionText(bool keep_selection) {
  if (text_input_manager_ && text_input_manager_->GetActiveWidget() &&
      has_composition_text_) {
    text_input_manager_->GetActiveWidget()->ImeFinishComposingText(
        keep_selection);
  }
  has_composition_text_ = false;
  // TODO(crbug.com/40141817): Return the number of characters committed by this
  // function.
  return std::numeric_limits<size_t>::max();
}

void RenderWidgetHostViewAura::ClearCompositionText() {
  if (text_input_manager_ && text_input_manager_->GetActiveWidget() &&
      has_composition_text_)
    text_input_manager_->GetActiveWidget()->ImeCancelComposition();
  has_composition_text_ = false;
}

void RenderWidgetHostViewAura::InsertText(
    const std::u16string& text,
    InsertTextCursorBehavior cursor_behavior) {
  CHECK_NE(GetTextInputType(), ui::TEXT_INPUT_TYPE_NONE);

  if (text_input_manager_ && text_input_manager_->GetActiveWidget()) {
    const int relative_cursor_position =
        cursor_behavior == InsertTextCursorBehavior::kMoveCursorBeforeText
            ? -text.length()
            : 0;
    text_input_manager_->GetActiveWidget()->ImeCommitText(
        text, std::vector<ui::ImeTextSpan>(), gfx::Range::InvalidRange(),
        relative_cursor_position);
  }
  has_composition_text_ = false;
}

void RenderWidgetHostViewAura::InsertChar(const ui::KeyEvent& event) {
  if (popup_child_host_view_ && popup_child_host_view_->NeedsInputGrab()) {
    popup_child_host_view_->InsertChar(event);
    return;
  }

  // Ignore character messages for VKEY_RETURN sent on CTRL+M. crbug.com/315547
  if (event_handler_->accept_return_character() ||
      event.GetCharacter() != ui::VKEY_RETURN) {
    // Send a blink::WebInputEvent::Char event to |host_|.
    ForwardKeyboardEventWithLatencyInfo(
        input::NativeWebKeyboardEvent(event, event.GetCharacter()),
        *event.latency(), nullptr);
  }
}

bool RenderWidgetHostViewAura::CanInsertImage() {
  RenderFrameHostImpl* render_frame_host = GetFocusedFrame();

  if (!render_frame_host) {
    return false;
  }

  return render_frame_host->has_focused_richly_editable_element();
}

void RenderWidgetHostViewAura::InsertImage(const GURL& src) {
  auto* input_handler = GetFrameWidgetInputHandlerForFocusedWidget();

  if (!input_handler) {
    return;
  }

  input_handler->ExecuteEditCommand("PasteFromImageURL",
                                    base::UTF8ToUTF16(src.spec()));
}

ui::TextInputType RenderWidgetHostViewAura::GetTextInputType() const {
  if (text_input_manager_ && text_input_manager_->GetTextInputState())
    return text_input_manager_->GetTextInputState()->type;
  return ui::TEXT_INPUT_TYPE_NONE;
}

ui::TextInputMode RenderWidgetHostViewAura::GetTextInputMode() const {
  if (text_input_manager_ && text_input_manager_->GetTextInputState())
    return text_input_manager_->GetTextInputState()->mode;
  return ui::TEXT_INPUT_MODE_DEFAULT;
}

base::i18n::TextDirection RenderWidgetHostViewAura::GetTextDirection() const {
  NOTIMPLEMENTED_LOG_ONCE();
  return base::i18n::UNKNOWN_DIRECTION;
}

int RenderWidgetHostViewAura::GetTextInputFlags() const {
  if (text_input_manager_ && text_input_manager_->GetTextInputState())
    return text_input_manager_->GetTextInputState()->flags;
  return 0;
}

bool RenderWidgetHostViewAura::CanComposeInline() const {
  if (text_input_manager_ && text_input_manager_->GetTextInputState())
    return text_input_manager_->GetTextInputState()->can_compose_inline;
  return true;
}

gfx::Rect RenderWidgetHostViewAura::ConvertRectToScreen(
    const gfx::Rect& rect) const {
  gfx::Point origin = rect.origin();
  gfx::Point end = gfx::Point(rect.right(), rect.bottom());

  aura::Window* root_window = window_->GetRootWindow();
  if (!root_window)
    return rect;
  aura::client::ScreenPositionClient* screen_position_client =
      aura::client::GetScreenPositionClient(root_window);
  if (!screen_position_client)
    return rect;
  screen_position_client->ConvertPointToScreen(window_, &origin);
  screen_position_client->ConvertPointToScreen(window_, &end);
  return gfx::Rect(origin.x(), origin.y(), base::ClampSub(end.x(), origin.x()),
                   base::ClampSub(end.y(), origin.y()));
}

gfx::Rect RenderWidgetHostViewAura::ConvertRectFromScreen(
    const gfx::Rect& rect) const {
  gfx::Rect result = rect;
  if (window_->GetRootWindow() &&
      aura::client::GetScreenPositionClient(window_->GetRootWindow()))
    wm::ConvertRectFromScreen(window_, &result);
  return result;
}

gfx::Rect RenderWidgetHostViewAura::GetCaretBounds() const {
  if (!text_input_manager_ || !text_input_manager_->GetActiveWidget())
    return gfx::Rect();

  // Check selection bound first (currently populated only for EditContext)
  const std::optional<gfx::Rect> text_selection_bound =
      text_input_manager_->GetTextSelectionBounds();
  if (text_selection_bound)
    return ConvertRectToScreen(text_selection_bound.value());

  // If no selection bound, we fall back to use selection region.
  const TextInputManager::SelectionRegion* region =
      text_input_manager_->GetSelectionRegion();
  gfx::Rect caret_rect = ConvertRectToScreen(
      gfx::RectBetweenSelectionBounds(region->anchor, region->focus));
  TRACE_EVENT1("ime", "RenderWidgetHostViewAura::GetCaretBounds", "caret_rect",
               caret_rect.ToString());
  return caret_rect;
}

gfx::Rect RenderWidgetHostViewAura::GetSelectionBoundingBox() const {
  auto* focused_view = GetFocusedViewForTextSelection();
  if (!focused_view)
    return gfx::Rect();

  const gfx::Rect bounding_box =
      text_input_manager_->GetSelectionRegion(focused_view)->bounding_box;
  if (bounding_box.IsEmpty())
    return gfx::Rect();

  return ConvertRectToScreen(bounding_box);
}

bool RenderWidgetHostViewAura::GetCompositionCharacterBounds(
    size_t index,
    gfx::Rect* rect) const {
  CHECK(rect);

  if (!text_input_manager_ || !text_input_manager_->GetActiveWidget())
    return false;

  const TextInputManager::CompositionRangeInfo* composition_range_info =
      text_input_manager_->GetCompositionRangeInfo();

  if (index >= composition_range_info->character_bounds.size())
    return false;
  *rect = ConvertRectToScreen(composition_range_info->character_bounds[index]);
  TRACE_EVENT1("ime", "RenderWidgetHostViewAura::GetCompositionCharacterBounds",
               "comp_char_rect", rect->ToString());
  return true;
}

bool RenderWidgetHostViewAura::HasCompositionText() const {
  return has_composition_text_;
}

ui::TextInputClient::FocusReason RenderWidgetHostViewAura::GetFocusReason()
    const {
  if (!window_->HasFocus())
    return ui::TextInputClient::FOCUS_REASON_NONE;

  switch (last_pointer_type_before_focus_) {
    case ui::EventPointerType::kMouse:
      return ui::TextInputClient::FOCUS_REASON_MOUSE;
    case ui::EventPointerType::kPen:
      return ui::TextInputClient::FOCUS_REASON_PEN;
    case ui::EventPointerType::kTouch:
      return ui::TextInputClient::FOCUS_REASON_TOUCH;
    default:
      return ui::TextInputClient::FOCUS_REASON_OTHER;
  }
}

bool RenderWidgetHostViewAura::GetTextRange(gfx::Range* range) const {
  if (!text_input_manager_ || !GetFocusedWidget())
    return false;

  const ui::mojom::TextInputState* state =
      text_input_manager_->GetTextInputState();
  if (!state)
    return false;

  range->set_start(0);
  range->set_end(state->value ? state->value->length() : 0);
  return true;
}

bool RenderWidgetHostViewAura::GetCompositionTextRange(
    gfx::Range* range) const {
  if (!text_input_manager_ || !GetFocusedWidget())
    return false;

  const ui::mojom::TextInputState* state =
      text_input_manager_->GetTextInputState();
  // Return false when there is no composition.
  if (!state || !state->composition)
    return false;

  *range = state->composition.value();
  return true;
}

bool RenderWidgetHostViewAura::GetEditableSelectionRange(
    gfx::Range* range) const {
  if (!text_input_manager_ || !GetFocusedWidget())
    return false;

  const ui::mojom::TextInputState* state =
      text_input_manager_->GetTextInputState();
  if (!state)
    return false;

  *range = state->selection;
  return true;
}

bool RenderWidgetHostViewAura::SetEditableSelectionRange(
    const gfx::Range& range) {
  // TODO(crbug.com/41432062): Write an unit test for this method.
  auto* input_handler = GetFrameWidgetInputHandlerForFocusedWidget();
  if (!input_handler)
    return false;
  input_handler->SetEditableSelectionOffsets(range.start(), range.end());
  return true;
}

bool RenderWidgetHostViewAura::GetTextFromRange(const gfx::Range& range,
                                                std::u16string* text) const {
  if (!text_input_manager_ || !GetFocusedWidget())
    return false;

  const ui::mojom::TextInputState* state =
      text_input_manager_->GetTextInputState();
  if (!state)
    return false;

  gfx::Range text_range;
  GetTextRange(&text_range);

  if (!text_range.Contains(range)) {
    text->clear();
    return false;
  }
  if (!state->value) {
    text->clear();
    return true;
  }
  if (text_range.EqualsIgnoringDirection(range)) {
    // Avoid calling substr whose performance is low.
    *text = *state->value;
  } else {
    *text = state->value->substr(range.GetMin(), range.length());
  }
  return true;
}

void RenderWidgetHostViewAura::OnInputMethodChanged() {
  // TODO(suzhe): implement the newly added "locale" property of HTML DOM
  // TextEvent.
}

bool RenderWidgetHostViewAura::ChangeTextDirectionAndLayoutAlignment(
      base::i18n::TextDirection direction) {
  if (!GetTextInputManager() && !GetTextInputManager()->GetActiveWidget())
    return false;

  GetTextInputManager()->GetActiveWidget()->UpdateTextDirection(direction);
  GetTextInputManager()->GetActiveWidget()->NotifyTextDirection();
  return true;
}

void RenderWidgetHostViewAura::ExtendSelectionAndDelete(
    size_t before, size_t after) {
  auto* input_handler = GetFrameWidgetInputHandlerForFocusedWidget();
  if (!input_handler)
    return;
  input_handler->ExtendSelectionAndDelete(before, after);
}

#if BUILDFLAG(IS_CHROMEOS)
void RenderWidgetHostViewAura::ExtendSelectionAndReplace(
    size_t before,
    size_t after,
    std::u16string_view replacement_text) {
  auto* input_handler = GetFrameWidgetInputHandlerForFocusedWidget();
  if (!input_handler) {
    return;
  }
  input_handler->ExtendSelectionAndReplace(before, after,
                                           std::u16string(replacement_text));
}
#endif

void RenderWidgetHostViewAura::EnsureCaretNotInRect(
    const gfx::Rect& rect_in_screen) {
  keyboard_occluded_bounds_ = rect_in_screen;

  // If keyboard is disabled, reset the insets_.
  if (keyboard_occluded_bounds_.IsEmpty()) {
    SetInsets(gfx::Insets());
  } else {
    UpdateInsetsWithVirtualKeyboardEnabled();
  }

  aura::Window* top_level_window = window_->GetToplevelWindow();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  wm::EnsureWindowNotInRect(top_level_window, keyboard_occluded_bounds_);
#endif

  // Perform overscroll if the caret is still hidden by the keyboard.
  const gfx::Rect hidden_window_bounds_in_screen = gfx::IntersectRects(
      keyboard_occluded_bounds_, top_level_window->GetBoundsInScreen());

  if (hidden_window_bounds_in_screen.IsEmpty())
    return;

  ScrollFocusedEditableNodeIntoView();
}

bool RenderWidgetHostViewAura::IsTextEditCommandEnabled(
    ui::TextEditCommand command) const {
  return false;
}

void RenderWidgetHostViewAura::SetTextEditCommandForNextKeyEvent(
    ui::TextEditCommand command) {}

ukm::SourceId RenderWidgetHostViewAura::GetClientSourceForMetrics() const {
  RenderFrameHostImpl* frame = GetFocusedFrame();
  // ukm::SourceId is not available while prerendering.
  if (frame && !frame->IsInLifecycleState(
                   RenderFrameHost::LifecycleState::kPrerendering)) {
    return frame->GetPageUkmSourceId();
  }
  return ukm::SourceId();
}

bool RenderWidgetHostViewAura::ShouldDoLearning() {
  return host() && host()->delegate() && host()->delegate()->ShouldDoLearning();
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
bool RenderWidgetHostViewAura::SetCompositionFromExistingText(
    const gfx::Range& range,
    const std::vector<ui::ImeTextSpan>& ui_ime_text_spans) {
  auto* input_handler = GetFrameWidgetInputHandlerForFocusedWidget();
  if (!input_handler)
    return false;
  input_handler->SetCompositionFromExistingText(range.start(), range.end(),
                                                ui_ime_text_spans);
  has_composition_text_ = true;
  return true;
}

#endif

#if BUILDFLAG(IS_CHROMEOS)
gfx::Range RenderWidgetHostViewAura::GetAutocorrectRange() const {
  if (!text_input_manager_ || !text_input_manager_->GetActiveWidget())
    return gfx::Range();
  return text_input_manager_->GetAutocorrectRange();
}

gfx::Rect RenderWidgetHostViewAura::GetAutocorrectCharacterBounds() const {
  if (!text_input_manager_ || !text_input_manager_->GetActiveWidget())
    return gfx::Rect();

  const std::vector<ui::mojom::ImeTextSpanInfoPtr>& ime_text_spans_info =
      text_input_manager_->GetTextInputState()->ime_text_spans_info;

  // If there are multiple autocorrect spans, use the first one.
  for (const auto& ime_text_span_info : ime_text_spans_info) {
    if (ime_text_span_info->span.type == ui::ImeTextSpan::Type::kAutocorrect) {
      return ConvertRectToScreen(ime_text_span_info->bounds);
    }
  }
  return {};
}

bool RenderWidgetHostViewAura::SetAutocorrectRange(
    const gfx::Range& range) {
  if (!range.is_empty()) {
    base::UmaHistogramEnumeration(
        "InputMethod.Assistive.Autocorrect.Count",
        TextInputClient::SubClass::kRenderWidgetHostViewAura);
  }

  auto* input_handler = GetFrameWidgetInputHandlerForFocusedWidget();
  if (!input_handler)
    return false;

  input_handler->ClearImeTextSpansByType(0,
                                         std::numeric_limits<uint32_t>::max(),
                                         ui::ImeTextSpan::Type::kAutocorrect);

  if (range.is_empty())
    return true;

  ui::ImeTextSpan ui_ime_text_span;
  ui_ime_text_span.type = ui::ImeTextSpan::Type::kAutocorrect;
  ui_ime_text_span.start_offset = 0;
  ui_ime_text_span.end_offset = range.length();
  ui_ime_text_span.underline_style = ui::ImeTextSpan::UnderlineStyle::kDot;
  ui_ime_text_span.underline_color =
      SkColorSetA(gfx::kGoogleGrey700, SK_AlphaOPAQUE * 0.7);
  ui_ime_text_span.thickness = ui::ImeTextSpan::Thickness::kThick;

  input_handler->AddImeTextSpansToExistingText(range.start(), range.end(),
                                               {ui_ime_text_span});
  return true;
}

std::optional<ui::GrammarFragment>
RenderWidgetHostViewAura::GetGrammarFragmentAtCursor() const {
  if (!text_input_manager_ || !text_input_manager_->GetActiveWidget())
    return std::nullopt;
  gfx::Range selection_range;
  if (GetEditableSelectionRange(&selection_range)) {
    return text_input_manager_->GetGrammarFragment(selection_range);
  } else {
    return std::nullopt;
  }
}

bool RenderWidgetHostViewAura::ClearGrammarFragments(const gfx::Range& range) {
  auto* input_handler = GetFrameWidgetInputHandlerForFocusedWidget();
  if (!input_handler)
    return false;

  input_handler->ClearImeTextSpansByType(
      range.start(), range.end(), ui::ImeTextSpan::Type::kGrammarSuggestion);
  return true;
}

bool RenderWidgetHostViewAura::AddGrammarFragments(
    const std::vector<ui::GrammarFragment>& fragments) {
  if (!fragments.empty()) {
    base::UmaHistogramEnumeration(
        "InputMethod.Assistive.Grammar.Count",
        TextInputClient::SubClass::kRenderWidgetHostViewAura);
  }

  auto* input_handler = GetFrameWidgetInputHandlerForFocusedWidget();
  if (!input_handler || fragments.empty())
    return false;

  unsigned max_fragment_end = 0;
  std::vector<::ui::ImeTextSpan> ime_text_spans;
  ime_text_spans.reserve(fragments.size());
  for (auto& fragment : fragments) {
    ui::ImeTextSpan ui_ime_text_span;
    ui_ime_text_span.type = ui::ImeTextSpan::Type::kGrammarSuggestion;
    ui_ime_text_span.start_offset = fragment.range.start();
    ui_ime_text_span.end_offset = fragment.range.end();
    ui_ime_text_span.thickness = ui::ImeTextSpan::Thickness::kThick;
    ui_ime_text_span.underline_style = ui::ImeTextSpan::UnderlineStyle::kDot;
    ui_ime_text_span.underline_color = gfx::kGoogleBlue400;
    ui_ime_text_span.suggestions = {fragment.suggestion};

    ime_text_spans.push_back(ui_ime_text_span);
    if (fragment.range.end() > max_fragment_end) {
      max_fragment_end = fragment.range.end();
    }
  }
  input_handler->AddImeTextSpansToExistingText(0, max_fragment_end,
                                               ime_text_spans);

  return true;
}

#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
void RenderWidgetHostViewAura::GetActiveTextInputControlLayoutBounds(
    std::optional<gfx::Rect>* control_bounds,
    std::optional<gfx::Rect>* selection_bounds) {
  if (text_input_manager_) {
    const std::optional<gfx::Rect> text_control_bounds =
        text_input_manager_->GetTextControlBounds();
    if (text_control_bounds) {
      *control_bounds = ConvertRectToScreen(text_control_bounds.value());
      TRACE_EVENT1(
          "ime",
          "RenderWidgetHostViewAura::GetActiveTextInputControlLayoutBounds",
          "control_bounds_rect", control_bounds->value().ToString());
    }
    // Selection bounds are currently populated only for EditContext.
    // For editable elements we use GetCompositionCharacterBounds.
    const std::optional<gfx::Rect> text_selection_bounds =
        text_input_manager_->GetTextSelectionBounds();
    if (text_selection_bounds) {
      *selection_bounds = ConvertRectToScreen(text_selection_bounds.value());
    }
  }
}
#endif

#if BUILDFLAG(IS_WIN)
void RenderWidgetHostViewAura::SetActiveCompositionForAccessibility(
    const gfx::Range& range,
    const std::u16string& active_composition_text,
    bool is_composition_committed) {
  ui::BrowserAccessibilityManager* manager =
      host()->GetRootBrowserAccessibilityManager();
  if (manager) {
    ui::AXPlatformNodeWin* focus_node = static_cast<ui::AXPlatformNodeWin*>(
        ui::AXPlatformNode::FromNativeViewAccessible(
            manager->GetFocus()->GetNativeViewAccessible()));
    if (focus_node) {
      // Notify accessibility object about this composition
      focus_node->OnActiveComposition(range, active_composition_text,
                                      is_composition_committed);
    }
  }
}
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH)
ui::TextInputClient::EditingContext
RenderWidgetHostViewAura::GetTextEditingContext() {
  ui::TextInputClient::EditingContext editing_context;
  // We use the focused frame's URL here and not the main frame because
  // TSF(Windows Text Service Framework) works on the active editable element
  // context and it uses this information to assist the UIA(Microsoft UI
  // Automation) service to determine the character that is being typed by the
  // user via IME composition, the URL of the site that the user is typing on
  // and other text related services that are used by the UIA clients to power
  // accessibility features on Windows. We want to expose the focused frame's
  // URL to TSF that notifies the UIA service which uses this info and the
  // focused element's data to provide better screen reading capabilities.
  RenderFrameHostImpl* frame = GetFocusedFrame();
  if (frame)
    editing_context.page_url = frame->GetLastCommittedURL();
  return editing_context;
}
#endif

#if BUILDFLAG(IS_WIN)
void RenderWidgetHostViewAura::NotifyOnFrameFocusChanged() {
  if (GetInputMethod()) {
    GetInputMethod()->OnUrlChanged();
  }
}
#endif

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, display::DisplayObserver implementation:

void RenderWidgetHostViewAura::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t metrics) {
#if BUILDFLAG(IS_OZONE)
  // TODO(crbug.com/348590032) If per-window scaling is enabled, the display
  // scale comparison below is not applicable as the WindowTreeHost uses the
  // accurate per-window scale value and the display uses a scale factor value
  // that is inferred from the logical size which is prone to rounding errors,
  // in which case this will never match and end up suppressing visual
  // properties synchronization after the display configuration changes. So
  // process display metrics change as usual skipping such checks. Also see
  // RenderWidgetHostViewBase::UpdateScreenInfo().
  if (ui::OzonePlatform::GetInstance()
          ->GetPlatformRuntimeProperties()
          .supports_per_window_scaling) {
    ProcessDisplayMetricsChanged();
    return;
  }
#endif  // BUILDFLAG(IS_OZONE)
  display::Screen* screen = display::Screen::GetScreen();
  if (display.id() != screen->GetDisplayNearestWindow(window_).id())
    return;

  if (window_->GetHost() && window_->GetHost()->device_scale_factor() !=
                                display.device_scale_factor()) {
    // The DisplayMetrics changed, but the Compositor hasn't been updated yet.
    // Delay updating until the Compositor is updated as well, otherwise we
    // are likely to hit surface invariants (LocalSurfaceId generated with a
    // size/scale-factor that differs from scale-factor used by Compositor).
    needs_to_update_display_metrics_ = true;
    return;
  }
  ProcessDisplayMetricsChanged();
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, aura::WindowDelegate implementation:

gfx::Size RenderWidgetHostViewAura::GetMinimumSize() const {
  return gfx::Size();
}

gfx::Size RenderWidgetHostViewAura::GetMaximumSize() const {
  return gfx::Size();
}

void RenderWidgetHostViewAura::OnBoundsChanged(const gfx::Rect& old_bounds,
                                               const gfx::Rect& new_bounds) {
  base::AutoReset<bool> in_bounds_changed(&in_bounds_changed_, true);
  // We care about this whenever RenderWidgetHostViewAura is not owned by a
  // WebContentsViewAura since changes to the Window's bounds need to be
  // messaged to the renderer.  WebContentsViewAura invokes SetSize() or
  // SetBounds() itself.  No matter how we got here, any redundant calls are
  // harmless.
  SetSize(new_bounds.size());

  if (GetInputMethod()) {
    GetInputMethod()->OnCaretBoundsChanged(this);
    UpdateInsetsWithVirtualKeyboardEnabled();
  }
}

gfx::NativeCursor RenderWidgetHostViewAura::GetCursor(const gfx::Point& point) {
  if (IsPointerLocked()) {
    return ui::mojom::CursorType::kNone;
  }
  return current_cursor_.GetNativeCursor();
}

int RenderWidgetHostViewAura::GetNonClientComponent(
    const gfx::Point& point) const {
  return HTCLIENT;
}

bool RenderWidgetHostViewAura::ShouldDescendIntoChildForEventHandling(
    aura::Window* child,
    const gfx::Point& location) {
  return true;
}

bool RenderWidgetHostViewAura::CanFocus() {
  return widget_type_ == WidgetType::kFrame;
}

void RenderWidgetHostViewAura::OnCaptureLost() {
  host()->LostCapture();
}

void RenderWidgetHostViewAura::OnPaint(const ui::PaintContext& context) {
  NOTREACHED_IN_MIGRATION();
}

void RenderWidgetHostViewAura::OnDeviceScaleFactorChanged(
    float old_device_scale_factor,
    float new_device_scale_factor) {
  if (!window_->GetRootWindow())
    return;

  // TODO(crbug.com/40268472): Add unittest for lacros.
  if (needs_to_update_display_metrics_ ||
      old_device_scale_factor != new_device_scale_factor) {
    ProcessDisplayMetricsChanged();
  }

  SynchronizeVisualProperties(cc::DeadlinePolicy::UseDefaultDeadline(),
                              window_->GetLocalSurfaceId());

  device_scale_factor_ = new_device_scale_factor;
}

void RenderWidgetHostViewAura::OnWindowDestroying(aura::Window* window) {
#if BUILDFLAG(IS_WIN)
  // The LegacyRenderWidgetHostHWND instance is destroyed when its window is
  // destroyed. Normally we control when that happens via the Destroy call
  // in the dtor. However there may be cases where the window is destroyed
  // by Windows, i.e. the parent window is destroyed before the
  // RenderWidgetHostViewAura instance goes away etc. To avoid that we
  // destroy the LegacyRenderWidgetHostHWND instance here.
  if (legacy_render_widget_host_HWND_) {
    // The Destroy call below will delete the LegacyRenderWidgetHostHWND
    // instance.
    legacy_render_widget_host_HWND_.ExtractAsDangling()->Destroy();
  }
#endif

  // Make sure that the input method no longer references to this object before
  // this object is removed from the root window (i.e. this object loses access
  // to the input method).
  DetachFromInputMethod(true);

  if (overscroll_controller_)
    overscroll_controller_->Reset();
}

void RenderWidgetHostViewAura::OnWindowDestroyed(aura::Window* window) {
  // This is not called on all destruction paths (e.g. if this view was never
  // inialized properly to create the window). So the destruction/cleanup code
  // that do not depend on |window_| should happen in the destructor, not here.
  delete this;
}

void RenderWidgetHostViewAura::OnWindowTargetVisibilityChanged(bool visible) {
}

bool RenderWidgetHostViewAura::HasHitTestMask() const {
  return false;
}

void RenderWidgetHostViewAura::GetHitTestMask(SkPath* mask) const {}

bool RenderWidgetHostViewAura::RequiresDoubleTapGestureEvents() const {
  RenderWidgetHostOwnerDelegate* owner_delegate = host()->owner_delegate();
  // TODO(crbug.com/41432676): Child local roots do not work here?
  if (!owner_delegate)
    return false;
  return double_tap_to_zoom_enabled_;
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, ui::EventHandler implementation:

void RenderWidgetHostViewAura::OnKeyEvent(ui::KeyEvent* event) {
  last_pointer_type_ = ui::EventPointerType::kUnknown;
  event_handler_->OnKeyEvent(event);
}

void RenderWidgetHostViewAura::OnMouseEvent(ui::MouseEvent* event) {
#if BUILDFLAG(IS_WIN)
  if (event->type() == ui::EventType::kMouseMoved) {
    if (event->location() == last_mouse_move_location_ &&
        event->movement().IsZero()) {
      event->SetHandled();
      return;
    }
    last_mouse_move_location_ = event->location();
  }
#endif
  last_pointer_type_ = ui::EventPointerType::kMouse;
  event_handler_->OnMouseEvent(event);
}

bool RenderWidgetHostViewAura::HasFallbackSurface() const {
  CHECK(delegated_frame_host_) << "Cannot be invoked during destruction.";
  return delegated_frame_host_->HasFallbackSurface();
}

bool RenderWidgetHostViewAura::TransformPointToCoordSpaceForView(
    const gfx::PointF& point,
    input::RenderWidgetHostViewInput* target_view,
    gfx::PointF* transformed_point) {
  CHECK(delegated_frame_host_) << "Cannot be invoked during destruction.";

  if (target_view == this) {
    *transformed_point = point;
    return true;
  }

  // In TransformPointToLocalCoordSpace() there is a Point-to-Pixel conversion,
  // but it is not necessary here because the final target view is responsible
  // for converting before computing the final transform.
  return target_view->TransformPointToLocalCoordSpace(point, GetFrameSinkId(),
                                                      transformed_point);
}

viz::FrameSinkId RenderWidgetHostViewAura::GetRootFrameSinkId() {
  if (!GetCompositor())
    return viz::FrameSinkId();

  return GetCompositor()->frame_sink_id();
}

viz::SurfaceId RenderWidgetHostViewAura::GetCurrentSurfaceId() const {
  CHECK(delegated_frame_host_) << "Cannot be invoked during destruction.";
  return delegated_frame_host_->GetCurrentSurfaceId();
}

void RenderWidgetHostViewAura::FocusedNodeChanged(
    bool editable,
    const gfx::Rect& node_bounds_in_screen) {
  // The last gesture most likely caused the focus change. The focus reason will
  // be incorrect if the focus was triggered without a user gesture.
  // TODO(https://crbug.com/824604): Get the focus reason from the renderer
  // process instead to get the true focus reason.
  last_pointer_type_before_focus_ = last_pointer_type_;

  auto* input_method = GetInputMethod();
  if (input_method)
    input_method->CancelComposition(this);
  has_composition_text_ = false;

#if BUILDFLAG(IS_WIN)
  if (window_ && virtual_keyboard_controller_win_) {
    virtual_keyboard_controller_win_->FocusedNodeChanged(editable);
  }
#endif
}

bool RenderWidgetHostViewAura::ShouldInitiateStylusWriting() {
#if BUILDFLAG(IS_WIN)
  // TODO(crbug.com/355578906): Check whether Windows Text Services Framework
  // Shell Handwriting API is available or supported by the OS.
  // return stylus_handwriting::win::IsStylusHandwritingWinEnabled();
  return false;
#else   // BUILDFLAG(IS_WIN)
  return false;
#endif  // BUILDFLAG(IS_WIN)
}

void RenderWidgetHostViewAura::OnStartStylusWriting() {
  // TODO(crbug.com/355578906): Call Windows Text Services Framework Shell
  // Handwriting API. Will call ITfHandwriting::RequestHandwritingForPointer to
  // display ink, then ITfHandwritingRequest::SetInputEvaluation to confirm
  // intent. RequestHandwritingForPointer is an asynchronous request, however
  // this method will be called after GestureScrollBegin, so intent can be
  // confirmed immediately. After intent is confirmed, the API will request that
  // focus is updated by calling ITfHandwritingSink::FocusHandwritingTarget.
  //
  // To handle ITfHandwritingSink::FocusHandwritingTarget, the browser will
  // request that focus be updated in the renderer based on the RECT from
  // ITfFocusHandwritingTargetArgs::GetPointerTargetInfo, which will be handled
  // via mojom::blink::FrameWidget::OnStartStylusWriting. Focus will only be set
  // on content eligible for handwriting. If focus cannot be set on content
  // eligible for handwriting with the RECT provided by GetPointerTargetInfo,
  // then focus will fallback to the eligible element that was initially tapped.
}

void RenderWidgetHostViewAura::OnEditElementFocusedForStylusWriting(
    const gfx::Rect& focused_edit_bounds,
    const gfx::Rect& caret_bounds) {
  // TODO(crbug.com/355578906): Update Windows Text Services Framework (TSF)
  // focus, stash relevant character bounds from the renderer, and notify the
  // TSF Shell Handwriting API that focus is set.
  //
  // There are 3 vital steps that need to be performed during this callback:
  // 1. TSF focus must be updated to reflect focus changes in the renderer, such
  //    that ITfThreadMgr::GetFocus returns the correct ITfDocumentMgr.
  // 2. Character bounding boxes from the renderer must be made available for
  //    ITextStoreACP::GetTextExt and ITextStoreACP::GetACPFromPoint to enable
  //    gesture recognition.
  // 3. The earlier FocusHandwritingTarget must be responded to by calling
  //    ITfFocusHandwritingTargetArgs::SetResponse.
  //
  // `SetResponse` must be called last in this sequence, signaling the Shell
  // Handwriting API may begin committing edits or collect character bounds
  // for evaluating gesture recognition using TSF/IME APIs.
  // Failure to update TSF focus before calling `SetResponse` will result in
  // either ink disappearing without making any modifications, or modifications
  // being committed to the wrong editable text region.
  // Failure to prepare character bounds in proximity of the location which was
  // requested by FocusHandwritingTarget before calling `SetResponse` may result
  // in the inability of Shell Handwriting to perform gestures (selection,
  // scratch out, split/join word, new-line) and may result in text being
  // inserted instead.
}

void RenderWidgetHostViewAura::OnEditElementFocusClearedForStylusWriting() {
  // TODO(crbug.com/355578906): Notify Windows Text Services Framework (TSF)
  // Shell Handwriting API [1] to cancel the inking session, causing ink to
  // disappear without making any modifications.
  // [1] `ITfFocusHandwritingTargetArgs::SetResponse(TF_NO_HANDWRITING_TARGET)`
}

void RenderWidgetHostViewAura::OnScrollEvent(ui::ScrollEvent* event) {
  event_handler_->OnScrollEvent(event);
}

void RenderWidgetHostViewAura::OnTouchEvent(ui::TouchEvent* event) {
  last_pointer_type_ = event->pointer_details().pointer_type;
  event_handler_->OnTouchEvent(event);
}

void RenderWidgetHostViewAura::OnGestureEvent(ui::GestureEvent* event) {
  last_pointer_type_ = event->details().primary_pointer_type();
  event_handler_->OnGestureEvent(event);
}

std::string_view RenderWidgetHostViewAura::GetLogContext() const {
  return "RenderWidgetHostViewAura";
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, wm::ActivationDelegate implementation:

bool RenderWidgetHostViewAura::ShouldActivate() const {
  aura::WindowTreeHost* host = window_->GetHost();
  if (!host)
    return true;
  const ui::Event* event = host->dispatcher()->current_event();
  return !event;
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, aura::client::CursorClientObserver implementation:

void RenderWidgetHostViewAura::OnCursorVisibilityChanged(bool is_visible) {
  NotifyRendererOfCursorVisibilityState(is_visible);
}

void RenderWidgetHostViewAura::OnSystemCursorSizeChanged(
    const gfx::Size& system_cursor_size) {
  UpdateSystemCursorSize(system_cursor_size);
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, aura::client::FocusChangeObserver implementation:

void RenderWidgetHostViewAura::OnWindowFocused(aura::Window* gained_focus,
                                               aura::Window* lost_focus) {
  if (window_ == gained_focus) {
    // We need to honor input bypass if the associated tab does not want input.
    // This gives the current focused window a chance to be the text input
    // client and handle events.
    if (host()->IsIgnoringInputEvents()) {
      return;
    }

    host()->GotFocus();
    UpdateActiveState(true);

    ui::InputMethod* input_method = GetInputMethod();
    if (input_method) {
      // Ask the system-wide IME to send all TextInputClient messages to |this|
      // object.
      input_method->SetFocusedTextInputClient(this);
    }

    ui::BrowserAccessibilityManager* manager =
        host()->GetRootBrowserAccessibilityManager();
    if (manager)
      manager->OnWindowFocused();
    return;
  }

  if (window_ != lost_focus) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  UpdateActiveState(false);
  host()->LostFocus();

  DetachFromInputMethod(false);

  // TODO(wjmaclean): Do we need to let TouchSelectionControllerClientAura
  // handle this, just in case it stomps on a new highlight in another view
  // that has just become focused? So far it doesn't appear to be a problem,
  // but we should keep an eye on it.
  selection_controller_->HideAndDisallowShowingAutomatically();

  if (overscroll_controller_)
    overscroll_controller_->Cancel();

  ui::BrowserAccessibilityManager* manager =
      host()->GetRootBrowserAccessibilityManager();
  if (manager)
    manager->OnWindowBlurred();

  // Close the child popup window if we lose focus (e.g. due to a JS alert or
  // system modal dialog). This is particularly important if
  // |popup_child_host_view_| has mouse capture.
  if (popup_child_host_view_)
    popup_child_host_view_->Shutdown();
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, aura::WindowTreeHostObserver implementation:

void RenderWidgetHostViewAura::OnHostMovedInPixels(aura::WindowTreeHost* host) {
  TRACE_EVENT0("ui", "RenderWidgetHostViewAura::OnHostMovedInPixels");

  UpdateScreenInfo();
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, RenderFrameMetadataProvider::Observer
// implementation:
void RenderWidgetHostViewAura::OnRenderFrameMetadataChangedAfterActivation(
    base::TimeTicks activation_time) {
  const cc::RenderFrameMetadata& metadata =
      host()->render_frame_metadata_provider()->LastRenderFrameMetadata();

  // TODO(crbug.com/40219248): Remove toSkColor and make all SkColor4f.
  SetContentBackgroundColor(metadata.root_background_color.toSkColor());
  if (inset_surface_id_.is_valid() && metadata.local_surface_id &&
      metadata.local_surface_id.value().is_valid() &&
      metadata.local_surface_id.value().IsSameOrNewerThan(inset_surface_id_)) {
    inset_surface_id_ = viz::LocalSurfaceId();
    ScrollFocusedEditableNodeIntoView();
  }

  if (metadata.selection.start != selection_start_ ||
      metadata.selection.end != selection_end_) {
    selection_start_ = metadata.selection.start;
    selection_end_ = metadata.selection.end;
    selection_controller_client_->UpdateClientSelectionBounds(selection_start_,
                                                              selection_end_);
  }
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, private:

RenderWidgetHostViewAura::~RenderWidgetHostViewAura() {
  host()->render_frame_metadata_provider()->RemoveObserver(this);

  // Ask the RWH to drop reference to us.
  host()->ViewDestroyed();

  // Dismiss any visible touch selection handles or touch selection menu.
  selection_controller_->HideAndDisallowShowingAutomatically();
  selection_controller_.reset();
  selection_controller_client_.reset();

  GetCursorManager()->ViewBeingDestroyed(this);

  delegated_frame_host_.reset();
  window_observer_.reset();
  if (window_) {
    if (window_->GetHost())
      window_->GetHost()->RemoveObserver(this);
    UnlockPointer();
    wm::SetTooltipText(window_, nullptr);

    // This call is usually no-op since |this| object is already removed from
    // the Aura root window and we don't have a way to get an input method
    // object associated with the window, but just in case.
    DetachFromInputMethod(true);
  }
  if (popup_parent_host_view_) {
    CHECK(!popup_parent_host_view_->popup_child_host_view_ ||
          popup_parent_host_view_->popup_child_host_view_ == this);
    popup_parent_host_view_->SetPopupChild(nullptr);
  }
  if (popup_child_host_view_) {
    CHECK(!popup_child_host_view_->popup_parent_host_view_ ||
          popup_child_host_view_->popup_parent_host_view_ == this);
    popup_child_host_view_->popup_parent_host_view_ = nullptr;
  }
  event_observer_for_popup_exit_.reset();

#if BUILDFLAG(IS_WIN)
  // The LegacyRenderWidgetHostHWND window should have been destroyed in
  // RenderWidgetHostViewAura::OnWindowDestroying and the pointer should
  // be set to NULL.
  CHECK(!legacy_render_widget_host_HWND_);
#endif

  if (text_input_manager_)
    text_input_manager_->RemoveObserver(this);
}

void RenderWidgetHostViewAura::CreateAuraWindow(aura::client::WindowType type) {
  CHECK(!window_);
  window_ = new aura::Window(this);
  window_->SetName("RenderWidgetHostViewAura");
  event_handler_->set_window(window_);
  window_observer_ = std::make_unique<WindowObserver>(this);

  wm::SetTooltipText(window_, &tooltip_);
  wm::SetActivationDelegate(window_, this);
  aura::client::SetFocusChangeObserver(window_, this);
  display_observer_.emplace(this);

  window_->SetType(type);
  window_->Init(ui::LAYER_SOLID_COLOR);
  window_->layer()->SetColor(GetBackgroundColor() ? *GetBackgroundColor()
                                                  : SK_ColorWHITE);
  UpdateFrameSinkIdRegistration();
}

void RenderWidgetHostViewAura::UpdateFrameSinkIdRegistration() {
  RenderWidgetHostViewBase::UpdateFrameSinkIdRegistration();

  // This needs to happen only after |window_| has been initialized using
  // Init(), because it needs to have the layer.
  if (window_) {
    window_->SetEmbedFrameSinkId(is_frame_sink_id_owner() ? frame_sink_id_
                                                          : viz::FrameSinkId());
  }
  delegated_frame_host_->SetIsFrameSinkIdOwner(is_frame_sink_id_owner());
}

void RenderWidgetHostViewAura::CreateDelegatedFrameHostClient() {
  delegated_frame_host_client_ =
      std::make_unique<DelegatedFrameHostClientAura>(this);
  delegated_frame_host_ = std::make_unique<DelegatedFrameHost>(
      frame_sink_id_, delegated_frame_host_client_.get(),
      false /* should_register_frame_sink_id */);
}

void RenderWidgetHostViewAura::UpdateCursorIfOverSelf() {
  if (host()->GetProcess()->FastShutdownStarted())
    return;

  aura::Window* root_window = window_->GetRootWindow();
  if (!root_window)
    return;

  if (ShouldSkipCursorUpdate())
    return;

  display::Screen* screen = display::Screen::GetScreen();
  CHECK(screen);
  gfx::Point root_window_point = screen->GetCursorScreenPoint();
  aura::client::ScreenPositionClient* screen_position_client =
      aura::client::GetScreenPositionClient(root_window);
  if (screen_position_client) {
    screen_position_client->ConvertPointFromScreen(
        root_window, &root_window_point);
  }

  if (root_window->GetEventHandlerForPoint(root_window_point) != window_)
    return;

  gfx::NativeCursor cursor = current_cursor_.GetNativeCursor();
  // Do not show loading cursor when the cursor is currently hidden.
  if (is_loading_ && cursor != ui::mojom::CursorType::kNone)
    cursor = ui::Cursor(ui::mojom::CursorType::kPointer);

  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(root_window);
  if (cursor_client) {
    cursor_client->SetCursor(cursor);
  }
}

bool RenderWidgetHostViewAura::SynchronizeVisualProperties(
    const cc::DeadlinePolicy& deadline_policy,
    const std::optional<viz::LocalSurfaceId>& child_local_surface_id) {
  CHECK(window_);
  CHECK(delegated_frame_host_) << "Cannot be invoked during destruction.";

  window_->UpdateLocalSurfaceIdFromEmbeddedClient(child_local_surface_id);
  // If the viz::LocalSurfaceId is invalid, we may have been evicted,
  // allocate a new one to establish bounds.
  if (!GetLocalSurfaceId().is_valid())
    window_->AllocateLocalSurfaceId();

  delegated_frame_host_->EmbedSurface(
      GetLocalSurfaceId(), window_->bounds().size(), deadline_policy);

  return host()->SynchronizeVisualProperties();
}

void RenderWidgetHostViewAura::OnDidUpdateVisualPropertiesComplete(
    const cc::RenderFrameMetadata& metadata) {
  CHECK(window_);

  if (host()->delegate()) {
    host()->delegate()->SetTopControlsShownRatio(
        host(), metadata.top_controls_shown_ratio);
  }

  if (host()->is_hidden()) {
    // When an embedded child responds, we want to accept its changes to the
    // viz::LocalSurfaceId. However we do not want to embed surfaces while
    // hidden. Nor do we want to embed invalid ids when we are evicted. Becoming
    // visible will generate a new id, if necessary, and begin embedding.
    window_->UpdateLocalSurfaceIdFromEmbeddedClient(metadata.local_surface_id);
  } else {
    SynchronizeVisualProperties(cc::DeadlinePolicy::UseDefaultDeadline(),
                                metadata.local_surface_id);
  }
}

ui::InputMethod* RenderWidgetHostViewAura::GetInputMethod() const {
  if (!window_)
    return nullptr;
  aura::Window* root_window = window_->GetRootWindow();
  if (!root_window)
    return nullptr;
  return root_window->GetHost()->GetInputMethod();
}

RenderWidgetHostViewBase*
RenderWidgetHostViewAura::GetFocusedViewForTextSelection() const {
  // We obtain the TextSelection from focused RWH which is obtained from the
  // frame tree.
  return GetFocusedWidget() ? GetFocusedWidget()->GetView() : nullptr;
}

void RenderWidgetHostViewAura::Shutdown() {
  if (!in_shutdown_) {
    in_shutdown_ = true;
    host()->ShutdownAndDestroyWidget(true);
  }
}

ui::mojom::VirtualKeyboardMode
RenderWidgetHostViewAura::GetVirtualKeyboardMode() {
  // overlaycontent flag can only be set from main frame.
  RenderFrameHostImpl* frame = host()->frame_tree()->GetMainFrame();
  if (!frame)
    return ui::mojom::VirtualKeyboardMode::kUnset;

  return frame->GetPage().virtual_keyboard_mode();
}

void RenderWidgetHostViewAura::NotifyVirtualKeyboardOverlayRect(
    const gfx::Rect& keyboard_rect) {
  // geometrychange event can only be fired on main frame and not focused frame
  // which could be an iframe.
  RenderFrameHostImpl* frame = host()->frame_tree()->GetMainFrame();
  if (!frame)
    return;

  if (GetVirtualKeyboardMode() !=
      ui::mojom::VirtualKeyboardMode::kOverlaysContent) {
    return;
  }
  gfx::Rect keyboard_root_relative_rect = keyboard_rect;
  if (!keyboard_root_relative_rect.IsEmpty()) {
    // If the rect is non-empty, we need to transform it to be widget-relative
    // window (DIP coordinates). The input is client coordinates for the root
    // window.
    // Transform the widget rect origin to root relative coords.
    gfx::PointF root_widget_origin(0.f, 0.f);
    TransformPointToRootSurface(&root_widget_origin);
    gfx::Rect root_widget_rect =
        gfx::Rect(root_widget_origin.x(), root_widget_origin.y(),
                  GetViewBounds().width(), GetViewBounds().height());
    // Intersect the keyboard rect with the root widget bounds and transform
    // back to widget-relative coordinates, which will be sent to the renderer.
    keyboard_root_relative_rect.Intersect(root_widget_rect);
    keyboard_root_relative_rect.Offset(-root_widget_origin.x(),
                                       -root_widget_origin.y());
  }
  frame->GetPage().NotifyVirtualKeyboardOverlayRect(
      keyboard_root_relative_rect);
}

bool RenderWidgetHostViewAura::IsHTMLFormPopup() const {
  return !!popup_parent_host_view_;
}

void RenderWidgetHostViewAura::ResetGestureDetection() {
  // TODO(bokan): See the Android implementation - Aura likely needs to
  // implement this as well so that suppressing input
  // (WebContentsImpl::IgnoreInputEvents) doesn't continue to generate gestures
  // which can confuse event validation.
}

bool RenderWidgetHostViewAura::FocusedFrameHasStickyActivation() const {
  // Unless user has interacted with the iframe, we shouldn't be displaying VK
  // or fire geometrychange event.
  RenderFrameHostImpl* frame = GetFocusedFrame();
  if (!frame)
    return false;

  return frame->frame_tree_node()->HasStickyUserActivation();
}

TouchSelectionControllerClientManager*
RenderWidgetHostViewAura::GetTouchSelectionControllerClientManager() {
  return selection_controller_client_.get();
}

bool RenderWidgetHostViewAura::NeedsInputGrab() {
  return widget_type_ == WidgetType::kPopup;
}

bool RenderWidgetHostViewAura::NeedsMouseCapture() {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  return NeedsInputGrab();
#else
  return false;
#endif
}

void RenderWidgetHostViewAura::SetTooltipsEnabled(bool enable) {
  if (enable) {
    tooltip_disabler_.reset();
  } else {
    tooltip_disabler_ =
        std::make_unique<wm::ScopedTooltipDisabler>(window_->GetRootWindow());
  }
}

void RenderWidgetHostViewAura::NotifyRendererOfCursorVisibilityState(
    bool is_visible) {
  if (host()->is_hidden() ||
      (cursor_visibility_state_in_renderer_ == VISIBLE && is_visible) ||
      (cursor_visibility_state_in_renderer_ == NOT_VISIBLE && !is_visible))
    return;

  cursor_visibility_state_in_renderer_ = is_visible ? VISIBLE : NOT_VISIBLE;
  host()->OnCursorVisibilityStateChanged(is_visible);
}

void RenderWidgetHostViewAura::SetOverscrollControllerEnabled(bool enabled) {
  if (!enabled)
    overscroll_controller_.reset();
  else if (!overscroll_controller_)
    overscroll_controller_ = std::make_unique<OverscrollController>();
}

void RenderWidgetHostViewAura::SetSelectionControllerClientForTest(
    std::unique_ptr<TouchSelectionControllerClientAura> client) {
  selection_controller_client_.swap(client);
  CreateSelectionController();
}

void RenderWidgetHostViewAura::InternalSetBounds(const gfx::Rect& rect) {
  // Don't recursively call SetBounds if this bounds update is the result of
  // a Window::SetBoundsInternal call.
  if (!in_bounds_changed_)
    window_->SetBounds(rect);

  if (!display_feature_bounds_.IsEmpty()) {
    // The view bounds have changed so if we have viewport segments from the
    // platform we need to make sure display_feature_ is updated considering the
    // new view bounds.
    ComputeDisplayFeature();
  }

  // Even if not showing yet, we need to synchronize on size. As the renderer
  // needs to begin layout. Waiting until we show to start layout leads to
  // significant delays in embedding the first shown surface (500+ ms.)
  SynchronizeVisualProperties(cc::DeadlinePolicy::UseDefaultDeadline(),
                              window_->GetLocalSurfaceId());

#if BUILDFLAG(IS_WIN)
  UpdateLegacyWin();

  if (IsPointerLocked()) {
    UpdateMouseLockRegion();
  }
#endif
}

void RenderWidgetHostViewAura::UpdateInsetsWithVirtualKeyboardEnabled() {
  // Update insets if the keyboard is shown.
  if (!keyboard_occluded_bounds_.IsEmpty()) {
    SetInsets(gfx::Insets::TLBR(
        0, 0,
        gfx::IntersectRects(GetViewBounds(), keyboard_occluded_bounds_)
            .height(),
        0));
  }
}

#if BUILDFLAG(IS_WIN)
void RenderWidgetHostViewAura::UpdateLegacyWin() {
  if (legacy_window_destroyed_ || !GetHostWindowHWND())
    return;

  if (!legacy_render_widget_host_HWND_) {
    legacy_render_widget_host_HWND_ =
        LegacyRenderWidgetHostHWND::Create(GetHostWindowHWND(), this);
  }

  if (legacy_render_widget_host_HWND_) {
    legacy_render_widget_host_HWND_->UpdateParent(GetHostWindowHWND());
    legacy_render_widget_host_HWND_->SetBounds(
        window_->GetBoundsInRootWindow());
    // There are cases where the parent window is created, made visible and
    // the associated RenderWidget is also visible before the
    // LegacyRenderWidgetHostHWND instace is created. Ensure that it is shown
    // here.
    if (!host()->is_hidden())
      legacy_render_widget_host_HWND_->Show();
  }
}
#endif

void RenderWidgetHostViewAura::AddedToRootWindow() {
  CHECK(delegated_frame_host_) << "Cannot be invoked during destruction.";

  window_->GetHost()->AddObserver(this);
  UpdateScreenInfo();

  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(window_->GetRootWindow());
  if (cursor_client) {
    cursor_client->AddObserver(this);
    NotifyRendererOfCursorVisibilityState(cursor_client->IsCursorVisible());
  }
  if (HasFocus()) {
    ui::InputMethod* input_method = GetInputMethod();
    if (input_method)
      input_method->SetFocusedTextInputClient(this);
  }

#if BUILDFLAG(IS_WIN)
  UpdateLegacyWin();
#endif

  delegated_frame_host_->AttachToCompositor(GetCompositor());
}

void RenderWidgetHostViewAura::RemovingFromRootWindow() {
  CHECK(delegated_frame_host_) << "Cannot be invoked during destruction.";

  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(window_->GetRootWindow());
  if (cursor_client)
    cursor_client->RemoveObserver(this);

  DetachFromInputMethod(true);

  window_->GetHost()->RemoveObserver(this);
  delegated_frame_host_->DetachFromCompositor();

#if BUILDFLAG(IS_WIN)
    // Update the legacy window's parent temporarily to the hidden window. It
    // will eventually get reparented to the right root.
    if (legacy_render_widget_host_HWND_)
      legacy_render_widget_host_HWND_->UpdateParent(ui::GetHiddenWindow());
#endif
}

void RenderWidgetHostViewAura::DetachFromInputMethod(bool is_removed) {
  ui::InputMethod* input_method = GetInputMethod();
  if (input_method) {
    input_method->DetachTextInputClient(this);
#if BUILDFLAG(IS_CHROMEOS_ASH)
    wm::RestoreWindowBoundsOnClientFocusLost(window_->GetToplevelWindow());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }

#if BUILDFLAG(IS_WIN)
  // If window is getting destroyed, then reset the VK controller, else,
  // dismiss the VK and notify about the keyboard inset since window has lost
  // focus.
  if (virtual_keyboard_controller_win_) {
    if (is_removed)
      virtual_keyboard_controller_win_.reset();
    else
      virtual_keyboard_controller_win_->HideAndNotifyKeyboardInset();
  }
#endif  // BUILDFLAG(IS_WIN)
}

void RenderWidgetHostViewAura::ForwardKeyboardEventWithLatencyInfo(
    const input::NativeWebKeyboardEvent& event,
    const ui::LatencyInfo& latency,
    bool* update_event) {
  RenderWidgetHostImpl* target_host = host();

  // If there are multiple widgets on the page (such as when there are
  // out-of-process iframes), pick the one that should process this event.
  if (host()->delegate())
    target_host = host()->delegate()->GetFocusedRenderWidgetHost(host());
  if (!target_host)
    return;

#if BUILDFLAG(IS_LINUX)
  auto* linux_ui = ui::LinuxUi::instance();
  std::vector<ui::TextEditCommandAuraLinux> commands;
  if (!event.skip_if_unhandled && linux_ui && event.os_event &&
      linux_ui->GetTextEditCommandsForEvent(*event.os_event,
                                            GetTextInputFlags(), &commands)) {
    // Transform from ui/ types to content/ types.
    std::vector<blink::mojom::EditCommandPtr> edit_commands;
    for (std::vector<ui::TextEditCommandAuraLinux>::const_iterator it =
             commands.begin(); it != commands.end(); ++it) {
      edit_commands.push_back(blink::mojom::EditCommand::New(
          it->GetCommandString(), it->argument()));
    }

    target_host->ForwardKeyboardEventWithCommands(
        event, latency, std::move(edit_commands), update_event);
    return;
  }
#endif

  target_host->ForwardKeyboardEventWithCommands(
      event, latency, std::vector<blink::mojom::EditCommandPtr>(),
      update_event);
}

void RenderWidgetHostViewAura::CreateSelectionController() {
  ui::TouchSelectionController::Config tsc_config;
  tsc_config.max_tap_duration = base::Milliseconds(
      ui::GestureConfiguration::GetInstance()->long_press_time_in_ms());
  tsc_config.tap_slop = ui::GestureConfiguration::GetInstance()
                            ->max_touch_move_in_pixels_for_click();
  tsc_config.enable_longpress_drag_selection =
      features::IsTouchTextEditingRedesignEnabled();
  selection_controller_ = std::make_unique<ui::TouchSelectionController>(
      selection_controller_client_.get(), tsc_config);
}

void RenderWidgetHostViewAura::OnOldViewDidNavigatePreCommit() {
  CHECK(delegated_frame_host_) << "Cannot be invoked during destruction.";

  // Invalidate the surface so that we don't attempt to evict it multiple times.
  window_->InvalidateLocalSurfaceId();
  delegated_frame_host_->DidNavigateMainFramePreCommit();
}

void RenderWidgetHostViewAura::OnNewViewDidNavigatePostCommit() {
  CancelActiveTouches();
}

void RenderWidgetHostViewAura::DidEnterBackForwardCache() {
  CHECK(delegated_frame_host_) << "Cannot be invoked during destruction.";

  window_->AllocateLocalSurfaceId();
  delegated_frame_host_->DidEnterBackForwardCache();
  // If we have the fallback content timer running, force it to stop. Else, when
  // the page is restored the timer could also fire, setting whatever
  // `DelegatedFrameHost::first_local_surface_id_after_navigation_` as the
  // fallback to our Surfacelayer.
  //
  // This is safe for BFCache restore because we will supply specific fallback
  // surfaces for BFCache.
  //
  // We do not want to call this in `RWHImpl::WasHidden()` because in the case
  // of `Visibility::OCCLUDED` we still want to keep the timer running.
  //
  // Called after to prevent prematurely evict the BFCached surface.
  host()->ForceFirstFrameAfterNavigationTimeout();
}

const viz::FrameSinkId& RenderWidgetHostViewAura::GetFrameSinkId() const {
  return frame_sink_id_;
}

const viz::LocalSurfaceId& RenderWidgetHostViewAura::GetLocalSurfaceId() const {
  return window_->GetLocalSurfaceId();
}

void RenderWidgetHostViewAura::OnUpdateTextInputStateCalled(
    TextInputManager* text_input_manager,
    RenderWidgetHostViewBase* updated_view,
    bool did_update_state) {
  CHECK_EQ(text_input_manager_, text_input_manager);

  if (!GetInputMethod())
    return;

  if (did_update_state)
    GetInputMethod()->OnTextInputTypeChanged(this);

  const ui::mojom::TextInputState* state =
      text_input_manager_->GetTextInputState();

#if BUILDFLAG(IS_CHROMEOS)
  if (state && state->type != ui::TEXT_INPUT_TYPE_NONE) {
    if (state->last_vk_visibility_request ==
        ui::mojom::VirtualKeyboardVisibilityRequest::SHOW) {
      GetInputMethod()->SetVirtualKeyboardVisibilityIfEnabled(true);
    } else if (state->last_vk_visibility_request ==
               ui::mojom::VirtualKeyboardVisibilityRequest::HIDE) {
      GetInputMethod()->SetVirtualKeyboardVisibilityIfEnabled(false);
    }
  }
#endif

  // Show the virtual keyboard if needed.
  if (state && state->type != ui::TEXT_INPUT_TYPE_NONE &&
      state->mode != ui::TEXT_INPUT_MODE_NONE) {
#if !BUILDFLAG(IS_WIN)
    if (state->show_ime_if_needed &&
        GetInputMethod()->GetTextInputClient() == this) {
      GetInputMethod()->SetVirtualKeyboardVisibilityIfEnabled(true);
    }
// TODO(crbug.com/40110609): Remove this once TSF fix for input pane policy
// is serviced
#elif BUILDFLAG(IS_WIN)
    if (GetInputMethod()) {
      if (!virtual_keyboard_controller_win_) {
        virtual_keyboard_controller_win_ =
            std::make_unique<VirtualKeyboardControllerWin>(this,
                                                           GetInputMethod());
      }
      virtual_keyboard_controller_win_->UpdateTextInputState(state);
    }
#endif
  }

  // Ensure that selection bounds changes are sent to the IME.
  if (state && state->type != ui::TEXT_INPUT_TYPE_NONE) {
    text_input_manager->NotifySelectionBoundsChanged(updated_view);
  }

  if (auto* render_widget_host = updated_view->host()) {
    // Monitor the composition information if there is a focused editable node.
    render_widget_host->RequestCompositionUpdates(
        false /* immediate_request */,
        state &&
            (state->type != ui::TEXT_INPUT_TYPE_NONE) /* monitor_updates */);
  }
}

void RenderWidgetHostViewAura::OnImeCancelComposition(
    TextInputManager* text_input_manager,
    RenderWidgetHostViewBase* view) {
  // |view| is not necessarily the one corresponding to
  // TextInputManager::GetActiveWidget() as RenderWidgetHostViewAura can call
  // this method to finish any ongoing composition in response to a mouse down
  // event.
  if (GetInputMethod())
    GetInputMethod()->CancelComposition(this);
  has_composition_text_ = false;
}

void RenderWidgetHostViewAura::OnSelectionBoundsChanged(
    TextInputManager* text_input_manager,
    RenderWidgetHostViewBase* updated_view) {
  // Note: accessibility caret move events are no longer fired directly here,
  // because they were redundant with the events fired by the top level window
  // by HWNDMessageHandler::OnCaretBoundsChanged().
  if (GetInputMethod())
    GetInputMethod()->OnCaretBoundsChanged(this);
}

void RenderWidgetHostViewAura::OnTextSelectionChanged(
    TextInputManager* text_input_manager,
    RenderWidgetHostViewBase* updated_view) {
  if (!GetTextInputManager())
    return;

  // We obtain the TextSelection from focused RWH which is obtained from the
  // frame tree.
  RenderWidgetHostViewBase* focused_view =
      GetFocusedWidget() ? GetFocusedWidget()->GetView() : nullptr;

  if (!focused_view)
    return;

  // IMF relies on the |OnCaretBoundsChanged| for the surrounding text changed
  // events to IME. Explicitly call |OnCaretBoundsChanged| here so that IMF can
  // know about the surrounding text changes when the caret bounds are not
  // changed. e.g. When the rendered text is wider than the input field,
  // deleting the last character won't change the caret bounds but will change
  // the surrounding text.
  if (GetInputMethod())
    GetInputMethod()->OnCaretBoundsChanged(this);

  if (ui::Clipboard::IsSupportedClipboardBuffer(
          ui::ClipboardBuffer::kSelection)) {
    const TextInputManager::TextSelection* selection =
        GetTextInputManager()->GetTextSelection(focused_view);
    if (selection->selected_text().length()) {
      // Set the ClipboardBuffer::kSelection to the ui::Clipboard.
      ui::ScopedClipboardWriter clipboard_writer(
          ui::ClipboardBuffer::kSelection);
      clipboard_writer.WriteText(selection->selected_text());
    }
  }
}

void RenderWidgetHostViewAura::SetPopupChild(
    RenderWidgetHostViewAura* popup_child_host_view) {
  popup_child_host_view_ = popup_child_host_view;
  event_handler_->SetPopupChild(
      popup_child_host_view,
      popup_child_host_view ? popup_child_host_view->event_handler() : nullptr);
}

void RenderWidgetHostViewAura::ScrollFocusedEditableNodeIntoView() {
  auto* input_handler = GetFrameWidgetInputHandlerForFocusedWidget();
  if (!input_handler)
    return;
  input_handler->ScrollFocusedEditableNodeIntoView();
}

void RenderWidgetHostViewAura::OnSynchronizedDisplayPropertiesChanged(
    bool rotation) {
  SynchronizeVisualProperties(cc::DeadlinePolicy::UseDefaultDeadline(),
                              std::nullopt);
}

viz::ScopedSurfaceIdAllocator
RenderWidgetHostViewAura::DidUpdateVisualProperties(
    const cc::RenderFrameMetadata& metadata) {
  base::OnceCallback<void()> allocation_task = base::BindOnce(
      &RenderWidgetHostViewAura::OnDidUpdateVisualPropertiesComplete,
      weak_ptr_factory_.GetWeakPtr(), metadata);
  return window_->GetSurfaceIdAllocator(std::move(allocation_task));
}

void RenderWidgetHostViewAura::DidNavigate() {
  CHECK(delegated_frame_host_) << "Cannot be invoked during destruction.";

  if (!IsShowing()) {
    // Navigating while hidden should not allocate a new LocalSurfaceID. Once
    // sizes are ready, or we begin to Show, we can then allocate the new
    // LocalSurfaceId.
    window_->InvalidateLocalSurfaceId();
  } else {
    if (is_first_navigation_) {
      // The first navigation does not need a new LocalSurfaceID. The renderer
      // can use the ID that was already provided.
      SynchronizeVisualProperties(cc::DeadlinePolicy::UseExistingDeadline(),
                                  window_->GetLocalSurfaceId());
    } else {
      SynchronizeVisualProperties(cc::DeadlinePolicy::UseExistingDeadline(),
                                  std::nullopt);
    }
  }
  delegated_frame_host_->DidNavigate();
  is_first_navigation_ = false;
}

MouseWheelPhaseHandler* RenderWidgetHostViewAura::GetMouseWheelPhaseHandler() {
  return &event_handler_->mouse_wheel_phase_handler();
}

void RenderWidgetHostViewAura::TakeFallbackContentFrom(
    RenderWidgetHostView* view) {
  CHECK(!static_cast<RenderWidgetHostViewBase*>(view)
             ->IsRenderWidgetHostViewChildFrame());
  RenderWidgetHostViewAura* view_aura =
      static_cast<RenderWidgetHostViewAura*>(view);
  CopyBackgroundColorIfPresentFrom(*view);

  CHECK(delegated_frame_host_) << "Cannot be invoked during destruction.";
  CHECK(view_aura->delegated_frame_host_);
  delegated_frame_host_->TakeFallbackContentFrom(
      view_aura->delegated_frame_host_.get());
}

bool RenderWidgetHostViewAura::CanSynchronizeVisualProperties() {
  return !needs_to_update_display_metrics_;
}

void RenderWidgetHostViewAura::SetLastPointerType(
    ui::EventPointerType last_pointer_type) {
  last_pointer_type_ = last_pointer_type;
}

void RenderWidgetHostViewAura::InvalidateLocalSurfaceIdAndAllocationGroup() {
  window_->InvalidateLocalSurfaceId(/*also_invalidate_allocation_group=*/true);
}

void RenderWidgetHostViewAura::InvalidateLocalSurfaceIdOnEviction() {
  window_->InvalidateLocalSurfaceId();
}

void RenderWidgetHostViewAura::ProcessDisplayMetricsChanged() {
  // TODO(crbug.com/40165350): Unify per-platform DisplayObserver instances.
  needs_to_update_display_metrics_ = false;
  UpdateScreenInfo();
  current_cursor_.UpdateDisplayInfoForWindow(window_);
  UpdateCursorIfOverSelf();
}

void RenderWidgetHostViewAura::CancelActiveTouches() {
  aura::Env* env = aura::Env::GetInstance();
  env->gesture_recognizer()->CancelActiveTouches(window());
}

blink::mojom::FrameWidgetInputHandler*
RenderWidgetHostViewAura::GetFrameWidgetInputHandlerForFocusedWidget() {
  auto* focused_widget = GetFocusedWidget();
  if (!focused_widget)
    return nullptr;
  return focused_widget->GetFrameWidgetInputHandler();
}

void RenderWidgetHostViewAura::SetTooltipText(
    const std::u16string& tooltip_text) {
  tooltip_ = tooltip_text;
  if (tooltip_observer_for_testing_)
    tooltip_observer_for_testing_->OnTooltipTextUpdated(tooltip_text);
}

ui::Compositor* RenderWidgetHostViewAura::GetCompositor() {
  if (!window_ || !window_->GetHost())
    return nullptr;

  return window_->GetHost()->compositor();
}

}  // namespace content
