// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_LEGACY_RENDER_WIDGET_HOST_WIN_H_
#define CONTENT_BROWSER_RENDERER_HOST_LEGACY_RENDER_WIDGET_HOST_WIN_H_

// clang-format off
// This needs to be included before ATL headers.
#include "base/win/atl.h"
// clang-format on

#include <atlapp.h>
#include <atlcrack.h>
#include <oleacc.h>
#include <wrl/client.h>

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "ui/accessibility/platform/ax_fragment_root_delegate_win.h"
#include "ui/base/win/internal_constants.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {
class AXFragmentRootWin;
class AXSystemCaretWin;
class ViewProp;
class WindowEventTarget;
}  // namespace ui

namespace content {

class DirectManipulationBrowserTestBase;
class DirectManipulationHelper;
class RenderWidgetHostViewAura;

// Reasons for the existence of this class outlined below:
// 1. Some screen readers expect every tab / every unique web content container
//    to be in its own HWND with class name Chrome_RenderWidgetHostHWND.
//    With Aura there is one main HWND which comprises the whole browser window
//    or the whole desktop. So, we need a fake HWND with the window class as
//    Chrome_RenderWidgetHostHWND as the root of the accessibility tree for
//    each tab.
// 2. Some legacy trackpad/trackpoint drivers have special code for sending
//    mouse wheel and scroll events to the Chrome_RenderWidgetHostHWND window.
// We should attempt to remove this code when the above are fixed.

// This class implements a child HWND with the same size as the content area,
// that delegates its accessibility implementation to the root of the
// BrowserAccessibilityManager tree. This HWND is hooked up as the parent of
// the root object in the BrowserAccessibilityManager tree, so when any
// accessibility client calls ::WindowFromAccessibleObject, they get this
// HWND instead of the DesktopWindowTreeHostWin. It also maintains a ViewProp to
// associate the parent's aura::WindowTreeHost with this HWND for lookup.
class CONTENT_EXPORT LegacyRenderWidgetHostHWND
    : public ATL::CWindowImpl<LegacyRenderWidgetHostHWND,
                              ATL::CWindow,
                              ATL::CWinTraits<WS_CHILD>>,
      public ui::AXFragmentRootDelegateWin {
 public:
  DECLARE_WND_CLASS_EX(ui::kLegacyRenderWidgetHostHwnd, CS_DBLCLKS, 0)

  typedef ATL::CWindowImpl<LegacyRenderWidgetHostHWND,
                           ATL::CWindow,
                           ATL::CWinTraits<WS_CHILD>>
      Base;

  // Creates and returns an instance of the LegacyRenderWidgetHostHWND class on
  // successful creation of a child window parented to the parent window passed
  // in.
  static LegacyRenderWidgetHostHWND* Create(HWND parent,
                                            RenderWidgetHostViewAura* host);

  LegacyRenderWidgetHostHWND(const LegacyRenderWidgetHostHWND&) = delete;
  LegacyRenderWidgetHostHWND& operator=(const LegacyRenderWidgetHostHWND&) =
      delete;

  // Destroys the HWND managed by this class.
  void Destroy();

  BEGIN_MSG_MAP_EX(LegacyRenderWidgetHostHWND)
    MESSAGE_HANDLER_EX(WM_GETOBJECT, OnGetObject)
    MESSAGE_RANGE_HANDLER(WM_KEYFIRST, WM_KEYLAST, OnKeyboardRange)
    MESSAGE_HANDLER_EX(WM_PAINT, OnPaint)
    MESSAGE_HANDLER_EX(WM_NCPAINT, OnNCPaint)
    MESSAGE_HANDLER_EX(WM_ERASEBKGND, OnEraseBkGnd)
    MESSAGE_HANDLER_EX(WM_INPUT, OnInput)
    MESSAGE_RANGE_HANDLER(WM_MOUSEFIRST, WM_MOUSELAST, OnMouseRange)
    MESSAGE_HANDLER_EX(WM_MOUSELEAVE, OnMouseLeave)
    MESSAGE_HANDLER_EX(WM_MOUSEACTIVATE, OnMouseActivate)
    MESSAGE_HANDLER_EX(WM_SETCURSOR, OnSetCursor)
    MESSAGE_HANDLER_EX(WM_TOUCH, OnTouch)
    MESSAGE_HANDLER_EX(WM_POINTERDOWN, OnPointer)
    MESSAGE_HANDLER_EX(WM_POINTERUPDATE, OnPointer)
    MESSAGE_HANDLER_EX(WM_POINTERUP, OnPointer)
    MESSAGE_HANDLER_EX(WM_POINTERENTER, OnPointer)
    MESSAGE_HANDLER_EX(WM_POINTERLEAVE, OnPointer)
    MESSAGE_HANDLER_EX(WM_HSCROLL, OnScroll)
    MESSAGE_HANDLER_EX(WM_VSCROLL, OnScroll)
    MESSAGE_HANDLER_EX(WM_NCHITTEST, OnNCHitTest)
    MESSAGE_RANGE_HANDLER(WM_NCMOUSEMOVE, WM_NCXBUTTONDBLCLK, OnMouseRange)
    MESSAGE_HANDLER_EX(WM_NCCALCSIZE, OnNCCalcSize)
    MESSAGE_HANDLER_EX(WM_SIZE, OnSize)
    MESSAGE_HANDLER_EX(WM_DESTROY, OnDestroy)
    MESSAGE_HANDLER_EX(DM_POINTERHITTEST, OnPointerHitTest)
  END_MSG_MAP()

  HWND hwnd() { return m_hWnd; }

  // Called when the child window is to be reparented to a new window.
  // The |parent| parameter contains the new parent window.
  void UpdateParent(HWND parent);
  HWND GetParent();

  IAccessible* window_accessible() { return window_accessible_.Get(); }

  // Functions to show and hide the window.
  void Show();
  void Hide();

  // Resizes the window to the bounds passed in.
  void SetBounds(const gfx::Rect& bounds);

  // Return the root accessible object for either MSAA or UI Automation.
  gfx::NativeViewAccessible GetOrCreateWindowRootAccessible(
      bool is_uia_request);

 protected:
  void OnFinalMessage(HWND hwnd) override;

 private:
  friend class AccessibilityObjectLifetimeWinBrowserTest;
  friend class DirectManipulationBrowserTestBase;

  LegacyRenderWidgetHostHWND(RenderWidgetHostViewAura* host);
  ~LegacyRenderWidgetHostHWND() override;

  // If initialization fails, deletes `this` and returns false.
  bool InitOrDeleteSelf(HWND parent);

  // Returns the target to which the windows input events are forwarded.
  static ui::WindowEventTarget* GetWindowEventTarget(HWND parent);

  LRESULT OnEraseBkGnd(UINT message, WPARAM w_param, LPARAM l_param);
  LRESULT OnGetObject(UINT message, WPARAM w_param, LPARAM l_param);
  LRESULT OnInput(UINT message, WPARAM w_param, LPARAM l_param);
  LRESULT OnKeyboardRange(UINT message,
                          WPARAM w_param,
                          LPARAM l_param,
                          BOOL& handled);
  LRESULT OnMouseLeave(UINT message, WPARAM w_param, LPARAM l_param);
  LRESULT OnMouseRange(UINT message,
                       WPARAM w_param,
                       LPARAM l_param,
                       BOOL& handled);
  LRESULT OnMouseActivate(UINT message, WPARAM w_param, LPARAM l_param);
  LRESULT OnPointer(UINT message, WPARAM w_param, LPARAM l_param);
  LRESULT OnTouch(UINT message, WPARAM w_param, LPARAM l_param);
  LRESULT OnScroll(UINT message, WPARAM w_param, LPARAM l_param);
  LRESULT OnNCHitTest(UINT message, WPARAM w_param, LPARAM l_param);

  LRESULT OnNCPaint(UINT message, WPARAM w_param, LPARAM l_param);
  LRESULT OnPaint(UINT message, WPARAM w_param, LPARAM l_param);
  LRESULT OnSetCursor(UINT message, WPARAM w_param, LPARAM l_param);
  LRESULT OnNCCalcSize(UINT message, WPARAM w_param, LPARAM l_param);
  LRESULT OnSize(UINT message, WPARAM w_param, LPARAM l_param);
  LRESULT OnDestroy(UINT message, WPARAM w_param, LPARAM l_param);

  LRESULT OnPointerHitTest(UINT message, WPARAM w_param, LPARAM l_param);

  // Overridden from AXFragmentRootDelegateWin.
  gfx::NativeViewAccessible GetChildOfAXFragmentRoot() override;
  gfx::NativeViewAccessible GetParentOfAXFragmentRoot() override;
  bool IsAXFragmentRootAControlElement() override;

  gfx::NativeViewAccessible GetOrCreateBrowserAccessibilityRoot();
  void CreateDirectManipulationHelper();

  Microsoft::WRL::ComPtr<IAccessible> window_accessible_;

  // Set to true if we turned on mouse tracking.
  bool mouse_tracking_enabled_ = false;

  raw_ptr<RenderWidgetHostViewAura> host_;

  // Some assistive software need to track the location of the caret.
  std::unique_ptr<ui::AXSystemCaretWin> ax_system_caret_;

  // Implements IRawElementProviderFragmentRoot when UIA is enabled.
  std::unique_ptr<ui::AXFragmentRootWin> ax_fragment_root_;

  // Set to true when we return a UIA object. Determines whether we need to
  // call UIA to clean up object references on window destruction.
  // This is important to avoid triggering a cross-thread COM call which could
  // cause re-entrancy during teardown. https://crbug.com/1087553
  bool did_return_uia_object_ = false;

  // This class provides functionality to register the legacy window as a
  // Direct Manipulation consumer. This allows us to support smooth scroll
  // in Chrome on Windows 10.
  std::unique_ptr<DirectManipulationHelper> direct_manipulation_helper_;

  // Instruct aura::WindowTreeHost to use the HWND's parent for lookup.
  std::unique_ptr<ui::ViewProp> window_tree_host_prop_;

  base::WeakPtrFactory<LegacyRenderWidgetHostHWND> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_LEGACY_RENDER_WIDGET_HOST_WIN_H_
