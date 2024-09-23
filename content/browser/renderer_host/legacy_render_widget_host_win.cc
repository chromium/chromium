// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/legacy_render_widget_host_win.h"

#include <objbase.h>

#include <memory>
#include <utility>

#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/win/win_util.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "content/browser/renderer_host/direct_manipulation_helper_win.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
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
#include "ui/base/win/internal_constants.h"
#include "ui/base/win/window_event_target.h"
#include "ui/display/win/screen_win.h"
#include "ui/gfx/geometry/rect.h"

namespace content {

// A custom MSAA object id used to determine if a screen reader or some
// other client is listening on MSAA events - if so, we enable full web
// accessibility support.
const int kIdScreenReaderHoneyPot = 1;

// static
LegacyRenderWidgetHostHWND* LegacyRenderWidgetHostHWND::Create(
    HWND parent,
    RenderWidgetHostViewAura* host) {
  // content_unittests passes in the desktop window as the parent. We allow
  // the LegacyRenderWidgetHostHWND instance to be created in this case for
  // these tests to pass.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableLegacyIntermediateWindow) ||
      (!GetWindowEventTarget(parent) && parent != ::GetDesktopWindow()))
    return nullptr;

  LegacyRenderWidgetHostHWND* legacy_window_instance =
      new LegacyRenderWidgetHostHWND(host);
  if (!legacy_window_instance->InitOrDeleteSelf(parent))
    return nullptr;

  return legacy_window_instance;
}

void LegacyRenderWidgetHostHWND::Destroy() {
  // Delete DirectManipulationHelper before the window is destroyed.
  direct_manipulation_helper_.reset();
  window_tree_host_prop_.reset();
  host_ = nullptr;
  if (::IsWindow(hwnd()))
    ::DestroyWindow(hwnd());
}

void LegacyRenderWidgetHostHWND::CreateDirectManipulationHelper() {
  // Direct Manipulation is enabled on Windows 10+. The CreateInstance function
  // returns NULL if Direct Manipulation is not available. Recreate
  // |direct_manipulation_helper_| when parent changed (compositor and window
  // event target updated).
  direct_manipulation_helper_ = DirectManipulationHelper::CreateInstance(
      hwnd(), host_->GetNativeView()->GetHost()->compositor(),
      GetWindowEventTarget(GetParent()));
}

