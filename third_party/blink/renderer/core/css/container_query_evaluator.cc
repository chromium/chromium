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
  LogicalAxes axes = kLogicalAxisNone;
  if (style.ContainerType() & kContainerTypeInlineSize)
    axes |= kLogicalAxisInline;
  if (style.ContainerType() & kContainerTypeBlockSize)
    axes |= kLogicalAxisBlock;
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
    Element* context_element,
    const ContainerSelector& container_selector) {
  // TODO(crbug.com/1213888): Cache results.
  for (Element* element = context_element; element;
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
  Element* container = FindContainer(context.container, query.Selector());
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
  return Eval(container_query, nullptr /* result_flags */);
}

bool ContainerQueryEvaluator::Eval(const ContainerQuery& container_query,
                                   MediaQueryResultFlags* result_flags) const {
  if (!media_query_evaluator_)
    return false;
  return media_query_evaluator_->Eval(*container_query.query_, result_flags) ==
         KleeneValue::kTrue;
}

bool ContainerQueryEvaluator::EvalAndAdd(const ContainerQuery& query,
                                         Change change,
                                         MatchResult& match_result) {
  MediaQueryResultFlags result_flags;
  bool result = Eval(query, &result_flags);
  unsigned unit_flags = result_flags.unit_flags;
  if (unit_flags & MediaQueryExpValue::UnitFlags::kDynamicViewport)
    match_result.SetDependsOnDynamicViewportUnits();
  // Note that container-relative units *may* fall back to the small viewport,
  // hence we also set the DependsOnStaticViewportUnits flag when in that case.
  if (unit_flags & (MediaQueryExpValue::UnitFlags::kStaticViewport |
                    MediaQueryExpValue::UnitFlags::kContainer)) {
    match_result.SetDependsOnStaticViewportUnits();
  }
  if (unit_flags & MediaQueryExpValue::UnitFlags::kRootFontRelative)
    match_result.SetDependsOnRemContainerQueries();
  results_.Set(&query, Result{result, unit_flags, change});
  unit_flags_ |= unit_flags;
  return result;
}

ContainerQueryEvaluator::Change ContainerQueryEvaluator::ContainerChanged(
    Document& document,
    Element& container,
    PhysicalSize size,
    PhysicalAxes contained_axes) {
  if (size_ == size && contained_axes_ == contained_axes && !font_dirty_)
    return Change::kNone;

  SetData(document, container, size, contained_axes);
  font_dirty_ = false;

  Change change = ComputeChange();

  if (change != Change::kNone)
    ClearResults(change);

  return change;
}

void ContainerQueryEvaluator::Trace(Visitor* visitor) const {
  visitor->Trace(media_query_evaluator_);
  visitor->Trace(results_);
}

void ContainerQueryEvaluator::SetData(Document& document,
                                      Element& container,
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
  PhysicalAxes supported_axes =
      ContainerTypeAxes(container.ComputedStyleRef()) & contained_axes;

  if ((supported_axes & PhysicalAxes(kPhysicalAxisHorizontal)) !=
      PhysicalAxes(kPhysicalAxisNone)) {
    width = size.width.ToDouble();
  }

  if ((supported_axes & PhysicalAxes(kPhysicalAxisVertical)) !=
      PhysicalAxes(kPhysicalAxisNone)) {
    height = size.height.ToDouble();
  }

  auto* query_values = MakeGarbageCollected<CSSContainerValues>(
      document, container, width, height);
  media_query_evaluator_ =
      MakeGarbageCollected<MediaQueryEvaluator>(query_values);
}

void ContainerQueryEvaluator::ClearResults(Change change) {
  switch (change) {
    case Change::kNone:
      NOTREACHED();
      break;
    case Change::kNearestContainer: {
      DCHECK(!referenced_by_unit_);
      // We are going to recalculate the style of all descendants which depend
      // on this container, *excluding* those that exist within nested
      // containers. Therefore all entries with `change` greater than
      // kNearestContainer need to remain.
      unit_flags_ = 0;
      HeapHashMap<Member<const ContainerQuery>, Result> new_results;
      for (const auto& pair : results_) {
        if (pair.value.change > change) {
          new_results.Set(pair.key, pair.value);
          unit_flags_ |= pair.value.unit_flags;
        }
      }
      std::swap(new_results, results_);
      break;
    }
    case Change::kDescendantContainers: {
      // We are going to recalculate the style of all descendants which
      // depend on this container, *including* those that exist within nested
      // containers. Therefore all results will be repopulated, and we can clear
      // everything.
      results_.clear();
      referenced_by_unit_ = false;
      unit_flags_ = 0;
      break;
    }
  }
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

void ContainerQueryEvaluator::UpdateValuesIfNeeded(Document& document,
                                                   Element& container,
                                                   StyleRecalcChange change) {
  if (!media_query_evaluator_)
    return;
  unsigned changed_flags = 0;
  if (change.RemUnitsMaybeChanged())
    changed_flags |= MediaQueryExpValue::kRootFontRelative;
  if (change.ContainerRelativeUnitsMaybeChanged())
    changed_flags |= MediaQueryExpValue::kContainer;
  if (!(unit_flags_ & changed_flags))
    return;
  const MediaValues& existing_values = media_query_evaluator_->GetMediaValues();
  auto* query_values = MakeGarbageCollected<CSSContainerValues>(
      document, container, existing_values.Width(), existing_values.Height());
  media_query_evaluator_ =
      MakeGarbageCollected<MediaQueryEvaluator>(query_values);
}

void ContainerQueryEvaluator::MarkFontDirtyIfNeeded(
    const ComputedStyle& old_style,
    const ComputedStyle& new_style) {
  if (!(unit_flags_ & MediaQueryExpValue::kFontRelative) || font_dirty_)
    return;
  font_dirty_ = old_style.GetFont() != new_style.GetFont();
}

}  // namespace blink
