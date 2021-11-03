// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/container_query_evaluator.h"
#include "third_party/blink/renderer/core/css/container_query.h"
#include "third_party/blink/renderer/core/css/css_container_values.h"
#include "third_party/blink/renderer/core/css/resolver/match_result.h"
#include "third_party/blink/renderer/core/css/style_recalc.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

#include "third_party/blink/renderer/core/css/media_values_cached.h"

namespace blink {

// static
Element* ContainerQueryEvaluator::FindContainer(
    const StyleRecalcContext& context,
    const AtomicString& container_name) {
  Element* container = context.container;
  if (!container)
    return nullptr;

  if (container_name == g_null_atom)
    return container;

  // TODO(crbug.com/1213888): Cache results.
  for (Element* element = container; element;
       element = LayoutTreeBuilderTraversal::ParentElement(*element)) {
    if (const ComputedStyle* style = element->GetComputedStyle()) {
      if (style->IsContainerForContainerQueries() &&
          style->ContainerName() == container_name)
        return element;
    }
  }

  return nullptr;
}

namespace {

bool IsSufficientlyContained(PhysicalAxes contained_axes,
                             PhysicalAxes queried_axes) {
  return (contained_axes & queried_axes) == queried_axes;
}

}  // namespace

double ContainerQueryEvaluator::Width() const {
  return size_.width.ToDouble();
}

double ContainerQueryEvaluator::Height() const {
  return size_.height.ToDouble();
}

bool ContainerQueryEvaluator::Eval(
    const ContainerQuery& container_query,
    MediaQueryResultList* viewport_dependent) const {
  if (container_query.QueriedAxes() == PhysicalAxes(kPhysicalAxisNone))
    return false;
  if (!IsSufficientlyContained(contained_axes_, container_query.QueriedAxes()))
    return false;
  DCHECK(media_query_evaluator_);
  return media_query_evaluator_->Eval(*container_query.media_queries_,
                                      {viewport_dependent, nullptr});
}

void ContainerQueryEvaluator::Add(const ContainerQuery& query, bool result) {
  results_.Set(&query, result);
}

bool ContainerQueryEvaluator::EvalAndAdd(const ContainerQuery& query,
                                         MatchResult& match_result) {
  MediaQueryResultList viewport_dependent;
  bool result = Eval(query, &viewport_dependent);
  if (!viewport_dependent.IsEmpty())
    match_result.SetDependsOnViewportContainerQueries();
  Add(query, result);
  return result;
}

ContainerQueryEvaluator::Change ContainerQueryEvaluator::ContainerChanged(
    Document& document,
    PhysicalSize size,
    PhysicalAxes contained_axes) {
  if (size_ == size && contained_axes_ == contained_axes)
    return Change::kNone;

  SetData(document, size, contained_axes);

  Change change = ComputeChange();

  // We can clear the results here because we will always recaculate the style
  // of all descendants which depend on this evaluator whenever we return
  // something other than kNone from this function, so the results will always
  // be repopulated.
  if (change != Change::kNone)
    ClearResults();

  return change;
}

void ContainerQueryEvaluator::Trace(Visitor* visitor) const {
  visitor->Trace(media_query_evaluator_);
  visitor->Trace(results_);
}

void ContainerQueryEvaluator::SetData(Document& document,
                                      PhysicalSize size,
                                      PhysicalAxes contained_axes) {
  size_ = size;
  contained_axes_ = contained_axes;

  auto* query_values = MakeGarbageCollected<CSSContainerValues>(
      document, size.width.ToDouble(), size.height.ToDouble());
  media_query_evaluator_ =
      MakeGarbageCollected<MediaQueryEvaluator>(query_values);
}

void ContainerQueryEvaluator::ClearResults() {
  results_.clear();
  referenced_by_unit_ = false;
}

ContainerQueryEvaluator::Change ContainerQueryEvaluator::ComputeChange() const {
  Change change = Change::kNone;

  if (referenced_by_unit_)
    return Change::kDescendantContainers;

  for (const auto& result : results_) {
    if (Eval(*result.key) != result.value) {
      change = std::max(change, result.key->Name() == g_null_atom
                                    ? Change::kNearestContainer
                                    : Change::kDescendantContainers);
    }
  }

  return change;
}

}  // namespace blink
