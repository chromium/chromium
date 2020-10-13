// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/virtualkeyboard/virtual_keyboard.h"

#include "third_party/blink/renderer/core/css/document_style_environment_variables.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/editing/ime/input_method_controller.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/virtualkeyboard/virtual_keyboard_geometry_change_event.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"

namespace blink {

VirtualKeyboard::VirtualKeyboard(LocalFrame* frame)
    : ExecutionContextClient(frame ? frame->DomWindow()->GetExecutionContext()
                                   : nullptr),
      VirtualKeyboardOverlayChangedObserver(frame) {}

ExecutionContext* VirtualKeyboard::GetExecutionContext() const {
  return ExecutionContextClient::GetExecutionContext();
}

const AtomicString& VirtualKeyboard::InterfaceName() const {
  return event_target_names::kVirtualKeyboard;
}

VirtualKeyboard::~VirtualKeyboard() = default;

bool VirtualKeyboard::overlaysContent() const {
  return overlays_content_;
}

DOMRect* VirtualKeyboard::boundingRect() const {
  return bounding_rect_;
}

void VirtualKeyboard::setOverlaysContent(bool overlays_content) {
  LocalFrame* frame = GetFrame();
  if (frame && frame->IsMainFrame()) {
    if (overlays_content != overlays_content_) {
      auto& local_frame_host = frame->GetLocalFrameHostRemote();
      local_frame_host.SetVirtualKeyboardOverlayPolicy(overlays_content);
      overlays_content_ = overlays_content;
    }
  } else {
    GetExecutionContext()->AddConsoleMessage(
        MakeGarbageCollected<ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kJavaScript,
            mojom::blink::ConsoleMessageLevel::kWarning,
            "Setting overlaysContent is only supported from "
            "the top level browsing context"));
  }
}

void VirtualKeyboard::VirtualKeyboardOverlayChanged(
    const gfx::Rect& keyboard_rect) {
  bounding_rect_ = DOMRect::FromFloatRect(FloatRect(gfx::RectF(keyboard_rect)));
  LocalFrame* frame = GetFrame();
  if (frame && frame->GetDocument()) {
    DocumentStyleEnvironmentVariables& vars =
        frame->GetDocument()->GetStyleEngine().EnsureEnvironmentVariables();
    vars.SetVariable(UADefinedVariable::kKeyboardInsetTop,
                     StyleEnvironmentVariables::FormatPx(keyboard_rect.y()));
    vars.SetVariable(UADefinedVariable::kKeyboardInsetLeft,
                     StyleEnvironmentVariables::FormatPx(keyboard_rect.x()));
    vars.SetVariable(
        UADefinedVariable::kKeyboardInsetBottom,
        StyleEnvironmentVariables::FormatPx(keyboard_rect.bottom()));
    vars.SetVariable(
        UADefinedVariable::kKeyboardInsetRight,
        StyleEnvironmentVariables::FormatPx(keyboard_rect.right()));
    vars.SetVariable(
        UADefinedVariable::kKeyboardInsetWidth,
        StyleEnvironmentVariables::FormatPx(keyboard_rect.width()));
    vars.SetVariable(
        UADefinedVariable::kKeyboardInsetHeight,
        StyleEnvironmentVariables::FormatPx(keyboard_rect.height()));
  }
  DispatchEvent(*(MakeGarbageCollected<VirtualKeyboardGeometryChangeEvent>(
      event_type_names::kGeometrychange)));
}

void VirtualKeyboard::show() {
  LocalFrame* frame = GetFrame();
  if (frame && frame->HasStickyUserActivation()) {
    frame->GetInputMethodController().SetVirtualKeyboardVisibilityRequest(
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
  LocalFrame* frame = GetFrame();
  if (frame) {
    frame->GetInputMethodController().SetVirtualKeyboardVisibilityRequest(
        ui::mojom::VirtualKeyboardVisibilityRequest::HIDE);
  }
}

void VirtualKeyboard::Trace(Visitor* visitor) const {
  visitor->Trace(bounding_rect_);
  EventTargetWithInlineData::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

}  // namespace blink
