// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AI_CLASSIFIER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AI_CLASSIFIER_H_

#include "base/task/sequenced_task_runner.h"
#include "third_party/blink/public/mojom/ai/ai_classifier.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_availability.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_classifier_classify_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_classifier_create_options.h"
#include "third_party/blink/renderer/modules/ai/ai_writing_assistance_base.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

// Stub for CreateCoreOptions in AIWritingAssistant Base
struct ClassifierCreateCoreOptionsStub {};

class Classifier;

// The Classifier class provides the JavaScript API for categorizing text.
// It inherits from AIWritingAssistanceBase to manage the Mojo connection
// lifecycle and route requests to the browser-process AI model.
using ClassifierBase =
    AIWritingAssistanceBase<Classifier,
                            mojom::blink::AIClassifier,
                            mojom::blink::AIManagerCreateClassifierClient,
                            ClassifierCreateCoreOptionsStub,
                            ClassifierCreateOptions,
                            ClassifierClassifyOptions>;

class Classifier final : public ScriptWrappable, public ClassifierBase {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static ScriptPromise<Classifier> create(ScriptState* script_state,
                                          ClassifierCreateOptions* options,
                                          ExceptionState& exception_state);

  static ScriptPromise<V8Availability> availability(
      ScriptState* script_state,
      ExceptionState& exception_state);

  Classifier(ScriptState* script_state,
             scoped_refptr<base::SequencedTaskRunner> task_runner,
             mojo::PendingRemote<mojom::blink::AIClassifier> pending_remote,
             ClassifierCreateOptions* options);

  void Trace(Visitor* visitor) const override;

  // AIWritingAssistanceBase overrides
  void remoteExecute(
      const String& input,
      const String& context,
      mojo::PendingRemote<blink::mojom::blink::ModelStreamingResponder>
          responder) override;

  // classifier.idl implementation
  ScriptPromise<IDLString> classify(ScriptState* script_state,
                                    const String& input,
                                    const ClassifierClassifyOptions* options,
                                    ExceptionState& exception_state);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AI_CLASSIFIER_H_
