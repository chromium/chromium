// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CONTAINER_QUERY_EVALUATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CONTAINER_QUERY_EVALUATOR_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/media_query_evaluator.h"
#include "third_party/blink/renderer/core/layout/geometry/axis.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_size.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class ContainerQuery;

class CORE_EXPORT ContainerQueryEvaluator final
    : public GarbageCollected<ContainerQueryEvaluator> {
 public:
  ContainerQueryEvaluator(PhysicalSize, PhysicalAxes contained_axes);

  bool Eval(const ContainerQuery&) const;

  // Add a dependent query to this evaluator. During calls to ContainerChanged,
  // all dependent queries are checked to see if the new size/axis information
  // causes a change in the evaluation result.
  void Add(const ContainerQuery&, bool result);

  bool EvalAndAdd(const ContainerQuery& query) {
    bool result = Eval(query);
    Add(query, result);
    return result;
  }

  // Update the size/axis information of the evaluator.
  //
  // A return value of 'false' means that the update has no effect on the
  // evaluation of queries associated with this evaluator, and therefore we do
  // not need to perform style recalc of any elements which depend on this
  // evaluator.
  //
  // A return value of 'true' means that the update *may* have an effect, and
  // therefore elements that depends on this evaluator need style recalc.
  //
  // Dependent queries are cleared when 'true' is returned (and left unchanged
  // otherwise).
  bool ContainerChanged(PhysicalSize, PhysicalAxes contained_axes);

  void Trace(Visitor*) const;

 private:
  void SetData(PhysicalSize, PhysicalAxes contained_axes);
  bool ResultsChanged() const;

  // TODO(crbug.com/1145970): Don't lean on MediaQueryEvaluator.
  Member<MediaQueryEvaluator> media_query_evaluator_;
  PhysicalSize size_;
  PhysicalAxes contained_axes_;
  HeapHashMap<Member<const ContainerQuery>, bool> results_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CONTAINER_QUERY_EVALUATOR_H_
