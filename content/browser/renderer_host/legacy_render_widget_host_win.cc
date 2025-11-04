// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/legacy_render_widget_host_win.h"

#include <objbase.h>

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/win/win_util.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "content/browser/renderer_host/direct_manipulation_helper_win.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "content/common/features.h"
#include "content/public/common/content_switches.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/platform/ax_fragment_root_win.h"
#include "ui/accessibility/platform/ax_platform.h"
#include "ui/accessibility/platform/ax_system_caret_win.h"
#include "ui/accessibility/platform/browser_accessibility_manager_win.h"
#include "ui/accessibility/platform/browser_accessibility_win.h"
#include "ui/accessibility/platform/one_shot_accessibility_tree_search.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/view_prop.h"
#include "ui/base/win/hidden_window.h"
#include "ui/base/win/internal_constants.h"
#include "ui/base/win/window_event_target.h"
#include "ui/display/win/screen_win.h"
#include "ui/gfx/geometry/rect.h"

namespace content {

// A custom MSAA object id used to determine if a screen reader or some
// other client is listening on MSAA events - if so, we enable full web
// accessibility support.
static constexpr int kIdScreenReaderHoneyPot = 1;

// static
LegacyRenderWidgetHostHWND* LegacyRenderWidgetHostHWND::Create(
    HWND parent,
    RenderWidgetHostViewAura* host) {
  // content_unittests passes in the desktop window as the parent. We allow
  // the LegacyRenderWidgetHostHWND instance to be created in this case for
  // these tests to pass.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableLegacyIntermediateWindow) ||
      (!GetWindowEventTarget(parent) && parent != ::GetDesktopWindow())) {
    return nullptr;
  }

  auto* legacy_window_instance = new LegacyRenderWidgetHostHWND(host);
  if (!legacy_window_instance->InitOrDeleteSelf(parent)) {
    return nullptr;
  }

  return legacy_window_instance;
}

void LegacyRenderWidgetHostHWND::Destroy() {
  // Delete DirectManipulationHelper before the window is destroyed.
  direct_manipulation_helper_.reset();
  window_tree_host_prop_.reset();
  host_ = nullptr;
  if (::IsWindow(hwnd())) {
    ::DestroyWindow(hwnd());
  }
}

// TODO(crbug.com/424432184): Remove this function when the feature is cleaned
// up.
void LegacyRenderWidgetHostHWND::CreateDirectManipulationHelper() {
  CHECK(!base::FeatureList::IsEnabled(
      features::kUpdateDirectManipulationHelperOnParentChange));
  // Direct Manipulation is enabled on Windows 10+. The CreateInstance function
  // returns NULL if Direct Manipulation is not available. Recreate
  // |direct_manipulation_helper_| when parent changed (compositor and window
  // event target updated).
  direct_manipulation_helper_ =
      DirectManipulationHelper::CreateInstance(hwnd());
  if (direct_manipulation_helper_) {
    direct_manipulation_helper_->UpdateEventHandler(
        host_->GetNativeView()->GetHost()->GetWeakPtr(),
        GetWindowEventTarget(GetParent()));
  }
}

