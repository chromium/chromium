// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_HANDWRITING_HANDWRITING_RECOGNITION_SERVICE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_HANDWRITING_HANDWRITING_RECOGNITION_SERVICE_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class HandwritingModelConstraint;
class HandwritingFeatureQuery;
class ScriptState;

class HandwritingRecognitionService final
    : public GarbageCollected<HandwritingRecognitionService>,
      public Supplement<Navigator> {
 public:
  static const char kSupplementName[];

  explicit HandwritingRecognitionService(Navigator&);

  static HandwritingRecognitionService& From(Navigator&);

  // IDL Interface:
  static ScriptPromise createHandwritingRecognizer(
      ScriptState*,
      Navigator&,
      const HandwritingModelConstraint*);
  static ScriptPromise queryHandwritingRecognizerSupport(
      ScriptState*,
      Navigator&,
      const HandwritingFeatureQuery*);

  void Trace(Visitor* visitor) const override;

 private:
  ScriptPromise CreateHandwritingRecognizer(ScriptState*,
                                            const HandwritingModelConstraint*);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_HANDWRITING_HANDWRITING_RECOGNITION_SERVICE_H_
