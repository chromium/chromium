// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/handwriting/handwriting_recognition_service.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_handwriting_feature_query.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_handwriting_feature_query_result.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_handwriting_model_constraint.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/handwriting/handwriting_recognizer.h"

namespace blink {

const char HandwritingRecognitionService::kSupplementName[] =
    "NavigatorHandwritingRecognitionService";

HandwritingRecognitionService::HandwritingRecognitionService(
    Navigator& navigator)
    : Supplement<Navigator>(navigator) {}

// static
HandwritingRecognitionService& HandwritingRecognitionService::From(
    Navigator& navigator) {
  HandwritingRecognitionService* supplement =
      Supplement<Navigator>::From<HandwritingRecognitionService>(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<HandwritingRecognitionService>(navigator);
    ProvideTo(navigator, supplement);
  }
  return *supplement;
}

// static
ScriptPromise HandwritingRecognitionService::createHandwritingRecognizer(
    ScriptState* script_state,
    Navigator& navigator,
    const HandwritingModelConstraint* constraint) {
  return HandwritingRecognitionService::From(navigator)
      .CreateHandwritingRecognizer(script_state, constraint);
}

ScriptPromise HandwritingRecognitionService::CreateHandwritingRecognizer(
    ScriptState* script_state,
    const HandwritingModelConstraint* constraint) {
  ScriptPromiseResolver* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  auto promise = resolver->Promise();

  auto* handwriting_recognizer = MakeGarbageCollected<HandwritingRecognizer>(
      ExecutionContext::From(script_state));

  resolver->Resolve(handwriting_recognizer);
  return promise;
}

// static
ScriptPromise HandwritingRecognitionService::queryHandwritingRecognizerSupport(
    ScriptState* script_state,
    Navigator& navigator,
    const HandwritingFeatureQuery* query) {
  ScriptPromiseResolver* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  auto promise = resolver->Promise();
  // TODO(crbug.com/1166910): This should call out to a mojo service. However,
  // we can't land the mojo service until a browser side implementation is
  // available (for security review). Until then, use this stub which never
  // resolves.
  auto* query_result = MakeGarbageCollected<HandwritingFeatureQueryResult>();
  resolver->Resolve(query_result);
  return promise;
}

void HandwritingRecognitionService::Trace(Visitor* visitor) const {
  Supplement<Navigator>::Trace(visitor);
}

}  // namespace blink
