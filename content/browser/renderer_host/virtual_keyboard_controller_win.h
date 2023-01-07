// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_VIRTUAL_KEYBOARD_CONTROLLER_WIN_H_
#define CONTENT_BROWSER_RENDERER_HOST_VIRTUAL_KEYBOARD_CONTROLLER_WIN_H_

#include "base/memory/raw_ptr.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "ui/base/ime/virtual_keyboard_controller_observer.h"

namespace gfx {
class Rect;
}

namespace ui {
namespace mojom {
class TextInputState;
}
class InputMethod;
}

namespace content {

class RenderWidgetHostViewAura;

// This class implements the ui::VirtualKeyboardControllerObserver interface
// which provides notifications about the on-screen keyboard on Windows getting
// displayed or hidden in response to taps on editable fields.
// It provides functionality to request blink to scroll the input field if it
// is obscured by the on screen keyboard.
// TryShow/TryHide APIs are Windows system APIs that are used to show/hide VK
// respectively.
// https://docs.microsoft.com/en-us/uwp/api/windows.ui.viewmanagement.inputpane?view=winrt-18362
class VirtualKeyboardControllerWin
    : public ui::VirtualKeyboardControllerObserver {
 public:
  VirtualKeyboardControllerWin(RenderWidgetHostViewAura* host_view,
                               ui::InputMethod* input_method);
  VirtualKeyboardControllerWin(const VirtualKeyboardControllerWin&) = delete;
  ~VirtualKeyboardControllerWin() override;

  VirtualKeyboardControllerWin& operator=(const VirtualKeyboardControllerWin&) =
      delete;

  void UpdateTextInputState(const ui::mojom::TextInputState* state);
  void FocusedNodeChanged(bool is_editable);
  void HideAndNotifyKeyboardInset();

  // VirtualKeyboardControllerObserver overrides.
  void OnKeyboardVisible(const gfx::Rect& keyboard_rect) override;
  void OnKeyboardHidden() override;

 private:
  void ShowVirtualKeyboard();
  void HideVirtualKeyboard();
  bool IsPointerTypeValidForVirtualKeyboard() const;

  raw_ptr<RenderWidgetHostViewAura> host_view_;
  raw_ptr<ui::InputMethod> input_method_;
  bool observers_registered_ = false;
  bool virtual_keyboard_shown_ = false;
  bool is_manual_policy_ = false;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_VIRTUAL_KEYBOARD_CONTROLLER_WIN_H_
