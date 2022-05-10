// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/extensions/chromeos/system_extensions/window_management/cros_window.h"

#include "base/callback_forward.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/geometry/dom_point.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/extensions/chromeos/system_extensions/window_management/cros_window_management.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "ui/gfx/geometry/rect.h"

namespace blink {

static constexpr char kShown[] = "shown";
static constexpr char kHidden[] = "hidden";
static constexpr char kWindowStateFullscreen[] = "fullscreen";
static constexpr char kWindowStateMaximized[] = "maximized";
static constexpr char kWindowStateMinimized[] = "minimized";
static constexpr char kWindowStateNormal[] = "normal";

namespace {

void OnResponse(ScriptPromiseResolver* resolver,
                mojom::blink::CrosWindowManagementStatus status) {
  switch (status) {
    case mojom::blink::CrosWindowManagementStatus::kSuccess:
      resolver->Resolve();
      break;
    case mojom::blink::CrosWindowManagementStatus::kWindowNoWindowState:
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kInvalidStateError,
          "Operation couldn't be performed on window."));
      break;
    case mojom::blink::CrosWindowManagementStatus::kWindowNotFound:
    case mojom::blink::CrosWindowManagementStatus::kWindowNoWidget:
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotFoundError, "Window not found."));
      break;
    default:
      NOTREACHED();
      break;
  }
}
}  // namespace

CrosWindow::CrosWindow(CrosWindowManagement* manager,
                       mojom::blink::CrosWindowInfoPtr window)
    : window_management_(manager), window_(std::move(window)) {}

void CrosWindow::Trace(Visitor* visitor) const {
  visitor->Trace(window_management_);
  ScriptWrappable::Trace(visitor);
}

String CrosWindow::appId() {
  return window_->app_id;
}

String CrosWindow::title() {
  return window_->title;
}

String CrosWindow::windowState() {
  switch (window_->window_state) {
    case mojom::blink::WindowState::kFullscreen:
      return kWindowStateFullscreen;
    case mojom::blink::WindowState::kMaximized:
      return kWindowStateMaximized;
    case mojom::blink::WindowState::kMinimized:
      return kWindowStateMinimized;
    case mojom::blink::WindowState::kNormal:
      return kWindowStateNormal;
  }
}

bool CrosWindow::isFocused() {
  return window_->is_focused;
}

String CrosWindow::visibilityState() {
  switch (window_->visibility_state) {
    case mojom::blink::VisibilityState::kShown:
      return kShown;
    case mojom::blink::VisibilityState::kHidden:
      return kHidden;
  }
}

String CrosWindow::id() {
  return String::FromUTF8(window_->id.ToString());
}

int32_t CrosWindow::screenLeft() {
  return window_->bounds.x();
}

int32_t CrosWindow::screenTop() {
  return window_->bounds.y();
}

int32_t CrosWindow::width() {
  return window_->bounds.width();
}

int32_t CrosWindow::height() {
  return window_->bounds.height();
}

ScriptPromise CrosWindow::setOrigin(ScriptState* script_state,
                                    int32_t x,
                                    int32_t y) {
  auto* cros_window_management =
      window_management_->GetCrosWindowManagementOrNull();
  if (!cros_window_management) {
    return ScriptPromise();
  }
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  cros_window_management->SetWindowBounds(
      window_->id, x, y, window_->bounds.width(), window_->bounds.height(),
      WTF::Bind(&OnResponse, WrapPersistent(resolver)));
  return resolver->Promise();
}

ScriptPromise CrosWindow::setBounds(ScriptState* script_state,
                                    int32_t x,
                                    int32_t y,
                                    int32_t width,
                                    int32_t height) {
  auto* cros_window_management =
      window_management_->GetCrosWindowManagementOrNull();
  if (!cros_window_management) {
    return ScriptPromise();
  }
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  cros_window_management->SetWindowBounds(
      window_->id, x, y, width, height,
      WTF::Bind(&OnResponse, WrapPersistent(resolver)));
  return resolver->Promise();
}

ScriptPromise CrosWindow::setFullscreen(ScriptState* script_state,
                                        bool fullscreen) {
  auto* cros_window_management =
      window_management_->GetCrosWindowManagementOrNull();
  if (!cros_window_management) {
    return ScriptPromise();
  }
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  cros_window_management->SetFullscreen(
      window_->id, fullscreen,
      WTF::Bind(&OnResponse, WrapPersistent(resolver)));
  return resolver->Promise();
}

ScriptPromise CrosWindow::maximize(ScriptState* script_state) {
  auto* cros_window_management =
      window_management_->GetCrosWindowManagementOrNull();
  if (!cros_window_management) {
    return ScriptPromise();
  }
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  cros_window_management->Maximize(
      window_->id, WTF::Bind(&OnResponse, WrapPersistent(resolver)));
  return resolver->Promise();
}

ScriptPromise CrosWindow::minimize(ScriptState* script_state) {
  auto* cros_window_management =
      window_management_->GetCrosWindowManagementOrNull();
  if (!cros_window_management) {
    return ScriptPromise();
  }
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  cros_window_management->Minimize(
      window_->id, WTF::Bind(&OnResponse, WrapPersistent(resolver)));
  return resolver->Promise();
}

ScriptPromise CrosWindow::focus(ScriptState* script_state) {
  auto* cros_window_management =
      window_management_->GetCrosWindowManagementOrNull();
  if (!cros_window_management) {
    return ScriptPromise();
  }
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  cros_window_management->Focus(
      window_->id, WTF::Bind(&OnResponse, WrapPersistent(resolver)));
  return resolver->Promise();
}

void CrosWindow::close() {
  auto* cros_window_management =
      window_management_->GetCrosWindowManagementOrNull();
  if (!cros_window_management) {
    return;
  }
  cros_window_management->Close(window_->id);
}

}  // namespace blink
