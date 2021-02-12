// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/eyedropper/eye_dropper.h"

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"

namespace blink {

EyeDropper::EyeDropper(ScriptState* script_state)
    : ExecutionContextClient(ExecutionContext::From(script_state)),
      opened_(false) {}

EyeDropper* EyeDropper::Create(ScriptState* script_state) {
  return MakeGarbageCollected<EyeDropper>(script_state);
}

ExecutionContext* EyeDropper::GetExecutionContext() const {
  return ExecutionContextClient::GetExecutionContext();
}

const AtomicString& EyeDropper::InterfaceName() const {
  return event_target_names::kEyeDropper;
}

EyeDropper::~EyeDropper() = default;

bool EyeDropper::opened() const {
  return opened_;
}

ScriptPromise EyeDropper::open() {
  // TODO(iopopesc): Add support for open.
  return ScriptPromise();
}

void EyeDropper::close() {
  // TODO(iopopesc): Add support for close.
}

void EyeDropper::Trace(Visitor* visitor) const {
  EventTargetWithInlineData::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

}  // namespace blink
