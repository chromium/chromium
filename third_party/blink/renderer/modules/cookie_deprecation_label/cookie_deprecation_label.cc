// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/cookie_deprecation_label/cookie_deprecation_label.h"

#include <utility>

#include "base/check.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

// static
CookieDeprecationLabel* CookieDeprecationLabel::cookieDeprecationLabel(
    Navigator& navigator) {
  CookieDeprecationLabel* supplement = navigator.GetCookieDeprecationLabel();
  if (!supplement) {
    supplement = MakeGarbageCollected<CookieDeprecationLabel>(navigator);
    navigator.SetCookieDeprecationLabel(supplement);
  }
  return supplement;
}

CookieDeprecationLabel::CookieDeprecationLabel(Navigator& navigator)
    : navigator_(navigator) {}

CookieDeprecationLabel::~CookieDeprecationLabel() = default;

ScriptPromise<IDLString> CookieDeprecationLabel::getValue(
    ScriptState* script_state) {
  String label;

  if (auto* dom_window = navigator_->DomWindow()) {
    label = dom_window->document()->Loader()->GetCookieDeprecationLabel();
  }

  return ToResolvedPromise<IDLString>(script_state, label);
}

void CookieDeprecationLabel::Trace(Visitor* visitor) const {
  visitor->Trace(navigator_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
