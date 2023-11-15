// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_LEGACY_RENDER_WIDGET_HOST_WIN_H_
#define CONTENT_BROWSER_RENDERER_HOST_LEGACY_RENDER_WIDGET_HOST_WIN_H_

#include "base/memory/raw_ptr.h"

#include <oleacc.h>
#include <wrl/client.h>

#include <memory>

#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "ui/accessibility/platform/ax_fragment_root_delegate_win.h"
#include "ui/base/win/internal_constants.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/win/msg_util.h"
#include "ui/gfx/win/window_impl.h"

namespace ui {
class AXFragmentRootWin;
class AXSystemCaretWin;
class WindowEventTarget;
}  // namespace ui

namespace content {

class DirectManipulationBrowserTestBase;
class DirectManipulationHelper;
class RenderWidgetHostViewAura;

// Reasons for the existence of this class outlined below:-
// 1. Some screen readers expect every tab / every unique web content container
//    to be in its own HWND with class name Chrome_RenderWidgetHostHWND.
//    With Aura there is one main HWND which comprises the whole browser window
//    or the whole desktop. So, we need a fake HWND with the window class as
//    Chrome_RenderWidgetHostHWND as the root of the accessibility tree for
//    each tab.
// 2. There are legacy drivers for trackpads/trackpoints which have special
//    code for sending mouse wheel and scroll events to the
//    Chrome_RenderWidgetHostHWND window.
// 3. Windowless NPAPI plugins like Flash and Silverlight which expect the
//    container window to have the same bounds as the web page. In Aura, the
//    default container window is the whole window which includes the web page
//    WebContents, etc. This causes the plugin mouse event calculations to
//    fail.
//    We should look to get rid of this code when all of the above are fixed.

// This class implements a child HWND with the same size as the content area,
// that delegates its accessibility implementation to the root of the
// BrowserAccessibilityManager tree. This HWND is hooked up as the parent of
// the root object in the BrowserAccessibilityManager tree, so when any
// accessibility client calls ::WindowFromAccessibleObject, they get this
// HWND instead of the DesktopWindowTreeHostWin.
class CONTENT_EXPORT LegacyRenderWidgetHostHWND
    : public gfx::WindowImpl,
      public ui::AXFragmentRootDelegateWin {
 public:
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
    CR_MESSAGE_HANDLER_EX(WM_DESTROY, OnDestroy)
    CR_MESSAGE_HANDLER_EX(DM_POINTERHITTEST, OnPointerHitTest)
  CR_END_MSG_MAP()

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
  bool mouse_tracking_enabled_;

  raw_ptr<RenderWidgetHostViewAura> host_;

  // Some assistive software need to track the location of the caret.
  std::unique_ptr<ui::AXSystemCaretWin> ax_system_caret_;

  // Implements IRawElementProviderFragmentRoot when UIA is enabled.
  std::unique_ptr<ui::AXFragmentRootWin> ax_fragment_root_;

  // Set to true when we return a UIA object. Determines whether we need to
  // call UIA to clean up object references on window destruction.
  // This is important to avoid triggering a cross-thread COM call which could
  // cause re-entrancy during teardown. https://crbug.com/1087553
  bool did_return_uia_object_;

  // This class provides functionality to register the legacy window as a
  // Direct Manipulation consumer. This allows us to support smooth scroll
  // in Chrome on Windows 10.
  std::unique_ptr<DirectManipulationHelper> direct_manipulation_helper_;

  CR_MSG_MAP_CLASS_DECLARATIONS(LegacyRenderWidgetHostHWND)

  base::WeakPtrFactory<LegacyRenderWidgetHostHWND> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_LEGACY_RENDER_WIDGET_HOST_WIN_H_

