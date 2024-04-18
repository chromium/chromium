// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/on_device_translation/dom_translation.h"

#include "third_party/blink/renderer/modules/on_device_translation/translation.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

DOMTranslation::DOMTranslation(ExecutionContext& context)
    : Supplement<ExecutionContext>(context),
      translation_(MakeGarbageCollected<Translation>(&context)) {}

void DOMTranslation::Trace(Visitor* visitor) const {
  visitor->Trace(translation_);
  Supplement<ExecutionContext>::Trace(visitor);
}

// static
const char DOMTranslation::kSupplementName[] = "DOMTranslation";

// static
DOMTranslation& DOMTranslation::From(ExecutionContext& context) {
  DOMTranslation* supplement =
      Supplement<ExecutionContext>::From<DOMTranslation>(context);
  if (!supplement) {
    supplement = MakeGarbageCollected<DOMTranslation>(context);
    ProvideTo(context, supplement);
  }
  return *supplement;
}

// static
Translation* DOMTranslation::translation(ExecutionContext& context) {
  return From(context).translation_;
}

}  // namespace blink
