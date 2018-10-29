// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

#include "third_party/blink/renderer/modules/badging/badge.h"

#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/renderer/bindings/modules/v8/usv_string_or_long.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

const char Badge::kSupplementName[] = "Badge";

Badge::~Badge() = default;

// static
Badge* Badge::From(ExecutionContext* context) {
  Badge* supplement = Supplement<ExecutionContext>::From<Badge>(context);
  if (!supplement) {
    supplement = new Badge(context);
    ProvideTo(*context, supplement);
  }
  return supplement;
}

// static
void Badge::set(ScriptState* script_state, ExceptionState& exception_state) {
  BadgeFromState(script_state)->Set(nullptr, exception_state);
}

// static
void Badge::set(ScriptState* script_state,
                USVStringOrLong& contents,
                ExceptionState& exception_state) {
  BadgeFromState(script_state)->Set(&contents, exception_state);
}

// static
void Badge::clear(ScriptState* script_state) {
  BadgeFromState(script_state)->Clear();
}

void Badge::Set(USVStringOrLong* contents, ExceptionState& exception_state) {
  if (contents) {
    if (contents->IsLong() && contents->GetAsLong() <= 0) {
      exception_state.ThrowTypeError("Badge contents should be > 0");
      return;
    }
    if (contents->IsUSVString() && contents->GetAsUSVString() == "") {
      exception_state.ThrowTypeError(
          "Badge contents cannot be the empty string");
      return;
    }
  }
  // TODO(estevenson): Add support for sending badge contents to the browser.
  // TODO(estevenson): Verify that contents is a single grapheme cluster.
  badge_service_->SetBadge();
}

void Badge::Clear() {
  badge_service_->ClearBadge();
}

void Badge::Trace(blink::Visitor* visitor) {
  Supplement<ExecutionContext>::Trace(visitor);
  ScriptWrappable::Trace(visitor);
}

Badge::Badge(ExecutionContext* context) {
  context->GetInterfaceProvider()->GetInterface(
      mojo::MakeRequest(&badge_service_));
  DCHECK(badge_service_);
}

// static
Badge* Badge::BadgeFromState(ScriptState* script_state) {
  return Badge::From(ExecutionContext::From(script_state));
}

}  // namespace blink
