// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/virtualkeyboard/virtual_keyboard.h"

#include "base/trace_event/trace_event.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/renderer/core/css/document_style_environment_variables.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/editing/ime/input_method_controller.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/frame/viewport_data.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/virtualkeyboard/virtual_keyboard_geometry_change_event.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"

namespace blink {

// static
const char VirtualKeyboard::kSupplementName[] = "VirtualKeyboard";

// static
VirtualKeyboard* VirtualKeyboard::virtualKeyboard(Navigator& navigator) {
  auto* keyboard = Supplement<Navigator>::From<VirtualKeyboard>(navigator);
  if (!keyboard) {
    keyboard = MakeGarbageCollected<VirtualKeyboard>(navigator);
    ProvideTo(navigator, keyboard);
  }
  return keyboard;
}

VirtualKeyboard::VirtualKeyboard(Navigator& navigator)
    : Supplement<Navigator>(navigator),
      VirtualKeyboardOverlayChangedObserver(
          navigator.DomWindow() ? navigator.DomWindow()->GetFrame() : nullptr) {
  bounding_rect_ = DOMRect::Create();
}

ExecutionContext* VirtualKeyboard::GetExecutionContext() const {
  return GetSupplementable()->DomWindow();
}

const AtomicString& VirtualKeyboard::InterfaceName() const {
  return event_target_names::kVirtualKeyboard;
}

VirtualKeyboard::~VirtualKeyboard() = default;

bool VirtualKeyboard::overlaysContent() const {
  LocalDOMWindow* window = GetSupplementable()->DomWindow();
  if (!window)
    return false;

  DCHECK(window->GetFrame());

  if (!window->GetFrame()->IsOutermostMainFrame())
    return false;

  return window->GetFrame()
      ->GetDocument()
      ->GetViewportData()
      .GetVirtualKeyboardOverlaysContent();
}

DOMRect* VirtualKeyboard::boundingRect() const {
  return bounding_rect_.Get();
}

void VirtualKeyboard::setOverlaysContent(bool overlays_content) {
  LocalDOMWindow* window = GetSupplementable()->DomWindow();
  if (!window)
    return;

  DCHECK(window->GetFrame());

  if (window->GetFrame()->IsOutermostMainFrame()) {
    window->GetFrame()
        ->GetDocument()
        ->GetViewportData()
        .SetVirtualKeyboardOverlaysContent(overlays_content);
  } else {
    GetExecutionContext()->AddConsoleMessage(
        MakeGarbageCollected<ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kJavaScript,
            mojom::blink::ConsoleMessageLevel::kWarning,
            "Setting overlaysContent is only supported from "
            "the top level browsing context"));
  }
  if (GetExecutionContext()) {
    UseCounter::Count(GetExecutionContext(),
                      WebFeature::kVirtualKeyboardOverlayPolicy);
  }
}

void VirtualKeyboard::VirtualKeyboardOverlayChanged(
    const gfx::Rect& keyboard_rect) {
  TRACE_EVENT0("vk", "VirtualKeyboard::VirtualKeyboardOverlayChanged");
  LocalDOMWindow* window = GetSupplementable()->DomWindow();
  if (!window)
    return;

  bounding_rect_ = DOMRect::FromRectF(gfx::RectF(keyboard_rect));
  DocumentStyleEnvironmentVariables& vars =
      window->document()->GetStyleEngine().EnsureEnvironmentVariables();
  vars.SetVariable(UADefinedVariable::kKeyboardInsetTop,
                   StyleEnvironmentVariables::FormatPx(keyboard_rect.y()));
  vars.SetVariable(UADefinedVariable::kKeyboardInsetLeft,
                   StyleEnvironmentVariables::FormatPx(keyboard_rect.x()));
  vars.SetVariable(UADefinedVariable::kKeyboardInsetBottom,
                   StyleEnvironmentVariables::FormatPx(keyboard_rect.bottom()));
  vars.SetVariable(UADefinedVariable::kKeyboardInsetRight,
                   StyleEnvironmentVariables::FormatPx(keyboard_rect.right()));
  vars.SetVariable(UADefinedVariable::kKeyboardInsetWidth,
                   StyleEnvironmentVariables::FormatPx(keyboard_rect.width()));
  vars.SetVariable(UADefinedVariable::kKeyboardInsetHeight,
                   StyleEnvironmentVariables::FormatPx(keyboard_rect.height()));
  DispatchEvent(*(MakeGarbageCollected<VirtualKeyboardGeometryChangeEvent>(
      event_type_names::kGeometrychange)));
}

void VirtualKeyboard::show() {
  TRACE_EVENT0("vk", "VirtualKeyboard::show");
  LocalDOMWindow* window = GetSupplementable()->DomWindow();
  if (!window)
    return;

  if (window->GetFrame()->HasStickyUserActivation()) {
    window->GetInputMethodController().SetVirtualKeyboardVisibilityRequest(
        ui::mojom::VirtualKeyboardVisibilityRequest::SHOW);
  } else {
    GetExecutionContext()->AddConsoleMessage(
        MakeGarbageCollected<ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kJavaScript,
            mojom::blink::ConsoleMessageLevel::kWarning,
            "Calling show is only supported if user has "
            "interacted with the page"));
  }
}

void VirtualKeyboard::hide() {
  TRACE_EVENT0("vk", "VirtualKeyboard::hide");
  LocalDOMWindow* window = GetSupplementable()->DomWindow();
  if (!window)
    return;

  window->GetInputMethodController().SetVirtualKeyboardVisibilityRequest(
      ui::mojom::VirtualKeyboardVisibilityRequest::HIDE);
}

void VirtualKeyboard::Trace(Visitor* visitor) const {
  visitor->Trace(bounding_rect_);
  EventTarget::Trace(visitor);
  Supplement<Navigator>::Trace(visitor);
}

}  // namespace blink
