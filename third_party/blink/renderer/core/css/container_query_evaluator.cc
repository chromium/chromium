// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/container_query_evaluator.h"
#include "third_party/blink/renderer/core/css/container_query.h"
#include "third_party/blink/renderer/core/css/css_container_values.h"
#include "third_party/blink/renderer/core/css/resolver/match_result.h"
#include "third_party/blink/renderer/core/css/style_recalc_context.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

#include "third_party/blink/renderer/core/css/media_values_cached.h"

namespace blink {

namespace {

// Produce PhysicalAxes corresponding to the computed container-type.
// Note that this may be different from the *actually* contained axes
// provided to ContainerChanged, since there are multiple sources of
// applied containment (e.g. the 'contain' property itself).
PhysicalAxes ContainerTypeAxes(const ComputedStyle& style) {
  LogicalAxes axes(kLogicalAxisNone);
  if (style.ContainerType() & kContainerTypeInlineSize)
    axes |= LogicalAxes(kLogicalAxisInline);
  if (style.ContainerType() & kContainerTypeBlockSize)
    axes |= LogicalAxes(kLogicalAxisBlock);
  return ToPhysicalAxes(axes, style.GetWritingMode());
}

bool NameMatches(const ComputedStyle& style,
                 const ContainerSelector& container_selector) {
  const AtomicString& name = container_selector.Name();
  return name.IsNull() || (style.ContainerName().Contains(name));
}

bool TypeMatches(const ComputedStyle& style,
                 const ContainerSelector& container_selector) {
  unsigned type = container_selector.Type(style.GetWritingMode());
  return !type || ((style.ContainerType() & type) == type);
}

bool Matches(const ComputedStyle& style,
             const ContainerSelector& container_selector) {
  return NameMatches(style, container_selector) &&
         TypeMatches(style, container_selector);
}

}  // namespace

// static
Element* ContainerQueryEvaluator::FindContainer(
    const StyleRecalcContext& context,
    const ContainerSelector& container_selector) {
  Element* container = context.container;
  if (!container)
    return nullptr;

  // TODO(crbug.com/1213888): Cache results.
  for (Element* element = container; element;
       element = element->ParentOrShadowHostElement()) {
    if (const ComputedStyle* style = element->GetComputedStyle()) {
      if (style->IsContainerForSizeContainerQueries() &&
          Matches(*style, container_selector)) {
        return element;
      }
    }
  }

  return nullptr;
}

bool ContainerQueryEvaluator::EvalAndAdd(const StyleRecalcContext& context,
                                         const ContainerQuery& query,
                                         MatchResult& match_result) {
  Element* container = FindContainer(context, query.Selector());
  if (!container)
    return false;
  ContainerQueryEvaluator* evaluator = container->GetContainerQueryEvaluator();
  if (!evaluator)
    return false;
  Change change = (context.container == container)
                      ? Change::kNearestContainer
                      : Change::kDescendantContainers;
  return evaluator->EvalAndAdd(query, change, match_result);
}

double ContainerQueryEvaluator::Width() const {
  return size_.width.ToDouble();
}

double ContainerQueryEvaluator::Height() const {
  return size_.height.ToDouble();
}

bool ContainerQueryEvaluator::Eval(
    const ContainerQuery& container_query) const {
  return Eval(container_query, MediaQueryEvaluator::Results());
}

bool ContainerQueryEvaluator::Eval(const ContainerQuery& container_query,
                                   MediaQueryEvaluator::Results results) const {
  if (!media_query_evaluator_)
    return false;
  return media_query_evaluator_->Eval(*container_query.query_, results) ==
         KleeneValue::kTrue;
}

bool ContainerQueryEvaluator::EvalAndAdd(const ContainerQuery& query,
                                         Change change,
                                         MatchResult& match_result) {
  MediaQueryResultList viewport_dependent;
  unsigned unit_flags = MediaQueryExpValue::UnitFlags::kNone;

  bool result = Eval(query, {&viewport_dependent, nullptr, &unit_flags});
  if (!viewport_dependent.IsEmpty())
    match_result.SetDependsOnViewportContainerQueries();
  if (unit_flags & MediaQueryExpValue::UnitFlags::kRootFontRelative)
    match_result.SetDependsOnRemContainerQueries();
  if (unit_flags & MediaQueryExpValue::UnitFlags::kFontRelative)
    depends_on_font_ = true;
  results_.Set(&query, Result{result, change});
  return result;
}

ContainerQueryEvaluator::Change ContainerQueryEvaluator::ContainerChanged(
    Document& document,
    const ComputedStyle& style,
    PhysicalSize size,
    PhysicalAxes contained_axes) {
  if (size_ == size && contained_axes_ == contained_axes && !font_dirty_)
    return Change::kNone;

  SetData(document, style, size, contained_axes);
  font_dirty_ = false;

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
                                      const ComputedStyle& style,
                                      PhysicalSize size,
                                      PhysicalAxes contained_axes) {
  size_ = size;
  contained_axes_ = contained_axes;

  absl::optional<double> width;
  absl::optional<double> height;

  // An axis is "supported" only when it appears in the computed value of
  // 'container-type', and when containment is actually applied for that axis.
  //
  // See IsEligibleForSizeContainment (and similar).
  PhysicalAxes supported_axes = ContainerTypeAxes(style) & contained_axes;

  if ((supported_axes & PhysicalAxes(kPhysicalAxisHorizontal)) !=
      PhysicalAxes(kPhysicalAxisNone)) {
    width = size.width.ToDouble();
  }

  if ((supported_axes & PhysicalAxes(kPhysicalAxisVertical)) !=
      PhysicalAxes(kPhysicalAxisNone)) {
    height = size.height.ToDouble();
  }

  auto* query_values =
      MakeGarbageCollected<CSSContainerValues>(document, style, width, height);
  media_query_evaluator_ =
      MakeGarbageCollected<MediaQueryEvaluator>(query_values);
}

void ContainerQueryEvaluator::ClearResults() {
  results_.clear();
  referenced_by_unit_ = false;
  depends_on_font_ = false;
}

ContainerQueryEvaluator::Change ContainerQueryEvaluator::ComputeChange() const {
  Change change = Change::kNone;

  if (referenced_by_unit_)
    return Change::kDescendantContainers;

  for (const auto& result : results_) {
    if (Eval(*result.key) != result.value.value)
      change = std::max(change, result.value.change);
  }

  return change;
}

void ContainerQueryEvaluator::MarkFontDirtyIfNeeded(
    const ComputedStyle& old_style,
    const ComputedStyle& new_style) {
  if (!depends_on_font_ || font_dirty_)
    return;
  font_dirty_ = old_style.GetFont() != new_style.GetFont();
}

}  // namespace blink
