// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_HANDWRITING_HANDWRITING_RECOGNIZER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_HANDWRITING_HANDWRITING_RECOGNIZER_H_

#include "base/macros.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

class ExceptionState;
class ExecutionContext;
class HandwritingDrawing;
class HandwritingHints;
class ScriptState;

class HandwritingRecognizer final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit HandwritingRecognizer(ExecutionContext* context);
  ~HandwritingRecognizer() override;

  // Used by the drawing to see if the recognizer is valid.
  bool IsValid();

  // IDL Interface:
  HandwritingDrawing* startDrawing(ScriptState* script_state,
                                   const HandwritingHints* hints,
                                   ExceptionState& exception_state);
  void finish(ExceptionState& exception_state);

  void Trace(Visitor* visitor) const override;

 private:
  // This function will invalidate the recognizer and its drawings;
  void Invalidate();

  // TODO(crbug.com/1166910): We may use the status of the mojo remote as and
  // indicator instead of having a standalone boolean. However, we can't land
  // the mojo service until a browser side implementation is available (for
  // security review). Until then, use this stub which never resolves.
  bool is_valid_;

  DISALLOW_COPY_AND_ASSIGN(HandwritingRecognizer);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_HANDWRITING_HANDWRITING_RECOGNIZER_H_
