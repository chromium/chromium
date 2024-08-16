// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ON_DEVICE_TRANSLATION_LANGUAGE_DETECTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ON_DEVICE_TRANSLATION_LANGUAGE_DETECTOR_H_

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_detection_result.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// The class that represents a Detector with source and target language.
class LanguageDetector final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit LanguageDetector();
  ~LanguageDetector() override = default;

  void Trace(Visitor* visitor) const override;

  // language_detector.idl implementation.
  ScriptPromise<IDLSequence<LanguageDetectionResult>> detect(
      ScriptState* script_state,
      const WTF::String& input,
      ExceptionState& exception_state);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ON_DEVICE_TRANSLATION_LANGUAGE_DETECTOR_H_
