// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/container_query_evaluator.h"

#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/core/css/container_query.h"
#include "third_party/blink/renderer/core/css/css_container_values.h"
#include "third_party/blink/renderer/core/css/media_values_cached.h"
#include "third_party/blink/renderer/core/css/resolver/match_result.h"
#include "third_party/blink/renderer/core/css/snapped_query_scroll_snapshot.h"
#include "third_party/blink/renderer/core/css/stuck_query_scroll_snapshot.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/css/style_recalc_context.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

namespace {

// Produce PhysicalAxes corresponding to the computed container-type.
// Note that this may be different from the *actually* contained axes
// provided to ContainerChanged, since there are multiple sources of
// applied containment (e.g. the 'contain' property itself).
PhysicalAxes ContainerTypeAxes(const ComputedStyle& style) {
  LogicalAxes axes = kLogicalAxesNone;
  if (style.ContainerType() & kContainerTypeInlineSize) {
    axes |= kLogicalAxesInline;
  }
  if (style.ContainerType() & kContainerTypeBlockSize) {
    axes |= kLogicalAxesBlock;
  }
  return ToPhysicalAxes(axes, style.GetWritingMode());
}

bool NameMatches(const ComputedStyle& style,
                 const ContainerSelector& container_selector,
                 const TreeScope* selector_tree_scope) {
  const AtomicString& name = container_selector.Name();
  if (name.IsNull()) {
    return true;
  }
  if (const ScopedCSSNameList* container_name = style.ContainerName()) {
    const HeapVector<Member<const ScopedCSSName>>& names =
        container_name->GetNames();
    for (auto scoped_name : names) {
      if (scoped_name->GetName() == name) {
        const TreeScope* name_tree_scope = scoped_name->GetTreeScope();
        if (!name_tree_scope || !selector_tree_scope) {
          // Either the container-name or @container have a UA or User origin.
          // In that case always match the name regardless of the other one's
          // origin.
          return true;
        }
        // Match a tree-scoped container name if the container-name
        // declaration's tree scope is an inclusive ancestor of the @container
        // rule's tree scope.
        for (const TreeScope* match_scope = selector_tree_scope; match_scope;
             match_scope = match_scope->ParentTreeScope()) {
          if (match_scope == name_tree_scope) {
            return true;
          }
        }
      }
    }
  }
  return false;
}

bool TypeMatches(const ComputedStyle& style,
                 const ContainerSelector& container_selector) {
  DCHECK(!container_selector.HasUnknownFeature());
  unsigned type = container_selector.Type(style.GetWritingMode());
  return !type || ((style.ContainerType() & type) == type);
}

bool Matches(const ComputedStyle& style,
             const ContainerSelector& container_selector,
             const TreeScope* selector_tree_scope) {
  return TypeMatches(style, container_selector) &&
         NameMatches(style, container_selector, selector_tree_scope);
}

Element* CachedContainer(Element* starting_element,
                         const ContainerSelector& container_selector,
                         const TreeScope* selector_tree_scope,
                         ContainerSelectorCache& container_selector_cache) {
  auto it =
      container_selector_cache.Find<ScopedContainerSelectorHashTranslator>(
          ScopedContainerSelector(container_selector, selector_tree_scope));
  if (it != container_selector_cache.end()) {
    return it->value.Get();
  }
  Element* container = ContainerQueryEvaluator::FindContainer(
      starting_element, container_selector, selector_tree_scope);
  container_selector_cache.insert(MakeGarbageCollected<ScopedContainerSelector>(
                                      container_selector, selector_tree_scope),
                                  container);
  return container;
}

}  // namespace

ContainerQueryEvaluator::ContainerQueryEvaluator(Element& container) {
  auto* query_values = MakeGarbageCollected<CSSContainerValues>(
      container.GetDocument(), container, std::nullopt, std::nullopt,
      ContainerStuckPhysical::kNo, ContainerStuckPhysical::kNo,
      static_cast<ContainerSnappedFlags>(ContainerSnapped::kNone));
  media_query_evaluator_ =
      MakeGarbageCollected<MediaQueryEvaluator>(query_values);
}

