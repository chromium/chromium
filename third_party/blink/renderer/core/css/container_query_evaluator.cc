// Copyright 2020 The Chromium Authors
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
  if (name.IsNull())
    return true;
  if (const ScopedCSSNameList* container_name = style.ContainerName()) {
    const HeapVector<Member<const ScopedCSSName>>& names =
        container_name->GetNames();
    for (auto scoped_name : names) {
      if (scoped_name->GetName() == name) {
        // TODO(crbug.com/1382790): Should only match if the name's tree scope
        // is an inclusive-ancestor tree scope of the selector source.
        return true;
      }
    }
  }
  return false;
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

Element* CachedContainer(Element* starting_element,
                         const ContainerSelector& container_selector,
                         ContainerSelectorCache& container_selector_cache) {
  ContainerSelectorCache::AddResult add_result =
      container_selector_cache.insert(container_selector, nullptr);

  if (add_result.is_new_entry) {
    add_result.stored_value->value = ContainerQueryEvaluator::FindContainer(
        starting_element, container_selector);
  }

  return add_result.stored_value->value.Get();
}

}  // namespace

// static
Element* ContainerQueryEvaluator::FindContainer(
    Element* starting_element,
    const ContainerSelector& container_selector) {
  // TODO(crbug.com/1213888): Cache results.
  for (Element* element = starting_element; element;
       element = element->ParentOrShadowHostElement()) {
    if (const ComputedStyle* style = element->GetComputedStyle()) {
      if (style->StyleType() == kPseudoIdNone &&
          Matches(*style, container_selector)) {
        return element;
      }
    }
  }

  return nullptr;
}

bool ContainerQueryEvaluator::EvalAndAdd(
    Element* style_container_candidate,
    const StyleRecalcContext& context,
    const ContainerQuery& query,
    ContainerSelectorCache& container_selector_cache,
    MatchResult& match_result) {
  const ContainerSelector& selector = query.Selector();
  bool selects_size = selector.SelectsSizeContainers();
  bool selects_style = selector.SelectsStyleContainers();
  if (!selects_size && !selects_style)
    return false;

  Element* starting_element =
      selects_size ? context.container : style_container_candidate;
  Element* container = CachedContainer(starting_element, query.Selector(),
                                       container_selector_cache);
  if (!container)
    return false;

  ContainerQueryEvaluator* evaluator = container->GetContainerQueryEvaluator();
  if (!evaluator) {
    if (selects_size || !selects_style)
      return false;
    evaluator = &container->EnsureContainerQueryEvaluator();
    evaluator->SetData(container->GetDocument(), *container, PhysicalSize(),
                       kPhysicalAxisNone);
  }
  Change change = starting_element == container ? Change::kNearestContainer
                                                : Change::kDescendantContainers;
  return evaluator->EvalAndAdd(query, change, match_result);
}

double ContainerQueryEvaluator::Width() const {
  return size_.width.ToDouble();
}

double ContainerQueryEvaluator::Height() const {
  return size_.height.ToDouble();
}

ContainerQueryEvaluator::Result ContainerQueryEvaluator::Eval(
    const ContainerQuery& container_query) const {
  if (!media_query_evaluator_)
    return Result();

  MediaQueryResultFlags result_flags;
  bool value =
      (media_query_evaluator_->Eval(*container_query.query_, &result_flags) ==
       KleeneValue::kTrue);

  Result result;
  result.value = value;
  result.unit_flags = result_flags.unit_flags;
  return result;
}

