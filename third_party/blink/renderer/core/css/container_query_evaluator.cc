// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/container_query_evaluator.h"

#include "base/feature_list.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/core/css/container_query.h"
#include "third_party/blink/renderer/core/css/container_state.h"
#include "third_party/blink/renderer/core/css/css_container_values.h"
#include "third_party/blink/renderer/core/css/media_values_cached.h"
#include "third_party/blink/renderer/core/css/resolver/match_result.h"
#include "third_party/blink/renderer/core/css/scroll_state_query_snapshot.h"
#include "third_party/blink/renderer/core/css/snapped_query_scroll_snapshot.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/style_recalc_context.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
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
    for (const auto& scoped_name : names) {
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
        // Keeping the TreeScope matching above to be able to count when this
        // is a behavioral change.
        selector_tree_scope->GetDocument().CountUse(
            WebFeature::kContainerNameQueryFailedTreeScope);
        if (RuntimeEnabledFeatures::CSSContainerNameNotTreeScopedEnabled()) {
          return true;
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

PaintLayerScrollableArea* FindScrollContainerScrollableArea(
    const Element& container) {
  if (const LayoutObject* layout_object = container.GetLayoutObject()) {
    if (const LayoutBox* snap_container =
            layout_object->ContainingScrollContainer()) {
      return snap_container->GetScrollableArea();
    }
  }
  return nullptr;
}

}  // namespace

ContainerQueryEvaluator::ContainerQueryEvaluator(Element& container) {
  if (PaintLayerScrollableArea* scrollable_area =
          FindScrollContainerScrollableArea(container)) {
    if (SnappedQueryScrollSnapshot* snapshot =
            scrollable_area->GetSnappedQueryScrollSnapshot()) {
      ContainerSnappedFlags snapped =
          static_cast<ContainerSnappedFlags>(ContainerSnapped::kNone);
      if (snapshot->GetSnappedTargetX() == container) {
        snapped |= static_cast<ContainerSnappedFlags>(ContainerSnapped::kX);
      }
      if (snapshot->GetSnappedTargetY() == container) {
        snapped |= static_cast<ContainerSnappedFlags>(ContainerSnapped::kY);
      }
      snapped_ = pending_snapped_ = snapped;
    }
  }
  auto* query_values = MakeGarbageCollected<CSSContainerValues>(
      container.GetDocument(), container, std::nullopt, std::nullopt,
      ContainerStuckPhysical::kNo, ContainerStuckPhysical::kNo, snapped_,
      static_cast<ContainerScrollableFlags>(ContainerScrollable::kNone),
      static_cast<ContainerScrollableFlags>(ContainerScrollable::kNone),
      ContainerScrolled::kNone, ContainerScrolled::kNone,
      WritingDirectionMode(WritingMode::kHorizontalTb, TextDirection::kLtr),
      PositionTryFallback());
  media_query_evaluator_ =
      MakeGarbageCollected<MediaQueryEvaluator>(query_values);
}

// static
Element* ContainerQueryEvaluator::FindContainer(
    Element* starting_element,
    const ContainerSelector& container_selector,
    const TreeScope* selector_tree_scope) {
  // TODO(crbug.com/1213888): Cache results.
  for (Element* element = starting_element; element;
       element = FlatTreeTraversal::ParentElement(*element)) {
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

// static
Element* ContainerQueryEvaluator::DetermineStartingElement(
    Element& element,
    PseudoId pseudo_id,
    const ContainerSelector& selector,
    Element* nearest_size_container) {
  // For size queries, we have an optimization where we keep track of
  // the nearest size container during style recalc. This is the first
  // possible candidate for such queries.
  if (selector.SelectsSizeContainers()) {
    return nearest_size_container;
  }
  // We don't have a similar optimization for any other query types,
  // so we'll generally start at the flat-tree parent.
  if (pseudo_id != kPseudoIdNone) {
    // Pseudo-elements start at their originating element, however.
    // https://drafts.csswg.org/css-conditional-5/#container-queries
    return &element;
  }
  return FlatTreeTraversal::ParentElement(element);
}

bool ContainerQueryEvaluator::EvalAndAdd(
    Element* starting_element,
    const StyleRecalcContext& context,
    const ContainerQuery& query,
    ContainerSelectorCache& container_selector_cache,
    MatchResult& match_result) {
  const ContainerSelector& selector = query.Selector();
  if (!selector.SelectsAnyContainer()) {
    return false;
  }
  SetDependencyFlags(query, match_result);
  if (Element* container = CachedContainer(starting_element, selector,
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

void ContainerQueryEvaluator::SetDependencyFlags(const ContainerQuery& query,
                                                 MatchResult& match_result) {
  const ContainerSelector& selector = query.Selector();
  if (selector.SelectsSizeContainers()) {
    match_result.SetDependsOnSizeContainerQueries();
  }
  if (selector.SelectsStyleContainers()) {
    match_result.SetDependsOnStyleContainerQueries();
  }
  if (selector.SelectsScrollStateContainers()) {
    match_result.SetDependsOnScrollStateContainerQueries();
  }
  if (selector.SelectsAnchoredContainers()) {
    match_result.SetDependsOnAnchoredContainerQueries();
  }
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
  if (!depends_on_size_) {
    depends_on_size_ = query.Selector().SelectsSizeContainers();
  }
  if (!depends_on_style_) {
    depends_on_style_ = query.Selector().SelectsStyleContainers();
  }
  if (!depends_on_stuck_) {
    depends_on_stuck_ = query.Selector().SelectsStickyContainers();
    if (depends_on_stuck_ && !scroll_state_snapshot_) {
      CHECK(media_query_evaluator_);
      Element* container_element = ContainerElement();
      CHECK(container_element);
      scroll_state_snapshot_ =
          MakeGarbageCollected<ScrollStateQuerySnapshot>(*container_element);
    }
  }
  if (!depends_on_snapped_) {
    depends_on_snapped_ = query.Selector().SelectsSnapContainers();
    if (depends_on_snapped_) {
      if (PaintLayerScrollableArea* scrollable_area =
              FindScrollContainerScrollableArea(*ContainerElement())) {
        scrollable_area->EnsureSnappedQueryScrollSnapshot();
      }
    }
  }
  if (!depends_on_scrollable_) {
    depends_on_scrollable_ = query.Selector().SelectsScrollableContainers();
    if (depends_on_scrollable_ && !scroll_state_snapshot_) {
      CHECK(media_query_evaluator_);
      Element* container_element = ContainerElement();
      CHECK(container_element);
      scroll_state_snapshot_ =
          MakeGarbageCollected<ScrollStateQuerySnapshot>(*container_element);
    }
  }
  if (RuntimeEnabledFeatures::CSSScrolledContainerQueriesEnabled() &&
      !depends_on_scrolled_) {
    depends_on_scrolled_ = query.Selector().SelectsScrolledContainers();
    if (depends_on_scrolled_ && !scroll_state_snapshot_) {
      CHECK(media_query_evaluator_);
      Element* container_element = ContainerElement();
      CHECK(container_element);
      scroll_state_snapshot_ =
          MakeGarbageCollected<ScrollStateQuerySnapshot>(*container_element);
    }
  }
  if ((unit_flags_ & MediaQueryExpValue::UnitFlags::kTreeCounting) == 0 &&
      (result.unit_flags & MediaQueryExpValue::UnitFlags::kTreeCounting) != 0) {
    Element* container = ContainerElement();
    if (ContainerNode* parent = container->ParentElementOrDocumentFragment()) {
      parent->SetChildrenAffectedByForwardPositionalRules();
      parent->SetChildrenAffectedByBackwardPositionalRules();
      container->GetDocument().GetStyleEngine().SetUsesTreeCountingFunctions();
    }
  }
  unit_flags_ |= result.unit_flags;

  return result.value;
}

ContainerQueryEvaluator::Change ContainerQueryEvaluator::SizeContainerChanged(
    PhysicalSize size,
    PhysicalAxes contained_axes) {
  if (size_ == size && contained_axes_ == contained_axes) {
    return Change::kNone;
  }

  UpdateContainerSize(size, contained_axes);

  Change change = ComputeSizeChange();
  if (change != Change::kNone) {
    ClearResults(change, kSizeContainer);
  }

  return referenced_by_unit_ ? Change::kDescendantContainers : change;
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
  if (scroll_state_snapshot_) {
    change = StickyContainerChanged(scroll_state_snapshot_->StuckHorizontal(),
                                    scroll_state_snapshot_->StuckVertical());
    Change scrollable_change = ScrollableContainerChanged(
        scroll_state_snapshot_->ScrollableHorizontal(),
        scroll_state_snapshot_->ScrollableVertical());
    change = std::max(change, scrollable_change);
    if (RuntimeEnabledFeatures::CSSScrolledContainerQueriesEnabled()) {
      Change scrolled_change =
          ScrolledContainerChanged(scroll_state_snapshot_->ScrolledHorizontal(),
                                   scroll_state_snapshot_->ScrolledVertical());
      change = std::max(change, scrolled_change);
    }
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
ContainerQueryEvaluator::ScrollableContainerChanged(
    ContainerScrollableFlags scrollable_horizontal,
    ContainerScrollableFlags scrollable_vertical) {
  if (scrollable_horizontal_ == scrollable_horizontal &&
      scrollable_vertical_ == scrollable_vertical) {
    return Change::kNone;
  }

  UpdateContainerScrollable(scrollable_horizontal, scrollable_vertical);
  Change change = ComputeScrollableChange();
  if (change != Change::kNone) {
    ClearResults(change, kScrollableContainer);
  }

  return change;
}

ContainerQueryEvaluator::Change
ContainerQueryEvaluator::ScrolledContainerChanged(
    ContainerScrolled scrolled_horizontal,
    ContainerScrolled scrolled_vertical) {
  if (scrolled_horizontal_ == scrolled_horizontal &&
      scrolled_vertical_ == scrolled_vertical) {
    return Change::kNone;
  }

  UpdateContainerScrolled(scrolled_horizontal, scrolled_vertical);
  Change change = ComputeScrolledChange();
  if (change != Change::kNone) {
    ClearResults(change, kScrolledContainer);
  }

  return change;
}

// Re-evaluate the cached results and clear any results which are affected by
// the anchored fallback changes.
ContainerQueryEvaluator::Change
ContainerQueryEvaluator::AnchoredContainerChanged(
    const PositionTryFallback& anchored_fallback,
    WritingDirectionMode abs_container_writing_direction) {
  if (anchored_fallback_ == anchored_fallback) {
    return Change::kNone;
  }
  UpdateAnchoredFallback(anchored_fallback, abs_container_writing_direction);
  Change change = ComputeAnchoredChange();
  if (change != Change::kNone) {
    ClearResults(change, kAnchoredContainer);
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

ContainerQueryEvaluator::Change
ContainerQueryEvaluator::StyleAffectingSizeChanged() {
  Change change = ComputeSizeChange();
  if (change != Change::kNone) {
    ClearResults(change, kSizeContainer);
  }
  return change;
}

ContainerQueryEvaluator::Change
ContainerQueryEvaluator::StyleAffectingScrollStateChanged() {
  Change change = Change::kNone;

  Change snap_change = ComputeSnapChange();
  if (snap_change != Change::kNone) {
    ClearResults(snap_change, kSnapContainer);
  }
  change = std::max(change, snap_change);

  Change sticky_change = ComputeStickyChange();
  if (sticky_change != Change::kNone) {
    ClearResults(sticky_change, kStickyContainer);
  }
  change = std::max(change, sticky_change);

  Change scrollable_change = ComputeScrollableChange();
  if (scrollable_change != Change::kNone) {
    ClearResults(scrollable_change, kScrollableContainer);
  }
  change = std::max(change, scrollable_change);

  if (RuntimeEnabledFeatures::CSSScrolledContainerQueriesEnabled()) {
    Change scrolled_change = ComputeScrolledChange();
    if (scrolled_change != Change::kNone) {
      ClearResults(scrolled_change, kScrolledContainer);
    }
    change = std::max(change, scrolled_change);
  }
  return change;
}

void ContainerQueryEvaluator::UpdateContainerValues() {
  const MediaValues& existing_values = media_query_evaluator_->GetMediaValues();
  Element* container = existing_values.ContainerElement();
  auto* query_values = MakeGarbageCollected<CSSContainerValues>(
      container->GetDocument(), *container, existing_values.Width(),
      existing_values.Height(), existing_values.StuckHorizontal(),
      existing_values.StuckVertical(), existing_values.SnappedFlags(),
      existing_values.ScrollableHorizontal(),
      existing_values.ScrollableVertical(),
      existing_values.ScrolledHorizontal(), existing_values.ScrolledVertical(),
      existing_values.AbsContainerWritingDirection(),
      existing_values.AnchoredFallback());
  media_query_evaluator_ =
      MakeGarbageCollected<MediaQueryEvaluator>(query_values);
}

void ContainerQueryEvaluator::Trace(Visitor* visitor) const {
  visitor->Trace(media_query_evaluator_);
  visitor->Trace(results_);
  visitor->Trace(scroll_state_snapshot_);
  visitor->Trace(anchored_fallback_);
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
      existing_values.SnappedFlags(), existing_values.ScrollableHorizontal(),
      existing_values.ScrollableVertical(),
      existing_values.ScrolledHorizontal(), existing_values.ScrolledVertical(),
      existing_values.AbsContainerWritingDirection(),
      existing_values.AnchoredFallback());
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
      existing_values.SnappedFlags(), existing_values.ScrollableHorizontal(),
      existing_values.ScrollableVertical(),
      existing_values.ScrolledHorizontal(), existing_values.ScrolledVertical(),
      existing_values.AbsContainerWritingDirection(),
      existing_values.AnchoredFallback());
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
      existing_values.StuckVertical(), snapped,
      existing_values.ScrollableHorizontal(),
      existing_values.ScrollableVertical(),
      existing_values.ScrolledHorizontal(), existing_values.ScrolledVertical(),
      existing_values.AbsContainerWritingDirection(),
      existing_values.AnchoredFallback());
  media_query_evaluator_ =
      MakeGarbageCollected<MediaQueryEvaluator>(query_values);
}

void ContainerQueryEvaluator::UpdateContainerScrollable(
    ContainerScrollableFlags scrollable_horizontal,
    ContainerScrollableFlags scrollable_vertical) {
  scrollable_horizontal_ = scrollable_horizontal;
  scrollable_vertical_ = scrollable_vertical;

  const MediaValues& existing_values = media_query_evaluator_->GetMediaValues();
  Element* container = existing_values.ContainerElement();

  auto* query_values = MakeGarbageCollected<CSSContainerValues>(
      container->GetDocument(), *container, existing_values.Width(),
      existing_values.Height(), existing_values.StuckHorizontal(),
      existing_values.StuckVertical(), existing_values.Snapped(),
      scrollable_horizontal, scrollable_vertical,
      existing_values.ScrolledHorizontal(), existing_values.ScrolledVertical(),
      existing_values.AbsContainerWritingDirection(),
      existing_values.AnchoredFallback());
  media_query_evaluator_ =
      MakeGarbageCollected<MediaQueryEvaluator>(query_values);
}

void ContainerQueryEvaluator::UpdateContainerScrolled(
    ContainerScrolled scrolled_horizontal,
    ContainerScrolled scrolled_vertical) {
  scrolled_horizontal_ = scrolled_horizontal;
  scrolled_vertical_ = scrolled_vertical;

  const MediaValues& existing_values = media_query_evaluator_->GetMediaValues();
  Element* container = existing_values.ContainerElement();

  auto* query_values = MakeGarbageCollected<CSSContainerValues>(
      container->GetDocument(), *container, existing_values.Width(),
      existing_values.Height(), existing_values.StuckHorizontal(),
      existing_values.StuckVertical(), existing_values.Snapped(),
      existing_values.ScrollableHorizontal(),
      existing_values.ScrollableVertical(), scrolled_horizontal,
      scrolled_vertical, existing_values.AbsContainerWritingDirection(),
      existing_values.AnchoredFallback());
  media_query_evaluator_ =
      MakeGarbageCollected<MediaQueryEvaluator>(query_values);
}

void ContainerQueryEvaluator::UpdateAnchoredFallback(
    const PositionTryFallback& anchored_fallback,
    WritingDirectionMode abs_container_writing_direction) {
  anchored_fallback_ = anchored_fallback;

  const MediaValues& existing_values = media_query_evaluator_->GetMediaValues();
  Element* container = existing_values.ContainerElement();

  auto* query_values = MakeGarbageCollected<CSSContainerValues>(
      container->GetDocument(), *container, existing_values.Width(),
      existing_values.Height(), existing_values.StuckHorizontal(),
      existing_values.StuckVertical(), existing_values.Snapped(),
      existing_values.ScrollableHorizontal(),
      existing_values.ScrollableVertical(),
      existing_values.ScrolledHorizontal(), existing_values.ScrolledVertical(),
      abs_container_writing_direction, anchored_fallback);
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
         (container_type == kScrollableContainer &&
          pair.key->Selector().SelectsScrollableContainers()) ||
         (RuntimeEnabledFeatures::CSSScrolledContainerQueriesEnabled() &&
          container_type == kScrolledContainer &&
          pair.key->Selector().SelectsScrolledContainers()) ||
         (container_type == kAnchoredContainer &&
          pair.key->Selector().SelectsAnchoredContainers()) ||
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

ContainerQueryEvaluator::Change
ContainerQueryEvaluator::ComputeScrollableChange() const {
  Change change = Change::kNone;

  for (const auto& result : results_) {
    const ContainerQuery& query = *result.key;
    if (!query.Selector().SelectsScrollableContainers()) {
      continue;
    }
    if (Eval(query).value == result.value.value) {
      continue;
    }
    change = std::max(result.value.change, change);
  }

  return change;
}

ContainerQueryEvaluator::Change ContainerQueryEvaluator::ComputeScrolledChange()
    const {
  Change change = Change::kNone;

  for (const auto& result : results_) {
    const ContainerQuery& query = *result.key;
    if (!query.Selector().SelectsScrolledContainers()) {
      continue;
    }
    if (Eval(query).value == result.value.value) {
      continue;
    }
    change = std::max(result.value.change, change);
  }

  return change;
}

ContainerQueryEvaluator::Change ContainerQueryEvaluator::ComputeAnchoredChange()
    const {
  Change change = Change::kNone;

  for (const auto& result : results_) {
    const ContainerQuery& query = *result.key;
    if (!query.Selector().SelectsAnchoredContainers()) {
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
  UpdateContainerValues();
}

StyleRecalcChange ContainerQueryEvaluator::ApplyAnchoredChanges(
    const StyleRecalcChange& child_change,
    const PositionTryFallback& try_fallback,
    WritingDirectionMode abs_container_writing_direction) {
  switch (
      AnchoredContainerChanged(try_fallback, abs_container_writing_direction)) {
    case ContainerQueryEvaluator::Change::kNone:
      return child_change;
    case ContainerQueryEvaluator::Change::kNearestContainer:
      return child_change.ForceRecalcAnchoredContainer();
    case ContainerQueryEvaluator::Change::kDescendantContainers:
      return child_change.ForceRecalcDescendantAnchoredContainers();
  }
}

StyleRecalcChange ContainerQueryEvaluator::ApplyScrollStateAndStyleChanges(
    const StyleRecalcChange& child_change,
    const ComputedStyle& old_style,
    const ComputedStyle& new_style,
    bool style_changed) {
  StyleRecalcChange recalc_change = child_change;
  switch (ApplyScrollState()) {
    case ContainerQueryEvaluator::Change::kNone:
      break;
    case ContainerQueryEvaluator::Change::kNearestContainer:
      recalc_change = recalc_change.ForceRecalcScrollStateContainer();
      break;
    case ContainerQueryEvaluator::Change::kDescendantContainers:
      recalc_change =
          recalc_change.ForceRecalcDescendantScrollStateContainers();
      break;
  }

  if (!style_changed && !DependsOnTreeCounting()) {
    // If some query depends on a tree-counting function (e.g. sibling-index()),
    // we must re-evaluate the queries even if the style didn't change. This is
    // because the sibling-index/count isn't stored on ComputedStyle.
    return recalc_change;
  }

  // If size container queries are expressed in font-relative units, the query
  // evaluation may change even if the size of the container in pixels did not
  // change. If the old and new style use different font properties, and there
  // are existing queries that depend on font relative units, we need to update
  // the container values and invalidate style for any changed queries.
  bool invalidate_for_font =
      (unit_flags_ & MediaQueryExpValue::kFontRelative) &&
      (base::FeatureList::IsEnabled(blink::features::kCSSFontComparisonFix)
           ? !base::ValuesEquivalent(old_style.GetFont(), new_style.GetFont())
           : old_style.GetFont() != new_style.GetFont());

  // Writing direction changes may affect how logical queries match for size and
  // scroll-state() queries even when the physical size or scroll-state do not
  // change.
  bool invalidate_for_writing_direction =
      MayDependOnWritingDirection() &&
      old_style.GetWritingDirection() != new_style.GetWritingDirection();

  if (invalidate_for_writing_direction || invalidate_for_font ||
      DependsOnTreeCounting()) {
    // Writing direction and font sizing are cached on CSSContainerValues. Need
    // to recreate the values based on the current ComputedStyle.
    UpdateContainerValues();
  }

  if (invalidate_for_writing_direction || invalidate_for_font ||
      DependsOnTreeCounting()) {
    switch (StyleAffectingSizeChanged()) {
      case ContainerQueryEvaluator::Change::kNone:
        break;
      case ContainerQueryEvaluator::Change::kNearestContainer:
        recalc_change = recalc_change.ForceRecalcSizeContainer();
        break;
      case ContainerQueryEvaluator::Change::kDescendantContainers:
        recalc_change = recalc_change.ForceRecalcDescendantSizeContainers();
        break;
    }
  }
  if (invalidate_for_writing_direction) {
    switch (StyleAffectingScrollStateChanged()) {
      case ContainerQueryEvaluator::Change::kNone:
        break;
      case ContainerQueryEvaluator::Change::kNearestContainer:
        recalc_change = recalc_change.ForceRecalcScrollStateContainer();
        break;
      case ContainerQueryEvaluator::Change::kDescendantContainers:
        recalc_change =
            recalc_change.ForceRecalcDescendantScrollStateContainers();
        break;
    }
  }
  if (old_style.InheritedVariables() != new_style.InheritedVariables() ||
      old_style.NonInheritedVariables() != new_style.NonInheritedVariables() ||
      DependsOnTreeCounting()) {
    switch (StyleContainerChanged()) {
      case ContainerQueryEvaluator::Change::kNone:
        break;
      case ContainerQueryEvaluator::Change::kNearestContainer:
        recalc_change = recalc_change.ForceRecalcStyleContainerChildren();
        break;
      case ContainerQueryEvaluator::Change::kDescendantContainers:
        recalc_change = recalc_change.ForceRecalcStyleContainerDescendants();
        break;
    }
  }
  return recalc_change;
}

Element* ContainerQueryEvaluator::ContainerElement() const {
  CHECK(media_query_evaluator_);
  return media_query_evaluator_->GetMediaValues().ContainerElement();
}

}  // namespace blink