void LegacyRenderWidgetHostHWND::UpdateParent(HWND new_parent) {
  const bool only_update_direct_manipulation_helper =
      base::FeatureList::IsEnabled(
          features::kUpdateDirectManipulationHelperOnParentChange);

  // Performance profiles for resizing show that roughly 1/3 of the
  // browser main thread CPU samples are inside of the ::SetParent call, even
  // though the parent is never changed during this operation. The CPU samples
  // disappear if we ask the OS for the current parent and avoid the SetParent
  // call altogether.
  const HWND current_parent = GetParent();
  if (current_parent != new_parent) {
    ::SetParent(hwnd(), new_parent);

    if (!only_update_direct_manipulation_helper) {
      CreateDirectManipulationHelper();
    }

    // Reset tooltips when parent changed; otherwise tooltips could stay open as
    // the former parent wouldn't be forwarded any mouse leave messages.
    host_->UpdateTooltip(std::u16string());

    // Store parent before hide to reroute pointer events while hidden.
    // See comment in OnPointer for more details.
    if (new_parent == ui::GetHiddenWindow() &&
        down_pointers_before_hide_.size() > 0) {
      parent_before_hide_ = current_parent;
    } else if (current_parent == ui::GetHiddenWindow()) {
      parent_before_hide_ = nullptr;
    }
  } else {
    // The first call to UpdateParent may have the parent correctly set on
    // account of InitOrDeleteSelf having just created the correctly parented
    // Window. We will need to create the DirectManipulationHelper in this case
    // if we haven't already done so. After initial creation, the
    // DirectManipulationHelper only needs to be re-created if the parent
    // subsequently changes.
    if (!only_update_direct_manipulation_helper &&
        !direct_manipulation_helper_) {
      CreateDirectManipulationHelper();
    }
  }

  if (only_update_direct_manipulation_helper) {
    // The DirectManipulationHelper was created in InitOrDeleteSelf. It must be
    // initialized on the first call to UpdateParent. After that it only needs
    // to be updated if the parent changes.
    if (direct_manipulation_helper_ &&
        (!direct_manipulation_helper_->event_target() ||
         current_parent != new_parent)) {
      direct_manipulation_helper_->UpdateEventHandler(
          host_->GetNativeView()->GetHost()->GetWeakPtr(),
          GetWindowEventTarget(new_parent));
    }
  }
}

HWND LegacyRenderWidgetHostHWND::GetParent() {
  return ::GetParent(hwnd());
}

void LegacyRenderWidgetHostHWND::Show() {
  ::ShowWindow(hwnd(), SW_SHOW);
}

void LegacyRenderWidgetHostHWND::Hide() {
  ::ShowWindow(hwnd(), SW_HIDE);
}

void LegacyRenderWidgetHostHWND::SetBounds(const gfx::Rect& bounds) {
  gfx::Rect bounds_in_pixel =
      display::win::GetScreenWin()->DIPToClientRect(hwnd(), bounds);
  ::SetWindowPos(hwnd(), nullptr, bounds_in_pixel.x(), bounds_in_pixel.y(),
                 bounds_in_pixel.width(), bounds_in_pixel.height(),
                 SWP_NOREDRAW);
  if (direct_manipulation_helper_) {
    direct_manipulation_helper_->SetSizeInPixels(bounds_in_pixel.size());
  }
}

void LegacyRenderWidgetHostHWND::OnFinalMessage(HWND hwnd) {
  if (host_) {
    host_->OnLegacyWindowDestroyed();
    host_ = nullptr;
  }

  // Re-enable flicks for just a moment
  base::win::EnableFlicks(hwnd);

  delete this;
}

LegacyRenderWidgetHostHWND::LegacyRenderWidgetHostHWND(
    RenderWidgetHostViewAura* host)
    : host_(host) {}

LegacyRenderWidgetHostHWND::~LegacyRenderWidgetHostHWND() {
  DCHECK(!::IsWindow(hwnd()));
}