bool ContainerQueryEvaluator::EvalAndAdd(const ContainerQuery& query,
                                         Change change,
                                         MatchResult& match_result) {
  HeapHashMap<Member<const ContainerQuery>, Result>::AddResult entry =
      results_.insert(&query, Result());

  Result& result = entry.stored_value->value;

  // We can only use the cached values when evaluating queries whose results
  // would have been cleared by [Size,Style]ContainerChanged. The following
  // represents dependencies on external circumstance that can change without
  // ContainerQueryEvaluator being notified.
  bool use_cached =
      (result.unit_flags & (MediaQueryExpValue::UnitFlags::kRootFontRelative |
                            MediaQueryExpValue::UnitFlags::kDynamicViewport |
                            MediaQueryExpValue::UnitFlags::kStaticViewport |
                            MediaQueryExpValue::UnitFlags::kContainer)) == 0;
  bool has_cached = !entry.is_new_entry;

  if (has_cached && use_cached) {
    // Verify that the cached result is equal to the value we would get
    // had we Eval'ed in full.
#if EXPENSIVE_DCHECKS_ARE_ON()
    Result actual = Eval(query);

    // This ignores `change`, because it's not actually part of Eval's result.
    DCHECK_EQ(result.value, actual.value);
    DCHECK_EQ(result.unit_flags, actual.unit_flags);
#endif  // EXPENSIVE_DCHECKS_ARE_ON()
  } else {
    result = Eval(query);
  }

  // Store the most severe `Change` seen.
  result.change = std::max(result.change, change);

  if (result.unit_flags & MediaQueryExpValue::UnitFlags::kDynamicViewport)
    match_result.SetDependsOnDynamicViewportUnits();
  // Note that container-relative units *may* fall back to the small viewport,
  // hence we also set the DependsOnStaticViewportUnits flag when in that case.
  if (result.unit_flags & (MediaQueryExpValue::UnitFlags::kStaticViewport |
                           MediaQueryExpValue::UnitFlags::kContainer)) {
    match_result.SetDependsOnStaticViewportUnits();
  }
  if (result.unit_flags & MediaQueryExpValue::UnitFlags::kRootFontRelative)
    match_result.SetDependsOnRemContainerQueries();
  if (!depends_on_style_)
    depends_on_style_ = query.Selector().SelectsStyleContainers();
  unit_flags_ |= result.unit_flags;

  return result.value;
}

ContainerQueryEvaluator::Change ContainerQueryEvaluator::SizeContainerChanged(
    Document& document,
    Element& container,
    PhysicalSize size,
    PhysicalAxes contained_axes) {
  if (size_ == size && contained_axes_ == contained_axes && !font_dirty_)
    return Change::kNone;

  SetData(document, container, size, contained_axes);
  font_dirty_ = false;

  Change change = ComputeSizeChange();

  if (change != Change::kNone)
    ClearResults(change, kSizeContainer);

  return change;
}

ContainerQueryEvaluator::Change
ContainerQueryEvaluator::StyleContainerChanged() {
  if (!depends_on_style_)
    return Change::kNone;

  Change change = ComputeStyleChange();

  if (change != Change::kNone)
    ClearResults(change, kStyleContainer);

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

void ContainerQueryEvaluator::ClearResults(Change change,
                                           ContainerType container_type) {
  if (change == Change::kNone)
    return;
  if (change == Change::kDescendantContainers) {
    if (container_type == kSizeContainer)
      referenced_by_unit_ = false;
    else
      depends_on_style_ = false;
  }
  unit_flags_ = 0;

  HeapHashMap<Member<const ContainerQuery>, Result> new_results;
  for (const auto& pair : results_) {
    if (pair.value.change <= change &&
        ((container_type == kSizeContainer &&
          pair.key->Selector().SelectsSizeContainers()) ||
         (container_type == kStyleContainer &&
          pair.key->Selector().SelectsStyleContainers()))) {
      continue;
    }
    new_results.Set(pair.key, pair.value);
    unit_flags_ |= pair.value.unit_flags;
  }

  std::swap(new_results, results_);
}

ContainerQueryEvaluator::Change ContainerQueryEvaluator::ComputeSizeChange()
    const {
  if (referenced_by_unit_)
    return Change::kDescendantContainers;

  Change change = Change::kNone;

  for (const auto& result : results_) {
    const ContainerQuery& query = *result.key;
    if (!query.Selector().SelectsSizeContainers())
      continue;
    if (Eval(query).value != result.value.value)
      change = std::max(result.value.change, change);
  }

  return change;
}

ContainerQueryEvaluator::Change ContainerQueryEvaluator::ComputeStyleChange()
    const {
  Change change = Change::kNone;

  for (const auto& result : results_) {
    const ContainerQuery& query = *result.key;
    if (!query.Selector().SelectsStyleContainers())
      continue;
    if (Eval(query).value == result.value.value)
      continue;
    change = std::max(result.value.change, change);
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