// static
Element* ContainerQueryEvaluator::ParentContainerCandidateElement(
    Element& element) {
  if (RuntimeEnabledFeatures::CSSFlatTreeContainerEnabled()) {
    return FlatTreeTraversal::ParentElement(element);
  }
  return element.ParentOrShadowHostElement();
}

// static
Element* ContainerQueryEvaluator::FindContainer(
    Element* starting_element,
    const ContainerSelector& container_selector,
    const TreeScope* selector_tree_scope) {
  // TODO(crbug.com/1213888): Cache results.
  for (Element* element = starting_element; element;
       element = ParentContainerCandidateElement(*element)) {
    if (const ComputedStyle* style = element->GetComputedStyle()) {
      if (style->StyleType() == kPseudoIdNone) {
        if (Matches(*style, container_selector, selector_tree_scope)) {
          return element;
        }
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
  if (selector.HasUnknownFeature()) {
    return false;
  }
  bool selects_size = selector.SelectsSizeContainers();
  bool selects_style = selector.SelectsStyleContainers();
  bool selects_state = selector.SelectsStateContainers();
  if (!selects_size && !selects_style && !selects_state) {
    return false;
  }

  if (selects_size) {
    match_result.SetDependsOnSizeContainerQueries();
  }
  if (selects_style) {
    match_result.SetDependsOnStyleContainerQueries();
  }
  if (selects_state) {
    match_result.SetDependsOnStateContainerQueries();
  }

  Element* starting_element =
      selects_size ? context.container : style_container_candidate;
  if (Element* container = CachedContainer(starting_element, query.Selector(),
                                           match_result.CurrentTreeScope(),
                                           container_selector_cache)) {
    Change change = starting_element == container
                        ? Change::kNearestContainer
                        : Change::kDescendantContainers;
    return container->EnsureContainerQueryEvaluator().EvalAndAdd(query, change,
                                                                 match_result);
  }
  return false;
}

std::optional<double> ContainerQueryEvaluator::Width() const {
  CHECK(media_query_evaluator_);
  return media_query_evaluator_->GetMediaValues().Width();
}

std::optional<double> ContainerQueryEvaluator::Height() const {
  CHECK(media_query_evaluator_);
  return media_query_evaluator_->GetMediaValues().Height();
}

ContainerQueryEvaluator::Result ContainerQueryEvaluator::Eval(
    const ContainerQuery& container_query) const {
  CHECK(media_query_evaluator_);

  if (container_query.Selector().HasUnknownFeature()) {
    Element* container = ContainerElement();
    CHECK(container);
    container->GetDocument().CountUse(WebFeature::kContainerQueryEvalUnknown);
  }

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

  if (result.unit_flags & MediaQueryExpValue::UnitFlags::kDynamicViewport) {
    match_result.SetDependsOnDynamicViewportUnits();
  }
  // Note that container-relative units *may* fall back to the small viewport,
  // hence we also set the DependsOnStaticViewportUnits flag when in that case.
  if (result.unit_flags & (MediaQueryExpValue::UnitFlags::kStaticViewport |
                           MediaQueryExpValue::UnitFlags::kContainer)) {
    match_result.SetDependsOnStaticViewportUnits();
  }
  if (result.unit_flags & MediaQueryExpValue::UnitFlags::kRootFontRelative) {
    match_result.SetDependsOnRootFontContainerQueries();
  }
  if (!depends_on_style_) {
    depends_on_style_ = query.Selector().SelectsStyleContainers();
  }
  if (!depends_on_stuck_) {
    depends_on_stuck_ = query.Selector().SelectsStickyContainers();
    if (depends_on_stuck_ && !stuck_snapshot_) {
      CHECK(media_query_evaluator_);
      Element* container_element = ContainerElement();
      CHECK(container_element);
      stuck_snapshot_ =
          MakeGarbageCollected<StuckQueryScrollSnapshot>(*container_element);
    }
  }
  if (!depends_on_snapped_) {
    depends_on_snapped_ = query.Selector().SelectsSnapContainers();
    if (depends_on_snapped_) {
      Element* container_element = ContainerElement();
      CHECK(container_element);
      if (const LayoutObject* layout_object =
              container_element->GetLayoutObject()) {
        if (const LayoutBox* snap_container =
                layout_object->ContainingScrollContainer()) {
          if (PaintLayerScrollableArea* scrollable_area =
                  snap_container->GetScrollableArea()) {
            scrollable_area->EnsureSnappedQueryScrollSnapshot();
          }
        }
      }
    }
  }
  unit_flags_ |= result.unit_flags;

  return result.value;
}

ContainerQueryEvaluator::Change ContainerQueryEvaluator::SizeContainerChanged(
    PhysicalSize size,
    PhysicalAxes contained_axes) {
  if (size_ == size && contained_axes_ == contained_axes && !font_dirty_) {
    return Change::kNone;
  }

  UpdateContainerSize(size, contained_axes);
  font_dirty_ = false;

  Change change = ComputeSizeChange();

  if (change != Change::kNone) {
    ClearResults(change, kSizeContainer);
  }

  return change;
}

void ContainerQueryEvaluator::SetPendingSnappedStateFromScrollSnapshot(
    const SnappedQueryScrollSnapshot& snapshot) {
  Element* container = ContainerElement();
  pending_snapped_ =
      static_cast<ContainerSnappedFlags>(ContainerSnapped::kNone);
  if (snapshot.GetSnappedTargetX() == container) {
    pending_snapped_ |=
        static_cast<ContainerSnappedFlags>(ContainerSnapped::kX);
  }
  if (snapshot.GetSnappedTargetY() == container) {
    pending_snapped_ |=
        static_cast<ContainerSnappedFlags>(ContainerSnapped::kY);
  }

  if (pending_snapped_ != snapped_) {
    // TODO(crbug.com/40279568): The kLocalStyleChange is not necessary for the
    // container itself, but it is a way to reach reach ApplyScrollState() in
    // Element::RecalcOwnStyle() for the next lifecycle update.
    container->SetNeedsStyleRecalc(kLocalStyleChange,
                                   StyleChangeReasonForTracing::Create(
                                       style_change_reason::kScrollTimeline));
  }
}

ContainerQueryEvaluator::Change ContainerQueryEvaluator::ApplyScrollState() {
  Change change = Change::kNone;
  if (stuck_snapshot_) {
    change = StickyContainerChanged(stuck_snapshot_->StuckHorizontal(),
                                    stuck_snapshot_->StuckVertical());
  }
  Change snap_change = SnapContainerChanged(pending_snapped_);
  change = std::max(change, snap_change);
  return change;
}

ContainerQueryEvaluator::Change ContainerQueryEvaluator::StickyContainerChanged(
    ContainerStuckPhysical stuck_horizontal,
    ContainerStuckPhysical stuck_vertical) {
  if (stuck_horizontal_ == stuck_horizontal &&
      stuck_vertical_ == stuck_vertical) {
    return Change::kNone;
  }

  UpdateContainerStuck(stuck_horizontal, stuck_vertical);
  Change change = ComputeStickyChange();
  if (change != Change::kNone) {
    ClearResults(change, kStickyContainer);
  }

  return change;
}

ContainerQueryEvaluator::Change ContainerQueryEvaluator::SnapContainerChanged(
    ContainerSnappedFlags snapped) {
  if (snapped_ == snapped) {
    return Change::kNone;
  }

  UpdateContainerSnapped(snapped);
  Change change = ComputeSnapChange();
  if (change != Change::kNone) {
    ClearResults(change, kSnapContainer);
  }

  return change;
}

ContainerQueryEvaluator::Change
ContainerQueryEvaluator::StyleContainerChanged() {
  if (!depends_on_style_) {
    return Change::kNone;
  }

  Change change = ComputeStyleChange();

  if (change != Change::kNone) {
    ClearResults(change, kStyleContainer);
  }

  return change;
}

void ContainerQueryEvaluator::Trace(Visitor* visitor) const {
  visitor->Trace(media_query_evaluator_);
  visitor->Trace(results_);
  visitor->Trace(stuck_snapshot_);
}

void ContainerQueryEvaluator::UpdateContainerSize(PhysicalSize size,
                                                  PhysicalAxes contained_axes) {
  size_ = size;
  contained_axes_ = contained_axes;

  std::optional<double> width;
  std::optional<double> height;

  const MediaValues& existing_values = media_query_evaluator_->GetMediaValues();
  Element* container = existing_values.ContainerElement();

  // An axis is "supported" only when it appears in the computed value of
  // 'container-type', and when containment is actually applied for that axis.
  //
  // See IsEligibleForSizeContainment (and similar).
  PhysicalAxes supported_axes =
      ContainerTypeAxes(container->ComputedStyleRef()) & contained_axes;

  if ((supported_axes & PhysicalAxes(kPhysicalAxesHorizontal)) !=
      PhysicalAxes(kPhysicalAxesNone)) {
    width = size.width.ToDouble();
  }

  if ((supported_axes & PhysicalAxes(kPhysicalAxesVertical)) !=
      PhysicalAxes(kPhysicalAxesNone)) {
    height = size.height.ToDouble();
  }

  auto* query_values = MakeGarbageCollected<CSSContainerValues>(
      container->GetDocument(), *container, width, height,
      existing_values.StuckHorizontal(), existing_values.StuckVertical(),
      existing_values.SnappedFlags());
  media_query_evaluator_ =
      MakeGarbageCollected<MediaQueryEvaluator>(query_values);
}

void ContainerQueryEvaluator::UpdateContainerStuck(
    ContainerStuckPhysical stuck_horizontal,
    ContainerStuckPhysical stuck_vertical) {
  stuck_horizontal_ = stuck_horizontal;
  stuck_vertical_ = stuck_vertical;

  const MediaValues& existing_values = media_query_evaluator_->GetMediaValues();
  Element* container = existing_values.ContainerElement();

  auto* query_values = MakeGarbageCollected<CSSContainerValues>(
      container->GetDocument(), *container, existing_values.Width(),
      existing_values.Height(), stuck_horizontal, stuck_vertical,
      existing_values.SnappedFlags());
  media_query_evaluator_ =
      MakeGarbageCollected<MediaQueryEvaluator>(query_values);
}

void ContainerQueryEvaluator::UpdateContainerSnapped(
    ContainerSnappedFlags snapped) {
  snapped_ = snapped;

  const MediaValues& existing_values = media_query_evaluator_->GetMediaValues();
  Element* container = existing_values.ContainerElement();

  auto* query_values = MakeGarbageCollected<CSSContainerValues>(
      container->GetDocument(), *container, existing_values.Width(),
      existing_values.Height(), existing_values.StuckHorizontal(),
      existing_values.StuckVertical(), snapped);
  media_query_evaluator_ =
      MakeGarbageCollected<MediaQueryEvaluator>(query_values);
}

void ContainerQueryEvaluator::ClearResults(Change change,
                                           ContainerType container_type) {
  if (change == Change::kNone) {
    return;
  }
  if (change == Change::kDescendantContainers) {
    if (container_type == kSizeContainer) {
      referenced_by_unit_ = false;
    } else {
      depends_on_style_ = false;
    }
  }
  unit_flags_ = 0;

  HeapHashMap<Member<const ContainerQuery>, Result> new_results;
  for (const auto& pair : results_) {
    if (pair.value.change <= change &&
        ((container_type == kSizeContainer &&
          pair.key->Selector().SelectsSizeContainers()) ||
         (container_type == kStickyContainer &&
          pair.key->Selector().SelectsStickyContainers()) ||
         (container_type == kSnapContainer &&
          pair.key->Selector().SelectsSnapContainers()) ||
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
  if (referenced_by_unit_) {
    return Change::kDescendantContainers;
  }

  Change change = Change::kNone;

  for (const auto& result : results_) {
    const ContainerQuery& query = *result.key;
    if (!query.Selector().SelectsSizeContainers()) {
      continue;
    }
    if (Eval(query).value != result.value.value) {
      change = std::max(result.value.change, change);
    }
  }

  return change;
}

ContainerQueryEvaluator::Change ContainerQueryEvaluator::ComputeStyleChange()
    const {
  Change change = Change::kNone;

  for (const auto& result : results_) {
    const ContainerQuery& query = *result.key;
    if (!query.Selector().SelectsStyleContainers()) {
      continue;
    }
    if (Eval(query).value == result.value.value) {
      continue;
    }
    change = std::max(result.value.change, change);
  }

  return change;
}

ContainerQueryEvaluator::Change ContainerQueryEvaluator::ComputeStickyChange()
    const {
  Change change = Change::kNone;

  for (const auto& result : results_) {
    const ContainerQuery& query = *result.key;
    if (!query.Selector().SelectsStickyContainers()) {
      continue;
    }
    if (Eval(query).value == result.value.value) {
      continue;
    }
    change = std::max(result.value.change, change);
  }

  return change;
}

ContainerQueryEvaluator::Change ContainerQueryEvaluator::ComputeSnapChange()
    const {
  Change change = Change::kNone;

  for (const auto& result : results_) {
    const ContainerQuery& query = *result.key;
    if (!query.Selector().SelectsSnapContainers()) {
      continue;
    }
    if (Eval(query).value == result.value.value) {
      continue;
    }
    change = std::max(result.value.change, change);
  }

  return change;
}

void ContainerQueryEvaluator::UpdateContainerValuesFromUnitChanges(
    StyleRecalcChange change) {
  CHECK(media_query_evaluator_);
  unsigned changed_flags = 0;
  if (change.RemUnitsMaybeChanged()) {
    changed_flags |= MediaQueryExpValue::kRootFontRelative;
  }
  if (change.ContainerRelativeUnitsMaybeChanged()) {
    changed_flags |= MediaQueryExpValue::kContainer;
  }
  if (!(unit_flags_ & changed_flags)) {
    return;
  }
  // We recreate both the MediaQueryEvaluator and the CSSContainerValues objects
  // here only to update the font-size etc from the current container style in
  // CSSContainerValues.
  const MediaValues& existing_values = media_query_evaluator_->GetMediaValues();
  Element* container = existing_values.ContainerElement();
  auto* query_values = MakeGarbageCollected<CSSContainerValues>(
      container->GetDocument(), *container, existing_values.Width(),
      existing_values.Height(), existing_values.StuckHorizontal(),
      existing_values.StuckVertical(), existing_values.SnappedFlags());
  media_query_evaluator_ =
      MakeGarbageCollected<MediaQueryEvaluator>(query_values);
}

void ContainerQueryEvaluator::MarkFontDirtyIfNeeded(
    const ComputedStyle& old_style,
    const ComputedStyle& new_style) {
  if (!(unit_flags_ & MediaQueryExpValue::kFontRelative) || font_dirty_) {
    return;
  }
  font_dirty_ = old_style.GetFont() != new_style.GetFont();
}

Element* ContainerQueryEvaluator::ContainerElement() const {
  CHECK(media_query_evaluator_);
  return media_query_evaluator_->GetMediaValues().ContainerElement();
}

}  // namespace blink
