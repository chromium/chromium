// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/dom_ai.h"

#include "third_party/blink/renderer/modules/ai/ai.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

DOMAI::DOMAI(ExecutionContext& context)
    : Supplement<ExecutionContext>(context) {}

void DOMAI::Trace(Visitor* visitor) const {
  visitor->Trace(ai_);
  Supplement<ExecutionContext>::Trace(visitor);
}

// static
const char DOMAI::kSupplementName[] = "DOMAI";

// static
DOMAI& DOMAI::From(ExecutionContext& context) {
  DOMAI* supplement = Supplement<ExecutionContext>::From<DOMAI>(context);
  if (!supplement) {
    supplement = MakeGarbageCollected<DOMAI>(context);
    ProvideTo(context, supplement);
  }
  return *supplement;
}

// static
AI* DOMAI::ai(ExecutionContext& context) {
  return From(context).ai();
}

AI* DOMAI::ai() {
  if (!ai_) {
    ai_ = MakeGarbageCollected<AI>(GetSupplementable());
  }
  return ai_.Get();
}

}  // namespace blink
