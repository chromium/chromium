// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/on_device_translation/ai_translator_factory.h"

namespace blink {

AITranslatorFactory::AITranslatorFactory(ExecutionContext* context) {}

ScriptPromise<V8AIAvailability> AITranslatorFactory::availability(
    ScriptState* script_state,
    AITranslatorCreateCoreOptions* options,
    ExceptionState& exception_state) {
  return AITranslator::availability(script_state, options, exception_state);
}

ScriptPromise<AITranslator> AITranslatorFactory::create(
    ScriptState* script_state,
    AITranslatorCreateOptions* options,
    ExceptionState& exception_state) {
  return AITranslator::create(script_state, options, exception_state);
}

void AITranslatorFactory::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