bool LegacyRenderWidgetHostHWND::InitOrDeleteSelf(HWND parent) {
  // Need to use weak_ptr to guard against `this` from being deleted by
  // Base::Create(), which used to be called in the constructor and caused
  // heap-use-after-free crash (https://crbug.com/1194694).
  auto weak_ptr = msg_handler_weak_factory_.GetWeakPtr();
  RECT rect = {0};
  Base::Create(parent, rect, L"Chrome Legacy Window",
               WS_CHILDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
               WS_EX_TRANSPARENT);
  if (!weak_ptr) {
    // Base::Create() runs nested windows message loops that could end up
    // deleting `this`. Therefore, upon returning false here, `this` is already
    // deleted.
    return false;
  }

  // We create a system caret regardless of accessibility mode since not all
  // assistive software that makes use of a caret is classified as a screen
  // reader, e.g. the built-in Windows Magnifier.
  ax_system_caret_ = std::make_unique<ui::AXSystemCaretWin>(hwnd());

  // If we failed to create the child, then return false.
  if (!::IsWindow(hwnd())) {
    delete this;
    return false;
  }

  // Ignore failure from this call. Some SKUs of Windows such as Hololens do not
  // support MSAA, and this call failing should not stop us from initializing
  // UI Automation support.
  ::CreateStdAccessibleObject(hwnd(), OBJID_WINDOW,
                              IID_PPV_ARGS(&window_accessible_));

  if (::ui::AXPlatform::GetInstance().IsUiaProviderEnabled()) {
    // The usual way for UI Automation to obtain a fragment root is through
    // WM_GETOBJECT. However, if there's a relation such as "Controller For"
    // between element A in one window and element B in another window, UIA
    // might call element A to discover the relation, receive a pointer to
    // element B, then ask element B for its fragment root, without having sent
    // WM_GETOBJECT to element B's window. So we create the fragment root now to
    // ensure it's ready if asked for.
    ax_fragment_root_ = std::make_unique<ui::AXFragmentRootWin>(hwnd(), this);
  }

  // Continue to send honey pot events until we have kWebContents to
  // ensure screen readers have the opportunity to enable.
  ui::AXMode mode =
      BrowserAccessibilityStateImpl::GetInstance()->GetAccessibilityMode();
  if (!mode.has_mode(ui::AXMode::kWebContents)) {
    // Attempt to detect screen readers or other clients who want full
    // accessibility support, by seeing if they respond to this event.
    NotifyWinEvent(EVENT_SYSTEM_ALERT, hwnd(), kIdScreenReaderHoneyPot,
                   CHILDID_SELF);
  }

  // Disable pen flicks (http://crbug.com/506977)
  base::win::DisableFlicks(hwnd());

  host_->UpdateTooltip(std::u16string());

  // Instruct aura::WindowTreeHost to use the HWND's parent for lookup.
  window_tree_host_prop_ = std::make_unique<ui::ViewProp>(
      hwnd(), aura::WindowTreeHost::kWindowTreeHostUsesParent,
      reinterpret_cast<HANDLE>(true));

  if (base::FeatureList::IsEnabled(
          features::kUpdateDirectManipulationHelperOnParentChange)) {
    // Create the DirectManipulationHelper as soon as hwnd() is set.
    // UpdateParent() will assign an event target to it. Note Direct
    // Manipulation is enabled on Windows 10+. The CreateInstance function
    // returns NULL if Direct Manipulation is not available.
    direct_manipulation_helper_ =
        DirectManipulationHelper::CreateInstance(hwnd());
  }

  return true;
}

// static
ui::WindowEventTarget* LegacyRenderWidgetHostHWND::GetWindowEventTarget(
    HWND parent) {
  return reinterpret_cast<ui::WindowEventTarget*>(ui::ViewProp::GetValue(
      parent, ui::WindowEventTarget::kWin32InputEventTarget));
}

LRESULT LegacyRenderWidgetHostHWND::OnEraseBkGnd(UINT message,
                                                 WPARAM w_param,
                                                 LPARAM l_param) {
  return 1;
}

LRESULT LegacyRenderWidgetHostHWND::OnGetObject(UINT message,
                                                WPARAM w_param,
                                                LPARAM l_param) {
  if (!host_) {
    // Do not service WM_GETOBJECT messages once Destroy() has been called.
    return 0;
  }

  // Casting the signed pointer-sized LPARAM to a signed LONG is well-defined:
  // only the low-order 32-bits are preserved.
  const auto obj_id = static_cast<LONG>(l_param);

  if (kIdScreenReaderHoneyPot == obj_id) {
    // When an MSAA client has responded to fake event for this id,
    // only basic accessibility support is enabled. (Full screen reader support
    // is detected later when specific, more advanced APIs are accessed.)
    ui::AXPlatform::GetInstance().OnScreenReaderHoneyPotQueried();
    return 0;
  }

  // The window will only service accessibility requests after processing a
  // WM_CREATE message and before processing a WM_DESTROY message; see
  // https://learn.microsoft.com/windows/win32/winauto/wm-getobject#remarks.
  if (!may_service_accessibility_requests_) {
    return 0;
  }

  switch (obj_id) {
    case UiaRootObjectId:
      if (ui::AXPlatform::GetInstance().IsUiaProviderEnabled()) {
        // Return the IRawElementProviderSimple for the window's client area to
        // a UI Automation client.
        Microsoft::WRL::ComPtr<IRawElementProviderSimple> root;
        GetOrCreateWindowRootAccessible(/*is_uia_request=*/true)
            ->QueryInterface(IID_PPV_ARGS(&root));

        ui::AXPlatform::GetInstance().SetUiaClientServiced(true);
        return ::UiaReturnRawElementProvider(hwnd(), w_param, l_param,
                                             root.Get());
      }

      // The UIA Provider is not enabled. The client will most likely try again
      // for OBJID_CLIENT.
      break;

    case OBJID_CLIENT:
      // Return the IAccessible for the web content to an MSAA client.
      if (IAccessible* root =
              GetOrCreateWindowRootAccessible(/*is_uia_request=*/false)) {
        return ::LresultFromObject(IID_IAccessible, w_param, root);
      }
      break;

    case OBJID_CARET:
      // Return the IAccessible for the window's caret to an MSAA client.
      if (host_->HasFocus()) {
        DCHECK(ax_system_caret_);
        return ::LresultFromObject(IID_IAccessible, w_param,
                                   ax_system_caret_->GetCaret());
      }
      break;

    default:
      break;
  }

  return 0;
}

