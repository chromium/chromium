// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CONTAINER_QUERY_EVALUATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CONTAINER_QUERY_EVALUATOR_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/media_query_evaluator.h"
#include "third_party/blink/renderer/core/layout/geometry/axis.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_size.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class ComputedStyle;
class ContainerQuery;
class Document;
class Element;
class MatchResult;
class StyleRecalcContext;
class ContainerSelector;

class CORE_EXPORT ContainerQueryEvaluator final
    : public GarbageCollected<ContainerQueryEvaluator> {
 public:
  static Element* FindContainer(const StyleRecalcContext& context,
                                const ContainerSelector&);

  // Creates an evaluator with no containment, hence all queries evaluated
  // against it will fail.
  ContainerQueryEvaluator() = default;

  // Used by container relative units (qi, qb, etc).
  double Width() const;
  double Height() const;
  void SetReferencedByUnit() { referenced_by_unit_ = true; }

  // Add a dependent query to this evaluator. During calls to ContainerChanged,
  // all dependent queries are checked to see if the new size/axis information
  // causes a change in the evaluation result.
  void Add(const ContainerQuery&, bool result);

  bool EvalAndAdd(const ContainerQuery& query, MatchResult& match_result);

  enum class Change {
    // The update has no effect on the evaluation of queries associated with
    // this evaluator, and therefore we do not need to perform style recalc of
    // any elements which depend on this evaluator.
    kNone,
    // The update can only affect elements for which this container is the
    // nearest container. In other words, we do not need to recalculate style
    // for elements in nested containers.
    kNearestContainer,
    // The update can affect elements within this container, and also in
    // descendant containers.
    kDescendantContainers,
  };

  // Update the size/axis information of the evaluator.
  //
  // Dependent queries are cleared when kUnnamed/kNamed is returned (and left
  // unchanged otherwise).
  Change ContainerChanged(Document&,
                          const ComputedStyle&,
                          PhysicalSize,
                          PhysicalAxes contained_axes);

  void MarkFontDirtyIfNeeded(const ComputedStyle& old_style,
                             const ComputedStyle& new_style);

  void Trace(Visitor*) const;

 private:
  friend class ContainerQueryEvaluatorTest;

  void SetData(Document&,
               const ComputedStyle&,
               PhysicalSize,
               PhysicalAxes contained_axes);
  void ClearResults();
  Change ComputeChange() const;
  bool Eval(const ContainerQuery&) const;
  bool Eval(const ContainerQuery&, MediaQueryEvaluator::Results) const;

  // TODO(crbug.com/1145970): Don't lean on MediaQueryEvaluator.
  Member<MediaQueryEvaluator> media_query_evaluator_;
  PhysicalSize size_;
  PhysicalAxes contained_axes_;
  HeapHashMap<Member<const ContainerQuery>, bool> results_;
  bool referenced_by_unit_ = false;
  bool depends_on_font_ = false;
  bool font_dirty_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CONTAINER_QUERY_EVALUATOR_H_
