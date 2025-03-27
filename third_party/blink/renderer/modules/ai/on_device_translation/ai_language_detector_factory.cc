// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/on_device_translation/ai_language_detector_factory.h"

#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_create_monitor_callback.h"
#include "third_party/blink/renderer/modules/ai/ai.h"
#include "third_party/blink/renderer/modules/ai/ai_availability.h"
#include "third_party/blink/renderer/modules/ai/ai_context_observer.h"
#include "third_party/blink/renderer/modules/ai/ai_create_monitor.h"
#include "third_party/blink/renderer/modules/ai/ai_interface_proxy.h"
#include "third_party/blink/renderer/modules/ai/ai_utils.h"
#include "third_party/blink/renderer/modules/ai/exception_helpers.h"
#include "third_party/blink/renderer/modules/ai/on_device_translation/language_detector.h"
#include "third_party/blink/renderer/platform/language_detection/language_detection_model.h"

namespace blink {

AILanguageDetectorFactory::AILanguageDetectorFactory(
    ExecutionContext* context) {}

void AILanguageDetectorFactory::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
}

ScriptPromise<V8AIAvailability> AILanguageDetectorFactory::availability(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  return LanguageDetector::availability(script_state, exception_state);
}

ScriptPromise<LanguageDetector> AILanguageDetectorFactory::create(
    ScriptState* script_state,
    LanguageDetectorCreateOptions* options,
    ExceptionState& exception_state) {
  return LanguageDetector::create(script_state, options, exception_state);
}

}  // namespace blink