// We send keyboard/mouse/touch messages to the parent window via SendMessage.
// While this works, this has the side effect of converting input messages into
// sent messages which changes their priority and could technically result
// in these messages starving other messages in the queue. Additionally
// keyboard/mouse hooks would not see these messages. The alternative approach
// is to set and release capture as needed on the parent to ensure that it
// receives all mouse events. However that was shelved due to possible issues
// with capture changes.
LRESULT LegacyRenderWidgetHostHWND::OnKeyboardRange(UINT message,
                                                    WPARAM w_param,
                                                    LPARAM l_param) {
  auto* event_target = GetWindowEventTarget(GetParent());
  if (!event_target) {
    return 0;
  }

  bool msg_handled = false;
  LRESULT ret = event_target->HandleKeyboardMessage(message, w_param, l_param,
                                                    &msg_handled);
  SetMsgHandled(msg_handled);
  return ret;
}

LRESULT LegacyRenderWidgetHostHWND::OnMouseRange(UINT message,
                                                 WPARAM w_param,
                                                 LPARAM l_param) {
  if (message == WM_MOUSEMOVE) {
    if (!mouse_tracking_enabled_) {
      mouse_tracking_enabled_ = true;
      TRACKMOUSEEVENT tme;
      tme.cbSize = sizeof(tme);
      tme.dwFlags = TME_LEAVE;
      tme.hwndTrack = hwnd();
      tme.dwHoverTime = 0;
      TrackMouseEvent(&tme);
    }
  }
  // The offsets for WM_NCXXX and WM_MOUSEWHEEL and WM_MOUSEHWHEEL messages are
  // in screen coordinates. We should not be converting them to parent
  // coordinates.
  if ((message >= WM_MOUSEFIRST && message <= WM_MOUSELAST) &&
      (message != WM_MOUSEWHEEL && message != WM_MOUSEHWHEEL)) {
    POINT mouse_coords;
    mouse_coords.x = GET_X_LPARAM(l_param);
    mouse_coords.y = GET_Y_LPARAM(l_param);
    ::MapWindowPoints(hwnd(), GetParent(), &mouse_coords, 1);
    l_param = MAKELPARAM(mouse_coords.x, mouse_coords.y);
  }

  auto* event_target = GetWindowEventTarget(GetParent());
  if (!event_target) {
    return 0;
  }

  bool msg_handled = false;
  LRESULT ret =
      event_target->HandleMouseMessage(message, w_param, l_param, &msg_handled);
  SetMsgHandled(msg_handled);
  // If the parent did not handle non-client mouse messages, call
  // DefWindowProc() on the message with the parent window handle. This ensures
  // that WM_SYSCOMMAND is generated for the parent and this class is out of
  // the picture.
  if (!msg_handled &&
      (message >= WM_NCMOUSEMOVE && message <= WM_NCXBUTTONDBLCLK)) {
    ret = ::DefWindowProc(GetParent(), message, w_param, l_param);
    SetMsgHandled(TRUE);
  }
  return ret;
}

