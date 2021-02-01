// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_HANDWRITING_HANDWRITING_STROKE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_HANDWRITING_HANDWRITING_STROKE_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

class ExecutionContext;
class HandwritingPoint;

class HandwritingStroke final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit HandwritingStroke(ExecutionContext* context);
  ~HandwritingStroke() override;

  static HandwritingStroke* Create(ExecutionContext* context);

  // IDL Interface:
  void addPoint(const HandwritingPoint* point);
  const HeapVector<Member<const HandwritingPoint>>& getPoints() const;
  void clear();

  void Trace(Visitor* visitor) const override;

 private:
  HeapVector<Member<const HandwritingPoint>> points_;

  DISALLOW_COPY_AND_ASSIGN(HandwritingStroke);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_HANDWRITING_HANDWRITING_STROKE_H_
