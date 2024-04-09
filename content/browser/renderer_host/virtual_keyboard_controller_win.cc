// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/virtual_keyboard_controller_win.h"

#include "base/trace_event/trace_event.h"
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/mojom/text_input_state.mojom.h"
#include "ui/base/ime/virtual_keyboard_controller.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/geometry/rect.h"

namespace content {

VirtualKeyboardControllerWin::VirtualKeyboardControllerWin(
    RenderWidgetHostViewAura* host_view,
    ui::InputMethod* input_method)
    : host_view_(host_view), input_method_(input_method) {
  host_view_->SetInsets(gfx::Insets());
}

VirtualKeyboardControllerWin::~VirtualKeyboardControllerWin() {
  if (observers_registered_) {
    // De-register the input pane observers.
    if (auto* controller = input_method_->GetVirtualKeyboardController())
      controller->RemoveObserver(this);
  }
}

void VirtualKeyboardControllerWin::HideAndNotifyKeyboardInset() {
  if (auto* controller = input_method_->GetVirtualKeyboardController()) {
    if (virtual_keyboard_shown_) {
      // If the VK is already showing, then dismiss it first.
      controller->DismissVirtualKeyboard();
      // Should also scroll the content into view after the VK dismisses.
      OnKeyboardHidden();
    }
  }
}

void VirtualKeyboardControllerWin::OnKeyboardVisible(
    const gfx::Rect& keyboard_rect) {
  TRACE_EVENT0("vk", "VirtualKeyboardControllerWin::OnKeyboardVisible");
  // If the software input panel (SIP) is manually raised by the user, the flag
  // should be set so we don't call TryShow API again.
  virtual_keyboard_shown_ = true;
  if (host_view_->GetVirtualKeyboardMode() !=
      ui::mojom::VirtualKeyboardMode::kOverlaysContent) {
    host_view_->SetInsets(gfx::Insets::TLBR(
        0, 0, keyboard_rect.IsEmpty() ? 0 : keyboard_rect.height(), 0));
  } else {
    host_view_->NotifyVirtualKeyboardOverlayRect(keyboard_rect);
  }
}

void VirtualKeyboardControllerWin::OnKeyboardHidden() {
  TRACE_EVENT0("vk", "VirtualKeyboardControllerWin::OnKeyboardHidden");
  // If the software input panel (SIP) is manually closed by the user, the flag
  // should be reset so we don't call TryHide API again. Also,
  // next time user taps on an editable element after manually dismissing the
  // keyboard, this flag is used to determine whether TryShow needs to be
  // called or not. Calling TryShow/TryHide multiple times leads to SIP
  // flickering.
  virtual_keyboard_shown_ = false;
  if (host_view_->GetVirtualKeyboardMode() !=
      ui::mojom::VirtualKeyboardMode::kOverlaysContent) {
    // Restore the viewport.
    host_view_->SetInsets(gfx::Insets());
  } else {
    host_view_->NotifyVirtualKeyboardOverlayRect(gfx::Rect());
  }
}

void VirtualKeyboardControllerWin::ShowVirtualKeyboard() {
  TRACE_EVENT0("vk", "VirtualKeyboardControllerWin::ShowVirtualKeyboard");
  if (input_method_->GetVirtualKeyboardController()) {
    if (!virtual_keyboard_shown_) {
      virtual_keyboard_shown_ = true;
      input_method_->SetVirtualKeyboardVisibilityIfEnabled(true);
    }
  }
}

void VirtualKeyboardControllerWin::HideVirtualKeyboard() {
  TRACE_EVENT0("vk", "VirtualKeyboardControllerWin::HideVirtualKeyboard");
  if (auto* controller = input_method_->GetVirtualKeyboardController()) {
    if (virtual_keyboard_shown_) {
      virtual_keyboard_shown_ = false;
      controller->DismissVirtualKeyboard();
    }
  }
}

void VirtualKeyboardControllerWin::UpdateTextInputState(
    const ui::mojom::TextInputState* state) {
  // Conditions to show the VK:
  // 1. User has to interact with the editable element.
  // 2. Pointer type has to be either touch or pen.
  // 3. Accessibility has set focus on an editable element.
  // 4. If virtualkeyboardpolicy is manual, leave the SIP in its current state -
  //    script authors need to call show() or hide() explicitly to trigger SIP
  //    actions.
  // 5. If virtualkeyboardpolicy is auto, show the SIP.
  // If there are no keyboard controllers or the pointer type is neither pen or
  // touch or accessibility has not set focus into an editable element, then
  // don't change the state of the keyboard.
  auto* controller = input_method_->GetVirtualKeyboardController();
  is_manual_policy_ =
      state->vk_policy == ui::mojom::VirtualKeyboardPolicy::MANUAL;
  if (!controller ||
      !(IsPointerTypeValidForVirtualKeyboard() || is_manual_policy_) ||
      !host_view_->host()->GetView() || !host_view_->host()->delegate()) {
    return;
  }
  // Register the observers if the pointer type is pen/touch.
  if (!observers_registered_) {
    controller->AddObserver(this);
    observers_registered_ = true;
  }
  if (state->show_ime_if_needed &&
      state->vk_policy == ui::mojom::VirtualKeyboardPolicy::AUTO) {
    ShowVirtualKeyboard();
    return;
  }

  if (is_manual_policy_) {
    switch (state->last_vk_visibility_request) {
      case ui::mojom::VirtualKeyboardVisibilityRequest::SHOW:
        if (host_view_->FocusedFrameHasStickyActivation())
          ShowVirtualKeyboard();
        break;
      case ui::mojom::VirtualKeyboardVisibilityRequest::HIDE:
        HideVirtualKeyboard();
        break;
      default:
        // Don't change the state of the VK.
        break;
    }
  }
}

void VirtualKeyboardControllerWin::FocusedNodeChanged(bool is_editable) {
  if (!is_editable) {
    HideVirtualKeyboard();
    return;
  }
}

bool VirtualKeyboardControllerWin::IsPointerTypeValidForVirtualKeyboard()
    const {
  return (host_view_->GetLastPointerType() == ui::EventPointerType::kTouch ||
          host_view_->GetLastPointerType() == ui::EventPointerType::kPen);
}

}  // namespace content
