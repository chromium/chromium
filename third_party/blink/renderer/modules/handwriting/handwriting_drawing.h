// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_HANDWRITING_HANDWRITING_DRAWING_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_HANDWRITING_HANDWRITING_DRAWING_H_

#include "base/macros.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

class ExecutionContext;
class HandwritingRecognizer;
class HandwritingStroke;
class ScriptState;

class HandwritingDrawing final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit HandwritingDrawing(ExecutionContext* context,
                              HandwritingRecognizer* recognizer);
  ~HandwritingDrawing() override;

  // IDL Interface:
  void addStroke(HandwritingStroke* stroke);
  void removeStroke(const HandwritingStroke* stroke);
  void clear();
  ScriptPromise getPrediction(ScriptState* script_state);
  const HeapVector<Member<HandwritingStroke>>& getStrokes();

  void Trace(Visitor* visitor) const override;

 private:
  bool IsValid() const;

  HeapVector<Member<HandwritingStroke>> strokes_;

  WeakMember<HandwritingRecognizer> recognizer_;

  DISALLOW_COPY_AND_ASSIGN(HandwritingDrawing);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_HANDWRITING_HANDWRITING_DRAWING_H_
