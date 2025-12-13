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
#include <oleacc.h>
#include <wrl/client.h>

#include <memory>
#include <set>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "content/common/content_export.h"
#include "ui/accessibility/platform/ax_fragment_root_delegate_win.h"
#include "ui/base/win/internal_constants.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/gfx/win/msg_util.h"

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
FORWARD_DECLARE_TEST(RenderWidgetHostViewAuraTest,
                     LegacyRenderWidgetHostHWNDPointerEventsWhileHidden);

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
  // in. May return nullptr on failure.
  // Owner must call Destroy() to destroy the returned object, rather than
  // directly deleting it.
  static LegacyRenderWidgetHostHWND* Create(HWND parent,
                                            RenderWidgetHostViewAura* host);

  LegacyRenderWidgetHostHWND(const LegacyRenderWidgetHostHWND&) = delete;
  LegacyRenderWidgetHostHWND& operator=(const LegacyRenderWidgetHostHWND&) =
      delete;

  // Destroys the HWND managed by this class. The class will then delete itself.
  void Destroy();

  CR_BEGIN_MSG_MAP_EX(LegacyRenderWidgetHostHWND)
    CR_MESSAGE_HANDLER_EX(WM_GETOBJECT, OnGetObject)
    CR_MESSAGE_RANGE_HANDLER_EX(WM_KEYFIRST, WM_KEYLAST, OnKeyboardRange)
    CR_MESSAGE_HANDLER_EX(WM_PAINT, OnPaint)
    CR_MESSAGE_HANDLER_EX(WM_NCPAINT, OnNCPaint)
    CR_MESSAGE_HANDLER_EX(WM_ERASEBKGND, OnEraseBkGnd)
    CR_MESSAGE_HANDLER_EX(WM_INPUT, OnInput)
    CR_MESSAGE_RANGE_HANDLER_EX(WM_MOUSEFIRST, WM_MOUSELAST, OnMouseRange)
    CR_MESSAGE_HANDLER_EX(WM_MOUSELEAVE, OnMouseLeave)
    CR_MESSAGE_HANDLER_EX(WM_MOUSEACTIVATE, OnMouseActivate)
    CR_MESSAGE_HANDLER_EX(WM_SETCURSOR, OnSetCursor)
    CR_MESSAGE_HANDLER_EX(WM_TOUCH, OnTouch)
    CR_MESSAGE_HANDLER_EX(WM_POINTERDOWN, OnPointer)
    CR_MESSAGE_HANDLER_EX(WM_POINTERUPDATE, OnPointer)
    CR_MESSAGE_HANDLER_EX(WM_POINTERUP, OnPointer)
    CR_MESSAGE_HANDLER_EX(WM_POINTERENTER, OnPointer)
    CR_MESSAGE_HANDLER_EX(WM_POINTERLEAVE, OnPointer)
    CR_MESSAGE_HANDLER_EX(WM_HSCROLL, OnScroll)
    CR_MESSAGE_HANDLER_EX(WM_VSCROLL, OnScroll)
    CR_MESSAGE_HANDLER_EX(WM_NCHITTEST, OnNCHitTest)
    CR_MESSAGE_RANGE_HANDLER_EX(WM_NCMOUSEMOVE, WM_NCXBUTTONDBLCLK,
                                OnMouseRange)
    CR_MESSAGE_HANDLER_EX(WM_NCCALCSIZE, OnNCCalcSize)
    CR_MESSAGE_HANDLER_EX(WM_SIZE, OnSize)
    CR_MESSAGE_HANDLER_EX(WM_CREATE, OnCreate)
    CR_MESSAGE_HANDLER_EX(WM_DESTROY, OnDestroy)
    CR_MESSAGE_HANDLER_EX(DM_POINTERHITTEST, OnPointerHitTest)
  CR_END_MSG_MAP()

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
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest,
                           LegacyRenderWidgetHostHWNDPointerEventsWhileHidden);

  explicit LegacyRenderWidgetHostHWND(RenderWidgetHostViewAura* host);
  ~LegacyRenderWidgetHostHWND() override;

  // If initialization fails, deletes `this` and returns false.
  bool InitOrDeleteSelf(HWND parent);

  // Returns the target to which the windows input events are forwarded.
  static ui::WindowEventTarget* GetWindowEventTarget(HWND parent);

  LRESULT OnEraseBkGnd(UINT message, WPARAM w_param, LPARAM l_param);
  LRESULT OnGetObject(UINT message, WPARAM w_param, LPARAM l_param);
  LRESULT OnInput(UINT message, WPARAM w_param, LPARAM l_param);
  LRESULT OnKeyboardRange(UINT message, WPARAM w_param, LPARAM l_param);
  LRESULT OnMouseLeave(UINT message, WPARAM w_param, LPARAM l_param);
  LRESULT OnMouseRange(UINT message, WPARAM w_param, LPARAM l_param);
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
  LRESULT OnCreate(UINT message, WPARAM w_param, LPARAM l_param);
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

  // Set to true once WM_CREATE handling has completed and back to false before
  // processing WM_DESTROY. Requests for accessibility objects via WM_GETOBJECT
  // are ignored outside of this window.
  bool may_service_accessibility_requests_ = false;

  // This class provides functionality to register the legacy window as a
  // Direct Manipulation consumer. This allows us to support smooth scroll
  // in Chrome on Windows 10.
  std::unique_ptr<DirectManipulationHelper> direct_manipulation_helper_;

  // Instruct aura::WindowTreeHost to use the HWND's parent for lookup.
  std::unique_ptr<ui::ViewProp> window_tree_host_prop_;

  // Track pointers that were down before hide and parent window before hide.
  // This is to ensure any touch gestures initiated before hide and ended
  // during hide are routed to the correct WindowEventTarget.
  // See comment in OnPointer.
  std::set<uint32_t> down_pointers_before_hide_;
  HWND parent_before_hide_ = nullptr;

  CR_MSG_MAP_CLASS_DECLARATIONS(LegacyRenderWidgetHostHWND)
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_LEGACY_RENDER_WIDGET_HOST_WIN_H_