void LegacyRenderWidgetHostHWND::UpdateParent(HWND new_parent) {
  // Performance profiles for resizing show that roughly 1/3 of the
  // browser main thread CPU samples are inside of the ::SetParent call, even
  // though the parent is never changed during this operation. The CPU samples
  // disappear if we ask the OS for the current parent and avoid the SetParent
  // call altogether.
  const HWND current_parent = GetParent();
  if (current_parent != new_parent) {
    if (GetWindowEventTarget(GetParent())) {
      GetWindowEventTarget(GetParent())->HandleParentChanged();
    }

    ::SetParent(hwnd(), new_parent);

    CreateDirectManipulationHelper();

    // Reset tooltips when parent changed; otherwise tooltips could stay open as
    // the former parent wouldn't be forwarded any mouse leave messages.
    host_->UpdateTooltip(std::u16string());
  } else {
    // The first call to UpdateParent may have the parent correctly set on
    // account of InitOrDeleteSelf having just created the correctly parented
    // Window. We will need to create the DirectManipulationHelper in this case
    // if we haven't already done so. After initial creation, the
    // DirectManipulationHelper only needs to be re-created if the parent
    // subsequently changes.
    if (!direct_manipulation_helper_) {
      CreateDirectManipulationHelper();
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
  gfx::Rect bounds_in_pixel = display::win::ScreenWin::DIPToClientRect(hwnd(),
                                                                       bounds);
  ::SetWindowPos(hwnd(), NULL, bounds_in_pixel.x(), bounds_in_pixel.y(),
                 bounds_in_pixel.width(), bounds_in_pixel.height(),
                 SWP_NOREDRAW);
  if (direct_manipulation_helper_)
    direct_manipulation_helper_->SetSizeInPixels(bounds_in_pixel.size());
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
  auto weak_ptr = weak_factory_.GetWeakPtr();
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

  // Only register a touch window if we are using WM_TOUCH.
  if (!features::IsUsingWMPointerForTouch())
    RegisterTouchWindow(hwnd(), TWF_WANTPALM);

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
  // Only the lower 32 bits of l_param are valid when checking the object id
  // because it sometimes gets sign-extended incorrectly (but not always).
  DWORD obj_id = static_cast<DWORD>(static_cast<DWORD_PTR>(l_param));

  if (kIdScreenReaderHoneyPot == obj_id) {
    // When an MSAA client has responded to fake event for this id,
    // only basic accessibility support is enabled. (Full screen reader support
    // is detected later when specific, more advanced APIs are accessed.)
    for (ui::WinAccessibilityAPIUsageObserver& observer :
         ui::GetWinAccessibilityAPIUsageObserverList()) {
      observer.OnScreenReaderHoneyPotQueried();
    }
    return static_cast<LRESULT>(0L);
  }

  if (!host_)
    return static_cast<LRESULT>(0L);

  const bool is_uia_request = static_cast<DWORD>(UiaRootObjectId) == obj_id;
  const bool is_uia_active =
      is_uia_request && ::ui::AXPlatform::GetInstance().IsUiaProviderEnabled();
  const bool is_msaa_request = static_cast<DWORD>(OBJID_CLIENT) == obj_id;

  if (is_uia_request) {
    CHECK_DEREF(CHECK_DEREF(GetContentClient()).browser())
        .OnUiaProviderRequested(is_uia_active);
  }

  if (is_uia_active || is_msaa_request) {
    gfx::NativeViewAccessible root =
        GetOrCreateWindowRootAccessible(is_uia_request);

    if (is_uia_active) {
      Microsoft::WRL::ComPtr<IRawElementProviderSimple> root_uia;
      root->QueryInterface(IID_PPV_ARGS(&root_uia));

      // Return the UIA object via UiaReturnRawElementProvider(). See:
      // https://docs.microsoft.com/en-us/windows/win32/winauto/wm-getobject
      did_return_uia_object_ = true;
      return UiaReturnRawElementProvider(hwnd(), w_param, l_param,
                                         root_uia.Get());
    } else {
      if (root == nullptr)
        return static_cast<LRESULT>(0L);

      Microsoft::WRL::ComPtr<IAccessible> root_msaa(root);
      return LresultFromObject(IID_IAccessible, w_param, root_msaa.Get());
    }
  }

  if (static_cast<DWORD>(OBJID_CARET) == obj_id && host_->HasFocus()) {
    DCHECK(ax_system_caret_);
    Microsoft::WRL::ComPtr<IAccessible> ax_system_caret_accessible =
        ax_system_caret_->GetCaret();
    return LresultFromObject(IID_IAccessible, w_param,
                             ax_system_caret_accessible.Get());
  }

  return static_cast<LRESULT>(0L);
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
                                                    LPARAM l_param,
                                                    BOOL& handled) {
  LRESULT ret = 0;
  if (GetWindowEventTarget(GetParent())) {
    bool msg_handled = false;
    ret = GetWindowEventTarget(GetParent())->HandleKeyboardMessage(
        message, w_param, l_param, &msg_handled);
    handled = msg_handled;
  }
  return ret;
}

LRESULT LegacyRenderWidgetHostHWND::OnMouseRange(UINT message,
                                                 WPARAM w_param,
                                                 LPARAM l_param,
                                                 BOOL& handled) {
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

  LRESULT ret = 0;

  if (GetWindowEventTarget(GetParent())) {
    bool msg_handled = false;
    ret = GetWindowEventTarget(GetParent())->HandleMouseMessage(
        message, w_param, l_param, &msg_handled);
    handled = msg_handled;
    // If the parent did not handle non client mouse messages, we call
    // DefWindowProc on the message with the parent window handle. This
    // ensures that WM_SYSCOMMAND is generated for the parent and we are
    // out of the picture.
    if (!handled &&
        (message >= WM_NCMOUSEMOVE && message <= WM_NCXBUTTONDBLCLK)) {
      ret = ::DefWindowProc(GetParent(), message, w_param, l_param);
      handled = TRUE;
    }
  }
  return ret;
}

LRESULT LegacyRenderWidgetHostHWND::OnMouseLeave(UINT message,
                                                 WPARAM w_param,
                                                 LPARAM l_param) {
  mouse_tracking_enabled_ = false;
  LRESULT ret = 0;
  HWND capture_window = ::GetCapture();
  if ((capture_window != GetParent()) && GetWindowEventTarget(GetParent())) {
    // We should send a WM_MOUSELEAVE to the parent window only if the mouse
    // has moved outside the bounds of the parent.
    POINT cursor_pos;
    ::GetCursorPos(&cursor_pos);

    // WindowFromPoint returns the top-most HWND. As hwnd() may not
    // respond with HTTRANSPARENT to a WM_NCHITTEST message,
    // it may be returned.
    HWND window_from_point = ::WindowFromPoint(cursor_pos);
    if (window_from_point != GetParent() &&
        (capture_window || window_from_point != hwnd())) {
      bool msg_handled = false;
      ret = GetWindowEventTarget(GetParent())->HandleMouseMessage(
          message, w_param, l_param, &msg_handled);
      SetMsgHandled(msg_handled);
    }
  }
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
  if (::GetWindowLong(GetParent(), GWL_EXSTYLE) & WS_EX_NOACTIVATE)
    return MA_NOACTIVATE;
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
  LRESULT ret = 0;
  if (GetWindowEventTarget(GetParent())) {
    bool msg_handled = false;
    ret = GetWindowEventTarget(GetParent())
              ->HandlePointerMessage(message, w_param, l_param, &msg_handled);
    SetMsgHandled(msg_handled);
  }
  return ret;
}

LRESULT LegacyRenderWidgetHostHWND::OnTouch(UINT message,
                                            WPARAM w_param,
                                            LPARAM l_param) {
  LRESULT ret = 0;
  if (GetWindowEventTarget(GetParent())) {
    bool msg_handled = false;
    ret = GetWindowEventTarget(GetParent())->HandleTouchMessage(
        message, w_param, l_param, &msg_handled);
    SetMsgHandled(msg_handled);
  }
  return ret;
}

LRESULT LegacyRenderWidgetHostHWND::OnInput(UINT message,
                                            WPARAM w_param,
                                            LPARAM l_param) {
  LRESULT ret = 0;
  if (GetWindowEventTarget(GetParent())) {
    bool msg_handled = false;
    ret = GetWindowEventTarget(GetParent())
              ->HandleInputMessage(message, w_param, l_param, &msg_handled);
    SetMsgHandled(msg_handled);
  }
  return ret;
}

LRESULT LegacyRenderWidgetHostHWND::OnScroll(UINT message,
                                             WPARAM w_param,
                                             LPARAM l_param) {
  LRESULT ret = 0;
  if (GetWindowEventTarget(GetParent())) {
    bool msg_handled = false;
    ret = GetWindowEventTarget(GetParent())->HandleScrollMessage(
        message, w_param, l_param, &msg_handled);
    SetMsgHandled(msg_handled);
  }
  return ret;
}

LRESULT LegacyRenderWidgetHostHWND::OnNCHitTest(UINT message,
                                                WPARAM w_param,
                                                LPARAM l_param) {
  if (GetWindowEventTarget(GetParent())) {
    bool msg_handled = false;
    LRESULT hit_test = GetWindowEventTarget(
        GetParent())->HandleNcHitTestMessage(message, w_param, l_param,
                                             &msg_handled);
    // If the parent returns HTNOWHERE which can happen for popup windows, etc
    // we return HTCLIENT.
    if (hit_test == HTNOWHERE)
      hit_test = HTCLIENT;
    return hit_test;
  }
  return HTNOWHERE;
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
  ::SetWindowLong(hwnd(), GWL_STYLE,
                  current_style | WS_VSCROLL | WS_HSCROLL);
  return 0;
}

LRESULT LegacyRenderWidgetHostHWND::OnDestroy(UINT message,
                                              WPARAM w_param,
                                              LPARAM l_param) {
  // If we have ever returned a UIA object via WM_GETOBJECT, signal that all
  // objects associated with this HWND can be discarded. See:
  // https://docs.microsoft.com/en-us/windows/win32/api/uiautomationcoreapi/nf-uiautomationcoreapi-uiareturnrawelementprovider#remarks
  if (did_return_uia_object_)
    UiaReturnRawElementProvider(hwnd(), 0, 0, nullptr);

  return 0;
}

LRESULT LegacyRenderWidgetHostHWND::OnPointerHitTest(UINT message,
                                                     WPARAM w_param,
                                                     LPARAM l_param) {
  if (!direct_manipulation_helper_)
    return 0;

  direct_manipulation_helper_->OnPointerHitTest(w_param);

  return 0;
}

gfx::NativeViewAccessible
LegacyRenderWidgetHostHWND::GetChildOfAXFragmentRoot() {
  return GetOrCreateBrowserAccessibilityRoot();
}

gfx::NativeViewAccessible
LegacyRenderWidgetHostHWND::GetParentOfAXFragmentRoot() {
  if (host_)
    return host_->GetParentNativeViewAccessible();
  return nullptr;
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
  if (!host_)
    return nullptr;

  RenderWidgetHostImpl* rwhi =
      RenderWidgetHostImpl::From(host_->GetRenderWidgetHost());
  if (!rwhi)
    return nullptr;

  ui::BrowserAccessibilityManagerWin* manager =
      static_cast<ui::BrowserAccessibilityManagerWin*>(
          rwhi->GetOrCreateRootBrowserAccessibilityManager());
  if (!manager || !manager->GetBrowserAccessibilityRoot())
    return nullptr;

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
    if (child_root)
      return child_root->GetNativeViewAccessible();
  }

  return root_node->GetNativeViewAccessible();
}

}  // namespace content