LRESULT LegacyRenderWidgetHostHWND::OnMouseLeave(UINT message,
                                                 WPARAM w_param,
                                                 LPARAM l_param) {
  mouse_tracking_enabled_ = false;
  HWND capture_window = ::GetCapture();
  if (capture_window == GetParent()) {
    return 0;
  }

  auto* event_target = GetWindowEventTarget(GetParent());
  if (!event_target) {
    return 0;
  }

  // We should send a WM_MOUSELEAVE to the parent window only if the mouse
  // has moved outside the bounds of the parent.
  POINT cursor_pos;
  ::GetCursorPos(&cursor_pos);

  // WindowFromPoint returns the top-most HWND. As hwnd() may not respond
  // with HTTRANSPARENT to a WM_NCHITTEST message, it may be returned.
  HWND window_from_point = ::WindowFromPoint(cursor_pos);
  if (window_from_point == GetParent()) {
    return 0;
  }

  if (!capture_window && window_from_point == hwnd()) {
    return 0;
  }

  bool msg_handled = false;
  LRESULT ret =
      event_target->HandleMouseMessage(message, w_param, l_param, &msg_handled);
  SetMsgHandled(msg_handled);
  return ret;
}

LRESULT LegacyRenderWidgetHostHWND::OnMouseActivate(UINT message,
                                                    WPARAM w_param,
                                                    LPARAM l_param) {
  // Don't pass this to DefWindowProc. That results in the WM_MOUSEACTIVATE
  // message going all the way to the parent which then messes up state
  // related to focused views, etc. This is because it treats this as if
  // it lost activation.
  // Our dummy window should not interfere with focus and activation in
  // the parent. Return MA_ACTIVATE here ensures that focus state in the parent
  // is preserved. The only exception is if the parent was created with the
  // WS_EX_NOACTIVATE style.
  if (::GetWindowLong(GetParent(), GWL_EXSTYLE) & WS_EX_NOACTIVATE) {
    return MA_NOACTIVATE;
  }
  // On Windows, if we select the menu item by touch and if the window at the
  // location is another window on the same thread, that window gets a
  // WM_MOUSEACTIVATE message and ends up activating itself, which is not
  // correct. We workaround this by setting a property on the window at the
  // current cursor location. We check for this property in our
  // WM_MOUSEACTIVATE handler and don't activate the window if the property is
  // set.
  if (::GetProp(hwnd(), ui::kIgnoreTouchMouseActivateForWindow)) {
    ::RemoveProp(hwnd(), ui::kIgnoreTouchMouseActivateForWindow);
    return MA_NOACTIVATE;
  }
  return MA_ACTIVATE;
}

LRESULT LegacyRenderWidgetHostHWND::OnPointer(UINT message,
                                              WPARAM w_param,
                                              LPARAM l_param) {
  // When this window is occluded, it is reparented to the global hidden window
  // parent by RWHVA::HideImpl. This means any WM_POINTER* messages received
  // while hidden will be ignored because the global hidden window has no
  // WindowEventTarget. So if this window is hidden during an ongoing touch
  // gesture and that gesture ends while hidden, any WM_POINTERUPs will be
  // ignored.
  // When this window is shown again, the web page that had been handling the
  // pointer event sequence(s) will end up unresponsive to touch because it is
  // stuck waiting for pointer up event(s) that never come.
  // To prevent this, we track the down pointers and the parent before hide.
  // We ensure the parent before hide handles any ongoing pointer events while
  // hidden.
  const uint32_t pointer_id = GET_POINTERID_WPARAM(w_param);
  const HWND parent =
      (parent_before_hide_ && down_pointers_before_hide_.contains(pointer_id))
          ? parent_before_hide_
          : GetParent();

  auto* event_target = GetWindowEventTarget(parent);
  if (!event_target) {
    return 0;
  }

  bool msg_handled = false;
  LRESULT ret = event_target->HandlePointerMessage(message, w_param, l_param,
                                                   &msg_handled);
  SetMsgHandled(msg_handled);

  if (message == WM_POINTERDOWN) {
    // We should never be adding to the down pointers set if we are hidden.
    CHECK(!parent_before_hide_);
    down_pointers_before_hide_.insert(pointer_id);
  } else if (message == WM_POINTERUP) {
    down_pointers_before_hide_.erase(pointer_id);
  }

  return ret;
}

