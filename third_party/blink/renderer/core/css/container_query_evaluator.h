// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CONTAINER_QUERY_EVALUATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CONTAINER_QUERY_EVALUATOR_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/media_query_evaluator.h"
#include "third_party/blink/renderer/core/css/media_query_exp.h"
#include "third_party/blink/renderer/core/css/style_recalc_change.h"
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
  // Look for a container query container in the shadow-including inclusive
  // ancestor chain of 'starting_element'.
  static Element* FindContainer(Element* starting_element,
                                const ContainerSelector&);
  static bool EvalAndAdd(const Element& matching_element,
                         const StyleRecalcContext&,
                         const ContainerQuery&,
                         MatchResult&);

  // Creates an evaluator with no containment, hence all queries evaluated
  // against it will fail.
  ContainerQueryEvaluator() = default;

  // Used by container relative units (qi, qb, etc).
  double Width() const;
  double Height() const;
  void SetReferencedByUnit() { referenced_by_unit_ = true; }

  enum class Change : uint8_t {
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
                          Element& container,
                          PhysicalSize,
                          PhysicalAxes contained_axes);

  // We may need to update the internal CSSContainerValues of this evaluator
  // when e.g. the rem unit changes.
  void UpdateValuesIfNeeded(Document&, Element& container, StyleRecalcChange);

  void MarkFontDirtyIfNeeded(const ComputedStyle& old_style,
                             const ComputedStyle& new_style);

  void Trace(Visitor*) const;

 private:
  friend class ContainerQueryEvaluatorTest;

  void SetData(Document&,
               Element& container,
               PhysicalSize,
               PhysicalAxes contained_axes);
  void ClearResults(Change change);
  Change ComputeChange() const;
  bool Eval(const ContainerQuery&) const;
  bool Eval(const ContainerQuery&, MediaQueryResultFlags*) const;

  struct Result {
    // Main evaluation result.
    bool value = false;
    // The units that were relevant for the result.
    // See `MediaQueryExpValue::UnitFlags`.
    unsigned unit_flags : MediaQueryExpValue::kUnitFlagsBits;
    // Indicates what we need to invalidate if the result value changes.
    Change change = Change::kNone;
  };

  // Evaluate and add a dependent query to this evaluator. During calls to
  // ContainerChanged, all dependent queries are checked to see if the new
  // size/axis information causes a change in the evaluation result.
  bool EvalAndAdd(const ContainerQuery& query,
                  Change change,
                  MatchResult& match_result);

  Member<MediaQueryEvaluator> media_query_evaluator_;
  PhysicalSize size_;
  PhysicalAxes contained_axes_;
  HeapHashMap<Member<const ContainerQuery>, Result> results_;
  bool referenced_by_unit_ = false;
  // The MediaQueryExpValue::UnitFlags of all queries evaluated against this
  // ContainerQueryEvaluator.
  unsigned unit_flags_ = 0;
  bool font_dirty_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CONTAINER_QUERY_EVALUATOR_H_
