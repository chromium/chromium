// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/on_device_translation/ai_translator_factory.h"

namespace blink {

AITranslatorFactory::AITranslatorFactory(ExecutionContext* context) {}

ScriptPromise<V8AIAvailability> AITranslatorFactory::availability(
    ScriptState* script_state,
    TranslatorCreateCoreOptions* options,
    ExceptionState& exception_state) {
  return Translator::availability(script_state, options, exception_state);
}

ScriptPromise<Translator> AITranslatorFactory::create(
    ScriptState* script_state,
    TranslatorCreateOptions* options,
    ExceptionState& exception_state) {
  return Translator::create(script_state, options, exception_state);
}

void AITranslatorFactory::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