LRESULT LegacyRenderWidgetHostHWND::OnTouch(UINT message,
                                            WPARAM w_param,
                                            LPARAM l_param) {
  auto* event_target = GetWindowEventTarget(GetParent());
  if (!event_target) {
    return 0;
  }

  bool msg_handled = false;
  LRESULT ret =
      event_target->HandleTouchMessage(message, w_param, l_param, &msg_handled);
  SetMsgHandled(msg_handled);
  return ret;
}

LRESULT LegacyRenderWidgetHostHWND::OnInput(UINT message,
                                            WPARAM w_param,
                                            LPARAM l_param) {
  auto* event_target = GetWindowEventTarget(GetParent());
  if (!event_target) {
    return 0;
  }

  bool msg_handled = false;
  LRESULT ret =
      event_target->HandleInputMessage(message, w_param, l_param, &msg_handled);
  SetMsgHandled(msg_handled);
  return ret;
}

LRESULT LegacyRenderWidgetHostHWND::OnScroll(UINT message,
                                             WPARAM w_param,
                                             LPARAM l_param) {
  auto* event_target = GetWindowEventTarget(GetParent());
  if (!event_target) {
    return 0;
  }

  bool msg_handled = false;
  LRESULT ret = event_target->HandleScrollMessage(message, w_param, l_param,
                                                  &msg_handled);
  SetMsgHandled(msg_handled);
  return ret;
}

LRESULT LegacyRenderWidgetHostHWND::OnNCHitTest(UINT message,
                                                WPARAM w_param,
                                                LPARAM l_param) {
  auto* event_target = GetWindowEventTarget(GetParent());
  if (!event_target) {
    return HTNOWHERE;
  }

  bool msg_handled = false;
  LRESULT hit_test = event_target->HandleNcHitTestMessage(
      message, w_param, l_param, &msg_handled);
  if (hit_test == HTNOWHERE) {
    // If the parent returns HTNOWHERE which can happen for popup windows, etc,
    // return HTCLIENT.
    return HTCLIENT;
  }
  return hit_test;
}

LRESULT LegacyRenderWidgetHostHWND::OnNCPaint(UINT message,
                                              WPARAM w_param,
                                              LPARAM l_param) {
  return 0;
}

LRESULT LegacyRenderWidgetHostHWND::OnPaint(UINT message,
                                            WPARAM w_param,
                                            LPARAM l_param) {
  PAINTSTRUCT ps = {0};
  ::BeginPaint(hwnd(), &ps);
  ::EndPaint(hwnd(), &ps);
  return 0;
}

LRESULT LegacyRenderWidgetHostHWND::OnSetCursor(UINT message,
                                                WPARAM w_param,
                                                LPARAM l_param) {
  return 0;
}

LRESULT LegacyRenderWidgetHostHWND::OnNCCalcSize(UINT message,
                                                 WPARAM w_param,
                                                 LPARAM l_param) {
  // Prevent scrollbars, etc from drawing.
  return 0;
}

LRESULT LegacyRenderWidgetHostHWND::OnSize(UINT message,
                                           WPARAM w_param,
                                           LPARAM l_param) {
  // Certain trackpad drivers on Windows have bugs where in they don't generate
  // WM_MOUSEWHEEL messages for the trackpoint and trackpad scrolling gestures
  // unless there is an entry for Chrome with the class name of the Window.
  // Additionally others check if the window WS_VSCROLL/WS_HSCROLL styles and
  // generate the legacy WM_VSCROLL/WM_HSCROLL messages.
  // We add these styles to ensure that trackpad/trackpoint scrolling
  // work.
  long current_style = ::GetWindowLong(hwnd(), GWL_STYLE);
  ::SetWindowLong(hwnd(), GWL_STYLE, current_style | WS_VSCROLL | WS_HSCROLL);
  return 0;
}

LRESULT LegacyRenderWidgetHostHWND::OnCreate(UINT message,
                                             WPARAM w_param,
                                             LPARAM l_param) {
  // The window may begin responding to WM_GETOBJECT messages from this point
  // until WM_DESTROY is received; see
  // https://learn.microsoft.com/windows/win32/winauto/wm-getobject#remarks.
  may_service_accessibility_requests_ = true;

  return 0;
}

LRESULT LegacyRenderWidgetHostHWND::OnDestroy(UINT message,
                                              WPARAM w_param,
                                              LPARAM l_param) {
  // The window will no longer service WM_GETOBJECT messages from this point
  // onward; see
  // https://learn.microsoft.com/windows/win32/winauto/wm-getobject#remarks.
  may_service_accessibility_requests_ = false;

  if (auto& ax_platform = ui::AXPlatform::GetInstance();
      ax_platform.HasServicedUiaClients()) {
    // Clean up UIA resources associated with this window's fragment root if all
    // providers have not previously been disconnected; see
    // https://learn.microsoft.com/en-us/windows/win32/api/uiautomationcoreapi/nf-uiautomationcoreapi-uiadisconnectprovider.
    if (ax_platform.IsUiaProviderEnabled() &&
        base::FeatureList::IsEnabled(features::kUiaDisconnectRootProviders)) {
      ::UiaDisconnectProvider(ax_fragment_root_->GetProvider());
    }

    // Disassociate this window from MSAA clients that are observing events; see
    // https://docs.microsoft.com/en-us/windows/win32/api/uiautomationcoreapi/nf-uiautomationcoreapi-uiareturnrawelementprovider#remarks
    ::UiaReturnRawElementProvider(hwnd(), 0, 0, nullptr);
  }

  return 0;
}

LRESULT LegacyRenderWidgetHostHWND::OnPointerHitTest(UINT message,
                                                     WPARAM w_param,
                                                     LPARAM l_param) {
  if (direct_manipulation_helper_) {
    direct_manipulation_helper_->OnPointerHitTest(w_param);
  }

  return 0;
}

gfx::NativeViewAccessible
LegacyRenderWidgetHostHWND::GetChildOfAXFragmentRoot() {
  return GetOrCreateBrowserAccessibilityRoot();
}

gfx::NativeViewAccessible
LegacyRenderWidgetHostHWND::GetParentOfAXFragmentRoot() {
  return host_ ? host_->GetParentNativeViewAccessible() : nullptr;
}

bool LegacyRenderWidgetHostHWND::IsAXFragmentRootAControlElement() {
  // Treat LegacyRenderWidgetHostHWND as a non-control element so that clients
  // don't read out "Chrome Legacy Window" for it.
  return false;
}

gfx::NativeViewAccessible
LegacyRenderWidgetHostHWND::GetOrCreateWindowRootAccessible(
    bool is_uia_request) {
  if (is_uia_request) {
    DCHECK(::ui::AXPlatform::GetInstance().IsUiaProviderEnabled());
    return ax_fragment_root_->GetNativeViewAccessible();
  }
  return GetOrCreateBrowserAccessibilityRoot();
}

gfx::NativeViewAccessible
LegacyRenderWidgetHostHWND::GetOrCreateBrowserAccessibilityRoot() {
  if (!host_) {
    return nullptr;
  }

  RenderWidgetHostImpl* rwhi =
      RenderWidgetHostImpl::From(host_->GetRenderWidgetHost());
  if (!rwhi) {
    return nullptr;
  }

  auto* manager = static_cast<ui::BrowserAccessibilityManagerWin*>(
      rwhi->GetOrCreateRootBrowserAccessibilityManager());
  if (!manager || !manager->GetBrowserAccessibilityRoot()) {
    return nullptr;
  }

  ui::BrowserAccessibility* root_node = manager->GetBrowserAccessibilityRoot();

  // Popups with HTML content (such as <input type="date">) will create a new
  // HWND with its own fragment root, but will also inject accessible nodes into
  // the main document's accessibility tree, thus sharing a
  // BrowserAccessibilityManager with the main document (see documentation for
  // BrowserAccessibilityManager::child_root_id_). We can't return the same root
  // node as the main document, as that will cause a cardinality problem - there
  // would be two different HWND's pointing to the same root. The popup HWND
  // should return the root of the popup, not the root of the main document
  if (host_->GetWidgetType() == WidgetType::kPopup) {
    // Check to see if the manager has a child root (it's expected that there
    // won't be in popups without HTML-based content such as <select> controls).
    ui::BrowserAccessibility* child_root = manager->GetPopupRoot();
    if (child_root) {
      return child_root->GetNativeViewAccessible();
    }
  }

  return root_node->GetNativeViewAccessible();
}

}  // namespace content
