// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/invalidation/rule_invalidation_data_visitor.h"

#include "third_party/blink/renderer/core/css/css_selector_list.h"
#include "third_party/blink/renderer/core/css/style_scope.h"
#include "third_party/blink/renderer/core/inspector/invalidation_set_to_selector_map.h"

namespace blink {

namespace {

bool SupportsInvalidation(CSSSelector::MatchType match) {
  switch (match) {
    case CSSSelector::kTag:
    case CSSSelector::kId:
    case CSSSelector::kClass:
    case CSSSelector::kAttributeExact:
    case CSSSelector::kAttributeSet:
    case CSSSelector::kAttributeHyphen:
    case CSSSelector::kAttributeList:
    case CSSSelector::kAttributeContain:
    case CSSSelector::kAttributeBegin:
    case CSSSelector::kAttributeEnd:
      return true;
    case CSSSelector::kUnknown:
    case CSSSelector::kPagePseudoClass:
      // These should not appear in StyleRule selectors.
      NOTREACHED_IN_MIGRATION();
      return false;
    default:
      // New match type added. Figure out if it needs a subtree invalidation or
      // not.
      NOTREACHED_IN_MIGRATION();
      return false;
  }
}

bool SupportsInvalidation(CSSSelector::PseudoType type) {
  switch (type) {
    case CSSSelector::kPseudoEmpty:
    case CSSSelector::kPseudoFirstChild:
    case CSSSelector::kPseudoFirstOfType:
    case CSSSelector::kPseudoLastChild:
    case CSSSelector::kPseudoLastOfType:
    case CSSSelector::kPseudoOnlyChild:
    case CSSSelector::kPseudoOnlyOfType:
    case CSSSelector::kPseudoNthChild:
    case CSSSelector::kPseudoNthOfType:
    case CSSSelector::kPseudoNthLastChild:
    case CSSSelector::kPseudoNthLastOfType:
    case CSSSelector::kPseudoPart:
    case CSSSelector::kPseudoState:
    case CSSSelector::kPseudoStateDeprecatedSyntax:
    case CSSSelector::kPseudoLink:
    case CSSSelector::kPseudoVisited:
    case CSSSelector::kPseudoAny:
    case CSSSelector::kPseudoWebkitAnyLink:
    case CSSSelector::kPseudoAnyLink:
    case CSSSelector::kPseudoAutofill:
    case CSSSelector::kPseudoWebKitAutofill:
    case CSSSelector::kPseudoAutofillPreviewed:
    case CSSSelector::kPseudoAutofillSelected:
    case CSSSelector::kPseudoHover:
    case CSSSelector::kPseudoDrag:
    case CSSSelector::kPseudoFocus:
    case CSSSelector::kPseudoFocusVisible:
    case CSSSelector::kPseudoFocusWithin:
    case CSSSelector::kPseudoActive:
    case CSSSelector::kPseudoChecked:
    case CSSSelector::kPseudoEnabled:
    case CSSSelector::kPseudoFullPageMedia:
    case CSSSelector::kPseudoDefault:
    case CSSSelector::kPseudoDisabled:
    case CSSSelector::kPseudoOptional:
    case CSSSelector::kPseudoPlaceholderShown:
    case CSSSelector::kPseudoRequired:
    case CSSSelector::kPseudoReadOnly:
    case CSSSelector::kPseudoReadWrite:
    case CSSSelector::kPseudoUserInvalid:
    case CSSSelector::kPseudoUserValid:
    case CSSSelector::kPseudoValid:
    case CSSSelector::kPseudoInvalid:
    case CSSSelector::kPseudoIndeterminate:
    case CSSSelector::kPseudoTarget:
    case CSSSelector::kPseudoCurrent:
    case CSSSelector::kPseudoBefore:
    case CSSSelector::kPseudoAfter:
    case CSSSelector::kPseudoMarker:
    case CSSSelector::kPseudoModal:
    case CSSSelector::kPseudoSelectorFragmentAnchor:
    case CSSSelector::kPseudoBackdrop:
    case CSSSelector::kPseudoLang:
    case CSSSelector::kPseudoDir:
    case CSSSelector::kPseudoNot:
    case CSSSelector::kPseudoPlaceholder:
    case CSSSelector::kPseudoDetailsContent:
    case CSSSelector::kPseudoFileSelectorButton:
    case CSSSelector::kPseudoResizer:
    case CSSSelector::kPseudoRoot:
    case CSSSelector::kPseudoScope:
    case CSSSelector::kPseudoScrollbar:
    case CSSSelector::kPseudoScrollbarButton:
    case CSSSelector::kPseudoScrollbarCorner:
    case CSSSelector::kPseudoScrollbarThumb:
    case CSSSelector::kPseudoScrollbarTrack:
    case CSSSelector::kPseudoScrollbarTrackPiece:
    case CSSSelector::kPseudoScrollMarkerGroup:
    case CSSSelector::kPseudoScrollMarker:
    case CSSSelector::kPseudoScrollNextButton:
    case CSSSelector::kPseudoScrollPrevButton:
    case CSSSelector::kPseudoColumn:
    case CSSSelector::kPseudoWindowInactive:
    case CSSSelector::kPseudoSelection:
    case CSSSelector::kPseudoCornerPresent:
    case CSSSelector::kPseudoDecrement:
    case CSSSelector::kPseudoIncrement:
    case CSSSelector::kPseudoHorizontal:
    case CSSSelector::kPseudoVertical:
    case CSSSelector::kPseudoStart:
    case CSSSelector::kPseudoEnd:
    case CSSSelector::kPseudoDoubleButton:
    case CSSSelector::kPseudoSingleButton:
    case CSSSelector::kPseudoNoButton:
    case CSSSelector::kPseudoFullScreen:
    case CSSSelector::kPseudoFullScreenAncestor:
    case CSSSelector::kPseudoFullscreen:
    case CSSSelector::kPseudoPaused:
    case CSSSelector::kPseudoPermissionElementInvalidStyle:
    case CSSSelector::kPseudoPermissionElementOccluded:
    case CSSSelector::kPseudoPermissionGranted:
    case CSSSelector::kPseudoPictureInPicture:
    case CSSSelector::kPseudoPlaying:
    case CSSSelector::kPseudoInRange:
    case CSSSelector::kPseudoOutOfRange:
    case CSSSelector::kPseudoWebKitCustomElement:
    case CSSSelector::kPseudoBlinkInternalElement:
    case CSSSelector::kPseudoCue:
    case CSSSelector::kPseudoFutureCue:
    case CSSSelector::kPseudoPastCue:
    case CSSSelector::kPseudoDefined:
    case CSSSelector::kPseudoHost:
    case CSSSelector::kPseudoSpatialNavigationFocus:
    case CSSSelector::kPseudoHasDatalist:
    case CSSSelector::kPseudoIsHtml:
    case CSSSelector::kPseudoListBox:
    case CSSSelector::kPseudoMultiSelectFocus:
    case CSSSelector::kPseudoHostHasNonAutoAppearance:
    case CSSSelector::kPseudoOpen:
    case CSSSelector::kPseudoClosed:
    case CSSSelector::kPseudoDialogInTopLayer:
    case CSSSelector::kPseudoSelectHasChildButton:
    case CSSSelector::kPseudoPicker:
    case CSSSelector::kPseudoPopoverInTopLayer:
    case CSSSelector::kPseudoPopoverOpen:
    case CSSSelector::kPseudoSlotted:
    case CSSSelector::kPseudoVideoPersistent:
    case CSSSelector::kPseudoVideoPersistentAncestor:
    case CSSSelector::kPseudoXrOverlay:
    case CSSSelector::kPseudoIs:
    case CSSSelector::kPseudoWhere:
    case CSSSelector::kPseudoParent:  // Same as kPseudoIs.
    case CSSSelector::kPseudoSearchText:
    case CSSSelector::kPseudoTargetText:
    case CSSSelector::kPseudoHighlight:
    case CSSSelector::kPseudoSpellingError:
    case CSSSelector::kPseudoGrammarError:
    case CSSSelector::kPseudoHas:
    case CSSSelector::kPseudoUnparsed:  // Never invalidates.
    case CSSSelector::kPseudoTrue:
    case CSSSelector::kPseudoViewTransition:
    case CSSSelector::kPseudoViewTransitionGroup:
    case CSSSelector::kPseudoViewTransitionImagePair:
    case CSSSelector::kPseudoViewTransitionNew:
    case CSSSelector::kPseudoViewTransitionOld:
    case CSSSelector::kPseudoActiveViewTransition:
    case CSSSelector::kPseudoActiveViewTransitionType:
    case CSSSelector::kPseudoHasSlotted:
      return true;
    case CSSSelector::kPseudoUnknown:
    case CSSSelector::kPseudoLeftPage:
    case CSSSelector::kPseudoRightPage:
    case CSSSelector::kPseudoFirstPage:
      // These should not appear in StyleRule selectors.
      NOTREACHED_IN_MIGRATION();
      return false;
    default:
      // New pseudo type added. Figure out if it needs a subtree invalidation or
      // not.
      NOTREACHED_IN_MIGRATION();
      return false;
  }
}

bool SupportsInvalidationWithSelectorList(CSSSelector::PseudoType pseudo) {
  return pseudo == CSSSelector::kPseudoAny ||
         pseudo == CSSSelector::kPseudoCue ||
         pseudo == CSSSelector::kPseudoHost ||
         pseudo == CSSSelector::kPseudoHostContext ||
         pseudo == CSSSelector::kPseudoIs ||
         pseudo == CSSSelector::kPseudoNot ||
         pseudo == CSSSelector::kPseudoSlotted ||
         pseudo == CSSSelector::kPseudoWhere ||
         pseudo == CSSSelector::kPseudoParent ||
         pseudo == CSSSelector::kPseudoNthChild ||
         pseudo == CSSSelector::kPseudoNthLastChild;
}

bool RequiresSubtreeInvalidation(const CSSSelector& selector) {
  if (selector.Match() != CSSSelector::kPseudoElement &&
      selector.Match() != CSSSelector::kPseudoClass) {
    DCHECK(SupportsInvalidation(selector.Match()));
    return false;
  }

  switch (selector.GetPseudoType()) {
    case CSSSelector::kPseudoFirstLine:
    case CSSSelector::kPseudoFirstLetter:
    // FIXME: Most pseudo classes/elements above can be supported and moved
    // to assertSupportedPseudo(). Move on a case-by-case basis. If they
    // require subtree invalidation, document why.
    case CSSSelector::kPseudoHostContext:
      // :host-context matches a shadow host, yet the simple selectors inside
      // :host-context matches an ancestor of the shadow host.
      return true;
    default:
      DCHECK(SupportsInvalidation(selector.GetPseudoType()));
      return false;
  }
}

// Creates a copy of an InvalidationSet by combining an empty InvalidationSet
// (of the same type) with the specified InvalidationSet.
//
// See also InvalidationSet::Combine.
scoped_refptr<InvalidationSet> CopyInvalidationSet(
    const InvalidationSet& invalidation_set) {
  if (invalidation_set.IsSiblingInvalidationSet()) {
    scoped_refptr<InvalidationSet> copy =
        SiblingInvalidationSet::Create(nullptr);
    copy->Combine(invalidation_set);
    return copy;
  }
  if (invalidation_set.IsSelfInvalidationSet()) {
    scoped_refptr<InvalidationSet> copy = DescendantInvalidationSet::Create();
    copy->SetInvalidatesSelf();
    return copy;
  }
  scoped_refptr<InvalidationSet> copy = DescendantInvalidationSet::Create();
  copy->Combine(invalidation_set);
  return copy;
}

bool IsSimpleSelectorValidAfterHost(const CSSSelector* simple_selector) {
  // TODO(blee@igalia.com) Need to support logical combinations after :host
  // (e.g. ':host:not(:has(.a))')
  return simple_selector->Match() == CSSSelector::kPseudoElement ||
         simple_selector->IsHostPseudoClass() ||
         simple_selector->GetPseudoType() == CSSSelector::kPseudoHas;
}

}  // anonymous namespace

template <RuleInvalidationDataVisitorType VisitorType>
RuleInvalidationDataVisitor<VisitorType>::RuleInvalidationDataVisitor(
    RuleInvalidationDataType& rule_invalidation_data)
    : rule_invalidation_data_(rule_invalidation_data) {}

template <RuleInvalidationDataVisitorType VisitorType>
void RuleInvalidationDataVisitor<VisitorType>::InvalidationSetFeatures::Merge(
    const InvalidationSetFeatures& other) {
  classes.AppendVector(other.classes);
  attributes.AppendVector(other.attributes);
  ids.AppendVector(other.ids);
  // Tag names that have been added to an invalidation set for an ID, a class,
  // or an attribute are called "emitted" tag names. Emitted tag names need to
  // go in a separate vector in order to correctly track which tag names to
  // add to the type rule invalidation set.
  //
  // Example: :is(.a, div) :is(span, .b, ol, .c li)
  //
  // For the above selector, we need span and ol in the type invalidation set,
  // but not li, since that tag name was added to the invalidation set for .c.
  // Hence, when processing the rightmost :is(), we end up with li in the
  // emitted_tag_names vector, and span and ol in the regular tag_names vector.
  if (other.has_features_for_rule_set_invalidation) {
    emitted_tag_names.AppendVector(other.tag_names);
  } else {
    tag_names.AppendVector(other.tag_names);
  }
  emitted_tag_names.AppendVector(other.emitted_tag_names);
  max_direct_adjacent_selectors = std::max(max_direct_adjacent_selectors,
                                           other.max_direct_adjacent_selectors);
  invalidation_flags.Merge(other.invalidation_flags);
  content_pseudo_crossing |= other.content_pseudo_crossing;
  has_nth_pseudo |= other.has_nth_pseudo;
}

template <RuleInvalidationDataVisitorType VisitorType>
bool RuleInvalidationDataVisitor<
    VisitorType>::InvalidationSetFeatures::HasFeatures() const {
  return !classes.empty() || !attributes.empty() || !ids.empty() ||
         !tag_names.empty() || !emitted_tag_names.empty() ||
         invalidation_flags.InvalidateCustomPseudo() ||
         invalidation_flags.InvalidatesParts();
}

template <RuleInvalidationDataVisitorType VisitorType>
bool RuleInvalidationDataVisitor<
    VisitorType>::InvalidationSetFeatures::HasIdClassOrAttribute() const {
  return !classes.empty() || !attributes.empty() || !ids.empty();
}

template <RuleInvalidationDataVisitorType VisitorType>
void RuleInvalidationDataVisitor<VisitorType>::InvalidationSetFeatures::
    NarrowToFeatures(const InvalidationSetFeatures& other) {
  unsigned size = Size();
  unsigned other_size = other.Size();
  if (size == 0 || (1 <= other_size && other_size < size)) {
    ClearFeatures();
    Merge(other);
  }
}

template <RuleInvalidationDataVisitorType VisitorType>
SelectorPreMatch
RuleInvalidationDataVisitor<VisitorType>::CollectFeaturesFromSelector(
    const CSSSelector& selector,
    const StyleScope* style_scope) {
  FeatureMetadata metadata;
  const unsigned max_direct_adjacent_selectors = 0;
  if (CollectMetadataFromSelector(selector, max_direct_adjacent_selectors,
                                  metadata) ==
      SelectorPreMatch::kNeverMatches) {
    return SelectorPreMatch::kNeverMatches;
  }
  if constexpr (is_builder()) {
    rule_invalidation_data_.uses_first_line_rules |=
        metadata.uses_first_line_rules;
    rule_invalidation_data_.uses_window_inactive_selector |=
        metadata.uses_window_inactive_selector;
    rule_invalidation_data_.max_direct_adjacent_selectors =
        std::max(rule_invalidation_data_.max_direct_adjacent_selectors,
                 metadata.max_direct_adjacent_selectors);
  }

  UpdateInvalidationSets(selector, style_scope);
  return SelectorPreMatch::kMayMatch;
}

template <RuleInvalidationDataVisitorType VisitorType>
SelectorPreMatch
RuleInvalidationDataVisitor<VisitorType>::CollectMetadataFromSelector(
    const CSSSelector& selector,
    unsigned max_direct_adjacent_selectors,
    FeatureMetadata& metadata) {
  CSSSelector::RelationType relation = CSSSelector::kDescendant;
  bool found_host_pseudo = false;

  for (const CSSSelector* current = &selector; current;
       current = current->NextSimpleSelector()) {
    switch (current->GetPseudoType()) {
      case CSSSelector::kPseudoHas:
        if (found_host_pseudo && !current->IsLastInComplexSelector() &&
            !IsSimpleSelectorValidAfterHost(current->NextSimpleSelector())) {
          return SelectorPreMatch::kNeverMatches;
        }
        break;
      case CSSSelector::kPseudoFirstLine:
        metadata.uses_first_line_rules = true;
        break;
      case CSSSelector::kPseudoWindowInactive:
        metadata.uses_window_inactive_selector = true;
        break;
      case CSSSelector::kPseudoHost:
      case CSSSelector::kPseudoHostContext:
        if (!found_host_pseudo && relation == CSSSelector::kSubSelector) {
          return SelectorPreMatch::kNeverMatches;
        }
        if (!current->IsLastInComplexSelector() &&
            !IsSimpleSelectorValidAfterHost(current->NextSimpleSelector())) {
          return SelectorPreMatch::kNeverMatches;
        }
        found_host_pseudo = true;
        // We fall through here to reach the "default" case. Entering the cases
        // for kPseudoIs/Where has no effect, since :host[-context]() can't
        // produce empty argument lists.
        DCHECK(!current->SelectorList() || current->SelectorList()->IsValid());
        [[fallthrough]];
      case CSSSelector::kPseudoIs:
      case CSSSelector::kPseudoWhere:
        if (const CSSSelectorList* selector_list = current->SelectorList()) {
          // An empty list (!IsValid) is possible here because of the forgiving
          // selector list parsing [1], in which empty lists are not syntax
          // errors, but also don't match anything [2].
          //
          // [1]
          // https://drafts.csswg.org/selectors/#typedef-forgiving-selector-list
          // [2] https://drafts.csswg.org/selectors/#matches
          if (!selector_list->IsValid()) {
            return SelectorPreMatch::kNeverMatches;
          }
        }
        [[fallthrough]];
      case CSSSelector::kPseudoParent:
      default:
        for (const CSSSelector* sub_selector = current->SelectorListOrParent();
             sub_selector;
             sub_selector = CSSSelectorList::Next(*sub_selector)) {
          CollectMetadataFromSelector(*sub_selector,
                                      max_direct_adjacent_selectors, metadata);
        }
        break;
    }

    relation = current->Relation();

    if (found_host_pseudo && relation != CSSSelector::kSubSelector) {
      return SelectorPreMatch::kNeverMatches;
    }

    if (relation == CSSSelector::kDirectAdjacent) {
      max_direct_adjacent_selectors++;
    } else if (max_direct_adjacent_selectors &&
               ((relation != CSSSelector::kSubSelector) ||
                current->IsLastInComplexSelector())) {
      if (max_direct_adjacent_selectors >
          metadata.max_direct_adjacent_selectors) {
        metadata.max_direct_adjacent_selectors = max_direct_adjacent_selectors;
      }
      max_direct_adjacent_selectors = 0;
    }
  }

  DCHECK(!max_direct_adjacent_selectors);
  return SelectorPreMatch::kMayMatch;
}

// Update all invalidation sets for a given selector (potentially in the
// given @scope). See UpdateInvalidationSetsForComplex() for details.
template <RuleInvalidationDataVisitorType VisitorType>
void RuleInvalidationDataVisitor<VisitorType>::UpdateInvalidationSets(
    const CSSSelector& selector,
    const StyleScope* style_scope) {
  STACK_UNINITIALIZED InvalidationSetFeatures features;
  FeatureInvalidationType feature_invalidation_type =
      UpdateInvalidationSetsForComplex(selector, /*in_nth_child=*/false,
                                       style_scope, features, kSubject,
                                       CSSSelector::kPseudoUnknown);
  if (feature_invalidation_type ==
      FeatureInvalidationType::kRequiresSubtreeInvalidation) {
    features.invalidation_flags.SetWholeSubtreeInvalid(true);
  }
  if (style_scope) {
    UpdateFeaturesFromStyleScope(*style_scope, features);
  }
}

// Update all invalidation sets for a given CSS selector; this is usually
// called for the entire selector at top level, but can also end up calling
// itself recursively if any of the selectors contain selector lists
// (e.g. for :not() or :has()).
template <RuleInvalidationDataVisitorType VisitorType>
RuleInvalidationDataVisitor<VisitorType>::FeatureInvalidationType
RuleInvalidationDataVisitor<VisitorType>::UpdateInvalidationSetsForComplex(
    const CSSSelector& complex,
    bool in_nth_child,
    const StyleScope* style_scope,
    InvalidationSetFeatures& features,
    PositionType position,
    CSSSelector::PseudoType pseudo_type) {
  // Given a rule, update the descendant invalidation sets for the features
  // found in its selector. The first step is to extract the features from the
  // rightmost compound selector (ExtractInvalidationSetFeaturesFromCompound).
  // Secondly, add those features to the invalidation sets for the features
  // found in the other compound selectors (AddFeaturesToInvalidationSets).
  // If we find a feature in the right-most compound selector that requires a
  // subtree recalc, next_compound will be the rightmost compound and we will
  // AddFeaturesToInvalidationSets for that one as well.

  InvalidationSetFeatures* sibling_features = nullptr;

  // Step 1. Note that this also, in passing, inserts self-invalidation
  // and nth-child InvalidationSets for the rightmost compound selector.
  // This probably isn't the prettiest, but it's how the structure is
  // at this point.
  const CSSSelector* last_in_compound =
      ExtractInvalidationSetFeaturesFromCompound(
          complex, features, position,
          /* for_logical_combination_in_has */ false, in_nth_child);

  bool was_whole_subtree_invalid =
      features.invalidation_flags.WholeSubtreeInvalid();

  if (features.invalidation_flags.WholeSubtreeInvalid()) {
    features.has_features_for_rule_set_invalidation = false;
  } else if (!features.HasFeatures()) {
    features.invalidation_flags.SetWholeSubtreeInvalid(true);
  }
  // Only check for has_nth_pseudo if this is the top-level complex selector.
  if (pseudo_type == CSSSelector::kPseudoUnknown && features.has_nth_pseudo) {
    // The rightmost compound contains an :nth-* selector.
    // Add the compound features to the NthSiblingInvalidationSet. That is, for
    // '#id:nth-child(even)', add #id to the invalidation set and make sure we
    // invalidate elements matching those features (SetInvalidateSelf()).
    SiblingInvalidationSetType* nth_set = EnsureNthInvalidationSet();
    AddFeaturesToInvalidationSet(nth_set, features);
    SetInvalidatesSelf(nth_set);
  }

  // Step 2.
  const CSSSelector* next_compound =
      last_in_compound ? last_in_compound->NextSimpleSelector() : &complex;

  if (next_compound) {
    if (last_in_compound) {
      UpdateFeaturesFromCombinator(last_in_compound->Relation(), nullptr,
                                   features, sibling_features, features,
                                   /* for_logical_combination_in_has */ false,
                                   in_nth_child);
    }

    AddFeaturesToInvalidationSets(*next_compound, in_nth_child,
                                  sibling_features, features);

    MarkInvalidationSetsWithinNthChild(*next_compound, in_nth_child);
  }

  if (style_scope) {
    AddFeaturesToInvalidationSetsForStyleScope(*style_scope, features);
  }

  if (!next_compound) {
    return kNormalInvalidation;
  }

  // We need to differentiate between no features (HasFeatures()==false)
  // and RequiresSubtreeInvalidation at the callsite. Hence we reset the flag
  // before returning, otherwise the distinction would be lost.
  features.invalidation_flags.SetWholeSubtreeInvalid(was_whole_subtree_invalid);
  return last_in_compound ? kNormalInvalidation : kRequiresSubtreeInvalidation;
}

template <RuleInvalidationDataVisitorType VisitorType>
void RuleInvalidationDataVisitor<VisitorType>::UpdateFeaturesFromCombinator(
    CSSSelector::RelationType combinator,
    const CSSSelector* last_compound_in_adjacent_chain,
    InvalidationSetFeatures& last_compound_in_adjacent_chain_features,
    InvalidationSetFeatures*& sibling_features,
    InvalidationSetFeatures& descendant_features,
    bool for_logical_combination_in_has,
    bool in_nth_child) {
  if (CSSSelector::IsAdjacentRelation(combinator)) {
    if (!sibling_features) {
      sibling_features = &last_compound_in_adjacent_chain_features;
      if (last_compound_in_adjacent_chain) {
        ExtractInvalidationSetFeaturesFromCompound(
            *last_compound_in_adjacent_chain,
            last_compound_in_adjacent_chain_features, kAncestor,
            for_logical_combination_in_has, in_nth_child);
        if (!last_compound_in_adjacent_chain_features.HasFeatures()) {
          last_compound_in_adjacent_chain_features.invalidation_flags
              .SetWholeSubtreeInvalid(true);
        }
      }
    }
    if (sibling_features->max_direct_adjacent_selectors ==
        SiblingInvalidationSet::kDirectAdjacentMax) {
      return;
    }
    if (combinator == CSSSelector::kDirectAdjacent) {
      ++sibling_features->max_direct_adjacent_selectors;
    } else {
      sibling_features->max_direct_adjacent_selectors =
          SiblingInvalidationSet::kDirectAdjacentMax;
    }
    return;
  }

  descendant_features.descendant_features_depth++;

  if (sibling_features &&
      last_compound_in_adjacent_chain_features.max_direct_adjacent_selectors) {
    last_compound_in_adjacent_chain_features = InvalidationSetFeatures();
  }

  sibling_features = nullptr;

  if (combinator == CSSSelector::kUAShadow) {
    descendant_features.invalidation_flags.SetTreeBoundaryCrossing(true);
  }
  if (combinator == CSSSelector::kShadowSlot) {
    descendant_features.invalidation_flags.SetInsertionPointCrossing(true);
  }
}

// A rule like @scope (.a) { .b {} } needs features equivalent to
// :is (.a .b, .a.b), because the scope established by @scope *includes* the
// scoping root. This function provides .a.b, i.e. the second part of the :is().
// The first part is handled by `AddFeaturesToInvalidationSetsForStyleScope`.
template <RuleInvalidationDataVisitorType VisitorType>
void RuleInvalidationDataVisitor<VisitorType>::UpdateFeaturesFromStyleScope(
    const StyleScope& style_scope,
    InvalidationSetFeatures& descendant_features) {
  auto add_features = [this](const CSSSelector* selector_list,
                             InvalidationSetFeatures& descendant_features) {
    for (const CSSSelector* selector = selector_list; selector;
         selector = CSSSelectorList::Next(*selector)) {
      InvalidationSetFeatures scope_features;
      ExtractInvalidationSetFeaturesFromCompound(
          *selector, scope_features, kSubject,
          /* for_logical_combination_in_has */ false, /*in_nth_child=*/false);
      descendant_features.Merge(scope_features);
    }
  };

  for (const StyleScope* scope = &style_scope; scope; scope = scope->Parent()) {
    add_features(scope->From(), descendant_features);
    add_features(scope->To(), descendant_features);
  }
}

template <RuleInvalidationDataVisitorType VisitorType>
void RuleInvalidationDataVisitor<VisitorType>::
    ExtractInvalidationSetFeaturesFromSimpleSelector(
        const CSSSelector& selector,
        InvalidationSetFeatures& features) {
  features.has_features_for_rule_set_invalidation |=
      selector.IsIdClassOrAttributeSelector();

  if (selector.Match() == CSSSelector::kTag &&
      selector.TagQName().LocalName() != CSSSelector::UniversalSelectorAtom()) {
    features.NarrowToTag(selector.TagQName().LocalName());
    return;
  }
  if (selector.Match() == CSSSelector::kId) {
    features.NarrowToId(selector.Value());
    return;
  }
  if (selector.Match() == CSSSelector::kClass) {
    features.NarrowToClass(selector.Value());
    return;
  }
  if (selector.IsAttributeSelector()) {
    features.NarrowToAttribute(selector.Attribute().LocalName());
    return;
  }
  switch (selector.GetPseudoType()) {
    case CSSSelector::kPseudoWebKitCustomElement:
    case CSSSelector::kPseudoBlinkInternalElement:
      features.invalidation_flags.SetInvalidateCustomPseudo(true);
      return;
    case CSSSelector::kPseudoSlotted:
      features.invalidation_flags.SetInvalidatesSlotted(true);
      return;
    case CSSSelector::kPseudoPart:
      features.invalidation_flags.SetInvalidatesParts(true);
      features.invalidation_flags.SetTreeBoundaryCrossing(true);
      return;
    default:
      return;
  }
}

// Extract invalidation set features and return a pointer to the the last
// simple selector of the compound, or nullptr if one of the selectors
// RequiresSubtreeInvalidation().
//
// It also deals with inserting self-invalidation entries for the compound
// itself, so it is not a pure “extract“ despite the name.
template <RuleInvalidationDataVisitorType VisitorType>
const CSSSelector* RuleInvalidationDataVisitor<VisitorType>::
    ExtractInvalidationSetFeaturesFromCompound(
        const CSSSelector& compound,
        InvalidationSetFeatures& features,
        PositionType position,
        bool for_logical_combination_in_has,
        bool in_nth_child) {
  // NOTE: Due to the check at the bottom of the loop, this loop stops
  // once we are at the end of the compound, ie., we see a relation that
  // is not a sub-selector. So for e.g. .a .b.c#d, we will see #d, .c, .b
  // and then stop, returning a pointer to .b.
  const CSSSelector* simple_selector = &compound;
  for (;; simple_selector = simple_selector->NextSimpleSelector()) {
    // Fall back to use subtree invalidations, even for features in the
    // rightmost compound selector. Returning nullptr here will make
    // addFeaturesToInvalidationSets start marking invalidation sets for
    // subtree recalc for features in the rightmost compound selector.
    if (RequiresSubtreeInvalidation(*simple_selector)) {
      features.invalidation_flags.SetWholeSubtreeInvalid(true);
      return nullptr;
    }

    ExtractInvalidationSetFeaturesFromSimpleSelector(*simple_selector,
                                                     features);

    // Initialize the entry in the invalidation set map for self-
    // invalidation, if supported.
    if (InvalidationSetType* invalidation_set =
            InvalidationSetForSimpleSelector(
                *simple_selector, InvalidationType::kInvalidateDescendants,
                position, in_nth_child)) {
      if (invalidation_set == rule_invalidation_data_.nth_invalidation_set) {
        features.has_nth_pseudo = true;
      } else if (position == kSubject) {
        SetInvalidatesSelf(invalidation_set);

        // If we are within :nth-child(), it means we'll need nth-child
        // invalidation for anything within this subject; see RuleFeatureSet
        // class comment.
        if (in_nth_child) {
          SetInvalidatesNth(invalidation_set);
        }
      }
    }

    ExtractInvalidationSetFeaturesFromSelectorList(
        *simple_selector, in_nth_child, features, position);

    if constexpr (is_builder()) {
      if (features.invalidation_flags.InvalidatesParts()) {
        rule_invalidation_data_.invalidates_parts = true;
      }
    }

    // While adding features to invalidation sets for logical combinations
    // inside :has(), ExtractInvalidationSetFeaturesFromCompound() can be
    // called again to extract features from the compound selector containing
    // the :has() pseudo class. (e.g. '.a:has(:is(.b ~ .c)) .d')
    // To avoid infinite recursive call, skip adding features for :has() if
    // ExtractInvalidationSetFeaturesFromCompound() is invoked for the logical
    // combinations inside :has().
    if (simple_selector->GetPseudoType() == CSSSelector::kPseudoHas &&
        !for_logical_combination_in_has) {
      CollectValuesInHasArgument(*simple_selector);
      AddFeaturesToInvalidationSetsForHasPseudoClass(
          *simple_selector, &compound, nullptr, features, in_nth_child);
    }

    if (!simple_selector->NextSimpleSelector() ||
        (simple_selector->Relation() != CSSSelector::kSubSelector &&
         simple_selector->Relation() != CSSSelector::kScopeActivation)) {
      return simple_selector;
    }
  }
}

template <RuleInvalidationDataVisitorType VisitorType>
void RuleInvalidationDataVisitor<VisitorType>::
    ExtractInvalidationSetFeaturesFromSelectorList(
        const CSSSelector& simple_selector,
        bool in_nth_child,
        InvalidationSetFeatures& features,
        PositionType position) {
  AutoRestoreMaxDirectAdjacentSelectors restore_max(&features);
  AutoRestoreDescendantFeaturesDepth restore_depth(&features);

  const CSSSelector* sub_selector = simple_selector.SelectorListOrParent();
  if (!sub_selector) {
    return;
  }
  CSSSelector::PseudoType pseudo_type = simple_selector.GetPseudoType();

  // For the :has pseudo class, we should not extract invalidation set features
  // here because the :has invalidation direction is different with others.
  // (preceding-sibling/ancestors/preceding-sibling-of-ancestors)
  if (pseudo_type == CSSSelector::kPseudoHas) {
    return;
  }

  in_nth_child |= pseudo_type == CSSSelector::kPseudoNthChild;
  in_nth_child |= pseudo_type == CSSSelector::kPseudoNthLastChild;

  DCHECK(SupportsInvalidationWithSelectorList(pseudo_type));

  bool all_sub_selectors_have_features = true;
  InvalidationSetFeatures any_features;

  for (; sub_selector; sub_selector = CSSSelectorList::Next(*sub_selector)) {
    InvalidationSetFeatures complex_features;
    if (UpdateInvalidationSetsForComplex(
            *sub_selector, in_nth_child, nullptr /* style_scope */,
            complex_features, position,
            pseudo_type) == kRequiresSubtreeInvalidation) {
      features.invalidation_flags.SetWholeSubtreeInvalid(true);
      continue;
    }
    if (complex_features.has_nth_pseudo) {
      features.has_nth_pseudo = true;
    }
    if (!all_sub_selectors_have_features) {
      continue;
    }
    if (complex_features.HasFeatures()) {
      any_features.Merge(complex_features);
    } else {
      all_sub_selectors_have_features = false;
    }
  }
  // Don't add any features if one of the sub-selectors of does not contain
  // any invalidation set features. E.g. :-webkit-any(*, span).
  //
  // For the :not() pseudo class, we should not use the inner features for
  // invalidation because we should invalidate elements _without_ that
  // feature. On the other hand, we should still have invalidation sets
  // for the features since we are able to detect when they change.
  // That is, ".a" should not have ".b" in its invalidation set for
  // ".a :not(.b)", but there should be an invalidation set for ".a" in
  // ":not(.a) .b".
  if (pseudo_type != CSSSelector::kPseudoNot) {
    if (all_sub_selectors_have_features) {
      features.NarrowToFeatures(any_features);
    }
  }
}

template <RuleInvalidationDataVisitorType VisitorType>
void RuleInvalidationDataVisitor<VisitorType>::AddFeaturesToInvalidationSets(
    const CSSSelector& selector,
    bool in_nth_child,
    InvalidationSetFeatures* sibling_features,
    InvalidationSetFeatures& descendant_features) {
  // selector is the selector immediately to the left of the rightmost
  // combinator. descendant_features has the features of the rightmost compound
  // selector.

  InvalidationSetFeatures last_compound_in_sibling_chain_features;
  const CSSSelector* compound = &selector;
  while (compound) {
    const CSSSelector* last_in_compound =
        AddFeaturesToInvalidationSetsForCompoundSelector(
            *compound, in_nth_child, sibling_features, descendant_features);
    DCHECK(last_in_compound);
    UpdateFeaturesFromCombinator(last_in_compound->Relation(), compound,
                                 last_compound_in_sibling_chain_features,
                                 sibling_features, descendant_features,
                                 /* for_logical_combination_in_has */ false,
                                 in_nth_child);
    compound = last_in_compound->NextSimpleSelector();
  }
}

template <RuleInvalidationDataVisitorType VisitorType>
const CSSSelector* RuleInvalidationDataVisitor<VisitorType>::
    AddFeaturesToInvalidationSetsForCompoundSelector(
        const CSSSelector& compound,
        bool in_nth_child,
        InvalidationSetFeatures* sibling_features,
        InvalidationSetFeatures& descendant_features) {
  bool compound_has_features_for_rule_set_invalidation = false;
  const CSSSelector* simple_selector = &compound;
  for (; simple_selector;
       simple_selector = simple_selector->NextSimpleSelector()) {
    base::AutoReset<bool> reset_has_features(
        &descendant_features.has_features_for_rule_set_invalidation, false);
    AddFeaturesToInvalidationSetsForSimpleSelector(
        *simple_selector, compound, in_nth_child, sibling_features,
        descendant_features);
    if (descendant_features.has_features_for_rule_set_invalidation) {
      compound_has_features_for_rule_set_invalidation = true;
    }
    if (simple_selector->Relation() != CSSSelector::kSubSelector) {
      break;
    }
    if (!simple_selector->NextSimpleSelector()) {
      break;
    }
  }

  if (compound_has_features_for_rule_set_invalidation) {
    descendant_features.has_features_for_rule_set_invalidation = true;
  } else if (sibling_features) {
    AddFeaturesToUniversalSiblingInvalidationSet(*sibling_features,
                                                 descendant_features);
  }

  return simple_selector;
}

template <RuleInvalidationDataVisitorType VisitorType>
void RuleInvalidationDataVisitor<VisitorType>::
    AddFeaturesToInvalidationSetsForSimpleSelector(
        const CSSSelector& simple_selector,
        const CSSSelector& compound,
        bool in_nth_child,
        InvalidationSetFeatures* sibling_features,
        InvalidationSetFeatures& descendant_features) {
  if (simple_selector.IsIdClassOrAttributeSelector()) {
    descendant_features.has_features_for_rule_set_invalidation = true;
  }

  CSSSelector::PseudoType pseudo_type = simple_selector.GetPseudoType();

  if (pseudo_type == CSSSelector::kPseudoHas) {
    CollectValuesInHasArgument(simple_selector);
    AddFeaturesToInvalidationSetsForHasPseudoClass(
        simple_selector, &compound, sibling_features, descendant_features,
        in_nth_child);
    if (simple_selector.HasArgumentMatchInShadowTree()) {
      descendant_features.invalidation_flags.SetTreeBoundaryCrossing(true);
    }
  }

  if (InvalidationSetType* invalidation_set = InvalidationSetForSimpleSelector(
          simple_selector,
          sibling_features ? InvalidationType::kInvalidateSiblings
                           : InvalidationType::kInvalidateDescendants,
          kAncestor, in_nth_child)) {
    if (!sibling_features) {
      if (invalidation_set == rule_invalidation_data_.nth_invalidation_set) {
        // TODO(futhark): We can extract the features from the current compound
        // to optimize this.
        SetWholeSubtreeInvalid(invalidation_set);
        AddFeaturesToInvalidationSet(
            EnsureSiblingDescendantInvalidationSet(
                To<SiblingInvalidationSet>(invalidation_set)),
            descendant_features);
        return;
      } else {
        AddFeaturesToInvalidationSet(invalidation_set, descendant_features);
        return;
      }
    }

    auto* sibling_invalidation_set =
        To<SiblingInvalidationSet>(invalidation_set);
    UpdateMaxDirectAdjacentSelectors(
        sibling_invalidation_set,
        sibling_features->max_direct_adjacent_selectors);
    AddFeaturesToInvalidationSet(invalidation_set, *sibling_features);
    if (sibling_features == &descendant_features) {
      SetInvalidatesSelf(sibling_invalidation_set);
      if (in_nth_child) {
        SetInvalidatesNth(sibling_invalidation_set);
      }
    } else {
      AddFeaturesToInvalidationSet(
          EnsureSiblingDescendantInvalidationSet(sibling_invalidation_set),
          descendant_features);
    }
    return;
  }

  // For the :has pseudo class, we should not extract invalidation set features
  // here because the :has invalidation direction is different with others.
  // (preceding-sibling/ancestors/preceding-sibling-of-ancestors)
  if (pseudo_type == CSSSelector::kPseudoHas) {
    return;
  }

  if (pseudo_type == CSSSelector::kPseudoPart) {
    descendant_features.invalidation_flags.SetInvalidatesParts(true);
  }

  AddFeaturesToInvalidationSetsForSelectorList(
      simple_selector, in_nth_child, sibling_features, descendant_features);
}

template <RuleInvalidationDataVisitorType VisitorType>
void RuleInvalidationDataVisitor<VisitorType>::
    AddFeaturesToInvalidationSetsForSelectorList(
        const CSSSelector& simple_selector,
        bool in_nth_child,
        InvalidationSetFeatures* sibling_features,
        InvalidationSetFeatures& descendant_features) {
  if (!simple_selector.SelectorListOrParent()) {
    return;
  }

  DCHECK(SupportsInvalidationWithSelectorList(simple_selector.GetPseudoType()));

  bool had_features_for_rule_set_invalidation =
      descendant_features.has_features_for_rule_set_invalidation;
  bool selector_list_contains_universal =
      simple_selector.GetPseudoType() == CSSSelector::kPseudoNot ||
      simple_selector.GetPseudoType() == CSSSelector::kPseudoHostContext;
  in_nth_child |=
      simple_selector.GetPseudoType() == CSSSelector::kPseudoNthChild;
  in_nth_child |=
      simple_selector.GetPseudoType() == CSSSelector::kPseudoNthLastChild;

  for (const CSSSelector* sub_selector = simple_selector.SelectorListOrParent();
       sub_selector; sub_selector = CSSSelectorList::Next(*sub_selector)) {
    AutoRestoreMaxDirectAdjacentSelectors restore_max(sibling_features);
    AutoRestoreDescendantFeaturesDepth restore_depth(&descendant_features);
    AutoRestoreTreeBoundaryCrossingFlag restore_tree_boundary(
        descendant_features);
    AutoRestoreInsertionPointCrossingFlag restore_insertion_point(
        descendant_features);

    if (simple_selector.IsHostPseudoClass()) {
      descendant_features.invalidation_flags.SetTreeBoundaryCrossing(true);
    }

    descendant_features.has_features_for_rule_set_invalidation = false;

    AddFeaturesToInvalidationSets(*sub_selector, in_nth_child, sibling_features,
                                  descendant_features);

    if (!descendant_features.has_features_for_rule_set_invalidation) {
      selector_list_contains_universal = true;
    }
  }

  descendant_features.has_features_for_rule_set_invalidation =
      had_features_for_rule_set_invalidation ||
      !selector_list_contains_universal;
}

// See also UpdateFeaturesFromStyleScope.
template <RuleInvalidationDataVisitorType VisitorType>
void RuleInvalidationDataVisitor<VisitorType>::
    AddFeaturesToInvalidationSetsForStyleScope(
        const StyleScope& style_scope,
        InvalidationSetFeatures& descendant_features) {
  auto add_features = [this](const CSSSelector& selector_list,
                             InvalidationSetFeatures& features) {
    for (const CSSSelector* selector = &selector_list; selector;
         selector = CSSSelectorList::Next(*selector)) {
      AddFeaturesToInvalidationSets(*selector, /*in_nth_child=*/false,
                                    nullptr /* sibling_features */, features);
    }
  };

  for (const StyleScope* scope = &style_scope; scope; scope = scope->Parent()) {
    if (scope->From()) {
      add_features(*scope->From(), descendant_features);
    }

    if (scope->To()) {
      add_features(*scope->To(), descendant_features);
    }
  }
}

template <RuleInvalidationDataVisitorType VisitorType>
void RuleInvalidationDataVisitor<VisitorType>::
    AddFeaturesToUniversalSiblingInvalidationSet(
        const InvalidationSetFeatures& sibling_features,
        const InvalidationSetFeatures& descendant_features) {
  SiblingInvalidationSetType* universal_set =
      EnsureUniversalSiblingInvalidationSet();
  AddFeaturesToInvalidationSet(universal_set, sibling_features);
  UpdateMaxDirectAdjacentSelectors(
      universal_set, sibling_features.max_direct_adjacent_selectors);

  if (&sibling_features == &descendant_features) {
    SetInvalidatesSelf(universal_set);
  } else {
    AddFeaturesToInvalidationSet(
        EnsureSiblingDescendantInvalidationSet(universal_set),
        descendant_features);
  }
}

template <RuleInvalidationDataVisitorType VisitorType>
void RuleInvalidationDataVisitor<VisitorType>::
    AddValuesInComplexSelectorInsideIsWhereNot(
        const CSSSelector* selector_first) {
  for (const CSSSelector* complex = selector_first; complex;
       complex = CSSSelectorList::Next(*complex)) {
    DCHECK(complex);

    for (const CSSSelector* simple = complex; simple;
         simple = simple->NextSimpleSelector()) {
      AddValueOfSimpleSelectorInHasArgument(*simple);
    }
  }
}

template <RuleInvalidationDataVisitorType VisitorType>
bool RuleInvalidationDataVisitor<VisitorType>::
    AddValueOfSimpleSelectorInHasArgument(const CSSSelector& selector) {
  if (selector.Match() == CSSSelector::kClass) {
    if constexpr (is_builder()) {
      rule_invalidation_data_.classes_in_has_argument.insert(selector.Value());
    }
    return true;
  }
  if (selector.IsAttributeSelector()) {
    if constexpr (is_builder()) {
      rule_invalidation_data_.attributes_in_has_argument.insert(
          selector.Attribute().LocalName());
    }
    return true;
  }
  if (selector.Match() == CSSSelector::kId) {
    if constexpr (is_builder()) {
      rule_invalidation_data_.ids_in_has_argument.insert(selector.Value());
    }
    return true;
  }
  if (selector.Match() == CSSSelector::kTag &&
      selector.TagQName().LocalName() != CSSSelector::UniversalSelectorAtom()) {
    if constexpr (is_builder()) {
      rule_invalidation_data_.tag_names_in_has_argument.insert(
          selector.TagQName().LocalName());
    }
    return true;
  }
  if (selector.Match() == CSSSelector::kPseudoClass) {
    CSSSelector::PseudoType pseudo_type = selector.GetPseudoType();

    switch (pseudo_type) {
      case CSSSelector::kPseudoNot:
        if constexpr (is_builder()) {
          rule_invalidation_data_.not_pseudo_in_has_argument = true;
        }
        [[fallthrough]];
      case CSSSelector::kPseudoIs:
      case CSSSelector::kPseudoWhere:
      case CSSSelector::kPseudoParent:
        AddValuesInComplexSelectorInsideIsWhereNot(
            selector.SelectorListOrParent());
        break;
      case CSSSelector::kPseudoVisited:
        // Ignore :visited to prevent history leakage.
        break;
      default:
        if constexpr (is_builder()) {
          rule_invalidation_data_.pseudos_in_has_argument.insert(pseudo_type);
        }
        break;
    }
    return true;
  }
  return false;
}

template <RuleInvalidationDataVisitorType VisitorType>
void RuleInvalidationDataVisitor<VisitorType>::CollectValuesInHasArgument(
    const CSSSelector& has_pseudo_class) {
  DCHECK_EQ(has_pseudo_class.GetPseudoType(), CSSSelector::kPseudoHas);
  const CSSSelectorList* selector_list = has_pseudo_class.SelectorList();
  DCHECK(selector_list);

  for (const CSSSelector* relative_selector = selector_list->First();
       relative_selector;
       relative_selector = CSSSelectorList::Next(*relative_selector)) {
    DCHECK(relative_selector);

    bool value_added = false;
    const CSSSelector* simple = relative_selector;
    while (simple->GetPseudoType() != CSSSelector::kPseudoRelativeAnchor) {
      value_added |= AddValueOfSimpleSelectorInHasArgument(*simple);

      if (simple->Relation() != CSSSelector::kSubSelector) {
        if (!value_added) {
          if constexpr (is_builder()) {
            rule_invalidation_data_.universal_in_has_argument = true;
          }
        }
        value_added = false;
      }

      simple = simple->NextSimpleSelector();
      DCHECK(simple);
    }
  }
}

template <RuleInvalidationDataVisitorType VisitorType>
void RuleInvalidationDataVisitor<VisitorType>::
    AddFeaturesToInvalidationSetsForHasPseudoClass(
        const CSSSelector& pseudo_has,
        const CSSSelector* compound_containing_has,
        InvalidationSetFeatures* sibling_features,
        InvalidationSetFeatures& descendant_features,
        bool in_nth_child) {
  DCHECK(compound_containing_has);
  DCHECK_EQ(pseudo_has.GetPseudoType(), CSSSelector::kPseudoHas);

  if (in_nth_child) {
    if constexpr (is_builder()) {
      rule_invalidation_data_.uses_has_inside_nth = true;
    }
  }

  // Add features to invalidation sets only when the :has() pseudo class
  // contains logical combinations containing a complex selector as argument.
  if (!pseudo_has.ContainsComplexLogicalCombinationsInsideHasPseudoClass()) {
    return;
  }

  // Set descendant features as WholeSubtreeInvalid if the descendant features
  // haven't been extracted yet. (e.g. '.a :has(:is(.b .c)).d {}')
  AutoRestoreWholeSubtreeInvalid restore_whole_subtree(descendant_features);
  if (!descendant_features.HasFeatures()) {
    descendant_features.invalidation_flags.SetWholeSubtreeInvalid(true);
  }

  // Use descendant features as sibling features if the :has() pseudo class is
  // in subject position.
  if (!sibling_features && descendant_features.descendant_features_depth == 0) {
    sibling_features = &descendant_features;
  }

  DCHECK(pseudo_has.SelectorList());

  for (const CSSSelector* relative = pseudo_has.SelectorList()->First();
       relative; relative = CSSSelectorList::Next(*relative)) {
    for (const CSSSelector* simple = relative;
         simple->GetPseudoType() != CSSSelector::kPseudoRelativeAnchor;
         simple = simple->NextSimpleSelector()) {
      switch (simple->GetPseudoType()) {
        case CSSSelector::kPseudoIs:
        case CSSSelector::kPseudoWhere:
        case CSSSelector::kPseudoNot:
        case CSSSelector::kPseudoParent:
          // Add features for each method to handle sibling descendant
          // relationship in the logical combination.
          // - For '.a:has(:is(.b ~ .c .d))',
          //   -> '.b ~ .c .a' (kForAllNonRightmostCompounds)
          //   -> '.b ~ .a' (kForCompoundImmediatelyFollowsAdjacentRelation)
          AddFeaturesToInvalidationSetsForLogicalCombinationInHas(
              *simple, compound_containing_has, sibling_features,
              descendant_features, CSSSelector::kSubSelector,
              kForAllNonRightmostCompounds);
          AddFeaturesToInvalidationSetsForLogicalCombinationInHas(
              *simple, compound_containing_has, sibling_features,
              descendant_features, CSSSelector::kSubSelector,
              kForCompoundImmediatelyFollowsAdjacentRelation);
          break;
        default:
          break;
      }
    }
  }
}

// Context for adding features for a compound selector in a logical combination
// inside :has(). This struct provides these information so that the features
// can be added correctly for the compound in logical combination.
// - needs_skip_adding_features:
//     - whether adding features needs to be skipped.
// - needs_update_features:
//     - whether updating features is needed.
// - last_compound_in_adjacent_chain:
//     - last compound in adjacent chain used for updating features.
// - use_indirect_adjacent_combinator_for_updating_features:
//     - whether we need to use adjacent combinator for updating features.
// Please check the comments in the constructor for more details.
template <RuleInvalidationDataVisitorType VisitorType>
struct RuleInvalidationDataVisitor<VisitorType>::
    AddFeaturesToInvalidationSetsForLogicalCombinationInHasContext {
  bool needs_skip_adding_features;
  bool needs_update_features;
  const CSSSelector* last_compound_in_adjacent_chain;
  bool use_indirect_adjacent_combinator_for_updating_features;

  AddFeaturesToInvalidationSetsForLogicalCombinationInHasContext(
      const CSSSelector* compound_in_logical_combination,
      const CSSSelector* compound_containing_has,
      CSSSelector::RelationType previous_combinator,
      AddFeaturesMethodForLogicalCombinationInHas add_features_method) {
    last_compound_in_adjacent_chain = nullptr;
    needs_skip_adding_features = false;
    needs_update_features = false;
    use_indirect_adjacent_combinator_for_updating_features = false;

    bool is_in_has_argument_checking_scope =
        previous_combinator == CSSSelector::kSubSelector;
    bool add_features_for_compound_immediately_follows_adjacent_relation =
        add_features_method == kForCompoundImmediatelyFollowsAdjacentRelation;

    if (is_in_has_argument_checking_scope) {
      // If the compound in the logical combination is for the element in the
      // :has() argument checking scope, skip adding features.
      needs_skip_adding_features = true;

      // If the compound in the logical combination is for the element in the
      // :has() argument checking scope, update features before moving to the
      // next compound.
      needs_update_features = true;

      // For the rightmost compound that need to be skipped, use the compound
      // selector containing :has() as last_compound_in_adjacent_chain for
      // updating features so that the features can be added as if the next
      // compounds are prepended to the compound containing :has().
      // (e.g. '.a:has(:is(.b .c ~ .d)) .e' -> '.b .c ~ .a .e')
      // The selector pointer of '.a:has(:is(.b .c ~ .d))' is passed though
      // the argument 'compound_containing_has'.
      last_compound_in_adjacent_chain = compound_containing_has;

      // In case of adding features only for adjacent combinator and its
      // next compound selector, update features as if the relation of the
      // last-in-compound is indirect adjacent combinator ('~').
      if (add_features_for_compound_immediately_follows_adjacent_relation) {
        use_indirect_adjacent_combinator_for_updating_features = true;
      }
    } else {
      // If this method call is for the compound immediately follows an
      // adjacent combinator in the logical combination but the compound
      // doesn't follow any adjacent combinator, skip adding features.
      if (add_features_for_compound_immediately_follows_adjacent_relation &&
          !CSSSelector::IsAdjacentRelation(previous_combinator)) {
        needs_skip_adding_features = true;
      }

      // Update features from the previous combinator when we add features
      // for all non-rightmost compound selectors. In case of adding features
      // only for adjacent combinator and its next compound selector, do not
      // update features so that we can use the same features that was
      // updated at the compound in :has() argument checking scope.
      if (add_features_method == kForAllNonRightmostCompounds) {
        needs_update_features = true;
      }

      last_compound_in_adjacent_chain = compound_in_logical_combination;
    }
  }
};

template <RuleInvalidationDataVisitorType VisitorType>
void RuleInvalidationDataVisitor<VisitorType>::
    AddFeaturesToInvalidationSetsForLogicalCombinationInHas(
        const CSSSelector& logical_combination,
        const CSSSelector* compound_containing_has,
        InvalidationSetFeatures* sibling_features,
        InvalidationSetFeatures& descendant_features,
        CSSSelector::RelationType previous_combinator,
        AddFeaturesMethodForLogicalCombinationInHas add_features_method) {
  DCHECK(compound_containing_has);

  for (const CSSSelector* complex = logical_combination.SelectorListOrParent();
       complex; complex = CSSSelectorList::Next(*complex)) {
    base::AutoReset<CSSSelector::RelationType> restore_previous_combinator(
        &previous_combinator, previous_combinator);
    AutoRestoreMaxDirectAdjacentSelectors restore_max(sibling_features);
    AutoRestoreDescendantFeaturesDepth restore_depth(&descendant_features);
    AutoRestoreTreeBoundaryCrossingFlag restore_tree_boundary(
        descendant_features);
    AutoRestoreInsertionPointCrossingFlag restore_insertion_point(
        descendant_features);

    const CSSSelector* compound_in_logical_combination = complex;
    InvalidationSetFeatures* inner_sibling_features = sibling_features;
    InvalidationSetFeatures last_compound_in_adjacent_chain_features;
    while (compound_in_logical_combination) {
      AddFeaturesToInvalidationSetsForLogicalCombinationInHasContext context(
          compound_in_logical_combination, compound_containing_has,
          previous_combinator, add_features_method);

      const CSSSelector* last_in_compound;
      if (context.needs_skip_adding_features) {
        last_in_compound =
            SkipAddingAndGetLastInCompoundForLogicalCombinationInHas(
                compound_in_logical_combination, compound_containing_has,
                inner_sibling_features, descendant_features,
                previous_combinator, add_features_method);
      } else {
        last_in_compound =
            AddFeaturesAndGetLastInCompoundForLogicalCombinationInHas(
                compound_in_logical_combination, compound_containing_has,
                inner_sibling_features, descendant_features,
                previous_combinator, add_features_method);
      }

      if (!last_in_compound) {
        break;
      }

      previous_combinator = last_in_compound->Relation();

      if (context.needs_update_features) {
        UpdateFeaturesFromCombinatorForLogicalCombinationInHas(
            context.use_indirect_adjacent_combinator_for_updating_features
                ? CSSSelector::kIndirectAdjacent
                : previous_combinator,
            context.last_compound_in_adjacent_chain,
            last_compound_in_adjacent_chain_features, inner_sibling_features,
            descendant_features);
      }

      compound_in_logical_combination = last_in_compound->NextSimpleSelector();
    }
  }
}

template <RuleInvalidationDataVisitorType VisitorType>
void RuleInvalidationDataVisitor<VisitorType>::
    UpdateFeaturesFromCombinatorForLogicalCombinationInHas(
        CSSSelector::RelationType combinator,
        const CSSSelector* last_compound_in_adjacent_chain,
        InvalidationSetFeatures& last_compound_in_adjacent_chain_features,
        InvalidationSetFeatures*& sibling_features,
        InvalidationSetFeatures& descendant_features) {
  // Always use indirect relation to add features to invalidation sets for
  // logical combinations inside :has() since it is too difficult to limit
  // invalidation distance by counting successive indirect relations in the
  // logical combinations inside :has().
  // (e.g. '.a:has(:is(:is(.a > .b) .c)) {}', '.a:has(~ :is(.b + .c + .d)) {}'
  switch (combinator) {
    case CSSSelector::CSSSelector::kDescendant:
    case CSSSelector::CSSSelector::kChild:
      combinator = CSSSelector::kDescendant;
      break;
    case CSSSelector::CSSSelector::kDirectAdjacent:
    case CSSSelector::CSSSelector::kIndirectAdjacent:
      combinator = CSSSelector::kIndirectAdjacent;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      return;
  }

  UpdateFeaturesFromCombinator(combinator, last_compound_in_adjacent_chain,
                               last_compound_in_adjacent_chain_features,
                               sibling_features, descendant_features,
                               /* for_logical_combination_in_has */ true,
                               /*in_nth_child=*/false);
}

template <RuleInvalidationDataVisitorType VisitorType>
const CSSSelector* RuleInvalidationDataVisitor<VisitorType>::
    SkipAddingAndGetLastInCompoundForLogicalCombinationInHas(
        const CSSSelector* compound_in_logical_combination,
        const CSSSelector* compound_containing_has,
        InvalidationSetFeatures* sibling_features,
        InvalidationSetFeatures& descendant_features,
        CSSSelector::RelationType previous_combinator,
        AddFeaturesMethodForLogicalCombinationInHas add_features_method) {
  const CSSSelector* simple = compound_in_logical_combination;
  for (; simple; simple = simple->NextSimpleSelector()) {
    switch (simple->GetPseudoType()) {
      case CSSSelector::kPseudoIs:
      case CSSSelector::kPseudoWhere:
      case CSSSelector::kPseudoNot:
      case CSSSelector::kPseudoParent:
        // Nested logical combinations in rightmost compound of a first-depth
        // logical combination inside :has()
        // (e.g. '.a:has(.a :is(.b :is(.c .d))) {}')
        AddFeaturesToInvalidationSetsForLogicalCombinationInHas(
            *simple, compound_containing_has, sibling_features,
            descendant_features, previous_combinator, add_features_method);
        break;
      default:
        break;
    }
    if (simple->Relation() != CSSSelector::kSubSelector) {
      break;
    }
  }
  return simple;
}

template <RuleInvalidationDataVisitorType VisitorType>
const CSSSelector* RuleInvalidationDataVisitor<VisitorType>::
    AddFeaturesAndGetLastInCompoundForLogicalCombinationInHas(
        const CSSSelector* compound_in_logical_combination,
        const CSSSelector* compound_containing_has,
        InvalidationSetFeatures* sibling_features,
        InvalidationSetFeatures& descendant_features,
        CSSSelector::RelationType previous_combinator,
        AddFeaturesMethodForLogicalCombinationInHas add_features_method) {
  DCHECK(compound_in_logical_combination);
  bool compound_has_features_for_rule_set_invalidation = false;
  const CSSSelector* simple = compound_in_logical_combination;

  for (; simple; simple = simple->NextSimpleSelector()) {
    base::AutoReset<bool> reset_has_features(
        &descendant_features.has_features_for_rule_set_invalidation, false);
    switch (simple->GetPseudoType()) {
      case CSSSelector::kPseudoIs:
      case CSSSelector::kPseudoWhere:
      case CSSSelector::kPseudoNot:
      case CSSSelector::kPseudoParent:
        // Nested logical combination inside :has()
        // (e.g. '.a:has(:is(:is(.a .b) .c)) {}')
        AddFeaturesToInvalidationSetsForLogicalCombinationInHas(
            *simple, compound_containing_has, sibling_features,
            descendant_features, previous_combinator, add_features_method);
        break;
      default:
        AddFeaturesToInvalidationSetsForSimpleSelector(
            *simple, *compound_in_logical_combination, /*in_nth_child=*/false,
            sibling_features, descendant_features);
        break;
    }
    if (descendant_features.has_features_for_rule_set_invalidation) {
      compound_has_features_for_rule_set_invalidation = true;
    }

    if (simple->Relation() != CSSSelector::kSubSelector) {
      break;
    }
  }

  // If the compound selector has features for invalidation, mark the
  // related flag in the descendant_features.
  // Otherwise add features to universal sibling invalidation set if
  // sibling_features exists. (e.g. '.a:has(:is(* .b)) ~ .c .d {}')
  if (compound_has_features_for_rule_set_invalidation) {
    descendant_features.has_features_for_rule_set_invalidation = true;
  } else if (sibling_features) {
    AddFeaturesToUniversalSiblingInvalidationSet(*sibling_features,
                                                 descendant_features);
  }

  return simple;
}

template <RuleInvalidationDataVisitorType VisitorType>
void RuleInvalidationDataVisitor<VisitorType>::
    MarkInvalidationSetsWithinNthChild(const CSSSelector& selector,
                                       bool in_nth_child) {
  const CSSSelector* simple_selector = &selector;
  for (; simple_selector;
       simple_selector = simple_selector->NextSimpleSelector()) {
    if (in_nth_child) {
      if (InvalidationSetType* invalidation_set =
              InvalidationSetForSimpleSelector(
                  *simple_selector, InvalidationType::kInvalidateDescendants,
                  kAncestor, in_nth_child)) {
        // This is, strictly speaking, setting the bit on too many classes.
        // If we have a selector like :nth-child(.a .b) .c, there's no reason
        // to set the invalidates_nth_ bit on .a; what we need is that .b
        // has the bit, and that the descendant invalidation set for .a
        // contains .b (so that adding .a to some element causes us to go
        // looking for .b elements in that element's subtree), and we've
        // already done that in AddFeaturesToInvalidationSetsForSelectorList()
        // -- setting the bit on .a is not really doing much. So that would be a
        // potential future optimization if we find it useful. (We still need to
        // traverse the ancestor selectors, though, in case they contain other
        // :nth-child() selectors, recursively.)
        SetInvalidatesNth(invalidation_set);
      }
    }
    if (simple_selector->SelectorList()) {
      bool sub_in_nth_child =
          in_nth_child ||
          simple_selector->GetPseudoType() == CSSSelector::kPseudoNthChild ||
          simple_selector->GetPseudoType() == CSSSelector::kPseudoNthLastChild;
      MarkInvalidationSetsWithinNthChild(
          *simple_selector->SelectorList()->First(), sub_in_nth_child);
    }
  }
}

template <RuleInvalidationDataVisitorType VisitorType>
RuleInvalidationDataVisitor<VisitorType>::InvalidationSetType*
RuleInvalidationDataVisitor<VisitorType>::InvalidationSetForSimpleSelector(
    const CSSSelector& selector,
    InvalidationType type,
    PositionType position,
    bool in_nth_child) {
  if (selector.Match() == CSSSelector::kClass) {
    if (type == InvalidationType::kInvalidateDescendants &&
        position == kSubject && !in_nth_child &&
        InsertIntoSelfInvalidationBloomFilter(
            selector.Value(), RuleInvalidationData::kClassSalt)) {
      // Do not insert self-invalidation sets for classes;
      // see comment on class_invalidation_sets_.
      return nullptr;
    }
    return EnsureClassInvalidationSet(selector.Value(), type, position,
                                      in_nth_child);
  }
  if (selector.IsAttributeSelector()) {
    return EnsureAttributeInvalidationSet(selector.Attribute().LocalName(),
                                          type, position, in_nth_child);
  }
  if (selector.Match() == CSSSelector::kId) {
    if (type == InvalidationType::kInvalidateDescendants &&
        position == kSubject &&
        InsertIntoSelfInvalidationBloomFilter(selector.Value(),
                                              RuleInvalidationData::kIdSalt)) {
      // Do not insert self-invalidation sets for IDs;
      // see comment on class_invalidation_sets_.
      return nullptr;
    }
    return EnsureIdInvalidationSet(selector.Value(), type, position,
                                   in_nth_child);
  }
  if (selector.Match() == CSSSelector::kPseudoClass) {
    switch (selector.GetPseudoType()) {
      case CSSSelector::kPseudoEmpty:
      case CSSSelector::kPseudoFirstChild:
      case CSSSelector::kPseudoLastChild:
      case CSSSelector::kPseudoOnlyChild:
      case CSSSelector::kPseudoLink:
      case CSSSelector::kPseudoVisited:
      case CSSSelector::kPseudoWebkitAnyLink:
      case CSSSelector::kPseudoAnyLink:
      case CSSSelector::kPseudoAutofill:
      case CSSSelector::kPseudoWebKitAutofill:
      case CSSSelector::kPseudoAutofillPreviewed:
      case CSSSelector::kPseudoAutofillSelected:
      case CSSSelector::kPseudoHover:
      case CSSSelector::kPseudoDrag:
      case CSSSelector::kPseudoFocus:
      case CSSSelector::kPseudoFocusVisible:
      case CSSSelector::kPseudoFocusWithin:
      case CSSSelector::kPseudoActive:
      case CSSSelector::kPseudoChecked:
      case CSSSelector::kPseudoEnabled:
      case CSSSelector::kPseudoDefault:
      case CSSSelector::kPseudoDisabled:
      case CSSSelector::kPseudoOptional:
      case CSSSelector::kPseudoPlaceholderShown:
      case CSSSelector::kPseudoRequired:
      case CSSSelector::kPseudoReadOnly:
      case CSSSelector::kPseudoReadWrite:
      case CSSSelector::kPseudoState:
      case CSSSelector::kPseudoStateDeprecatedSyntax:
      case CSSSelector::kPseudoUserInvalid:
      case CSSSelector::kPseudoUserValid:
      case CSSSelector::kPseudoValid:
      case CSSSelector::kPseudoInvalid:
      case CSSSelector::kPseudoIndeterminate:
      case CSSSelector::kPseudoTarget:
      case CSSSelector::kPseudoLang:
      case CSSSelector::kPseudoDir:
      case CSSSelector::kPseudoFullScreen:
      case CSSSelector::kPseudoFullScreenAncestor:
      case CSSSelector::kPseudoFullscreen:
      case CSSSelector::kPseudoPaused:
      case CSSSelector::kPseudoPermissionElementInvalidStyle:
      case CSSSelector::kPseudoPermissionElementOccluded:
      case CSSSelector::kPseudoPermissionGranted:
      case CSSSelector::kPseudoPictureInPicture:
      case CSSSelector::kPseudoPlaying:
      case CSSSelector::kPseudoInRange:
      case CSSSelector::kPseudoOutOfRange:
      case CSSSelector::kPseudoDefined:
      case CSSSelector::kPseudoOpen:
      case CSSSelector::kPseudoClosed:
      case CSSSelector::kPseudoPopoverOpen:
      case CSSSelector::kPseudoVideoPersistent:
      case CSSSelector::kPseudoVideoPersistentAncestor:
      case CSSSelector::kPseudoXrOverlay:
      case CSSSelector::kPseudoHasDatalist:
      case CSSSelector::kPseudoMultiSelectFocus:
      case CSSSelector::kPseudoModal:
      case CSSSelector::kPseudoSelectorFragmentAnchor:
      case CSSSelector::kPseudoActiveViewTransition:
      case CSSSelector::kPseudoActiveViewTransitionType:
      case CSSSelector::kPseudoHasSlotted:
        return EnsurePseudoInvalidationSet(selector.GetPseudoType(), type,
                                           position, in_nth_child);
      case CSSSelector::kPseudoFirstOfType:
      case CSSSelector::kPseudoLastOfType:
      case CSSSelector::kPseudoOnlyOfType:
      case CSSSelector::kPseudoNthChild:
      case CSSSelector::kPseudoNthOfType:
      case CSSSelector::kPseudoNthLastChild:
      case CSSSelector::kPseudoNthLastOfType:
        return EnsureNthInvalidationSet();
      case CSSSelector::kPseudoHas:
        return position == kAncestor
                   ? EnsurePseudoInvalidationSet(selector.GetPseudoType(), type,
                                                 position, in_nth_child)
                   : nullptr;
      case CSSSelector::kPseudoPart:
      default:
        break;
    }
  }
  return nullptr;
}

template <RuleInvalidationDataVisitorType VisitorType>
RuleInvalidationDataVisitor<VisitorType>::InvalidationSetType*
RuleInvalidationDataVisitor<VisitorType>::EnsureClassInvalidationSet(
    const AtomicString& class_name,
    InvalidationType type,
    PositionType position,
    bool in_nth_child) {
  CHECK(!class_name.empty());
  return EnsureInvalidationSet(rule_invalidation_data_.class_invalidation_sets,
                               class_name, type, position, in_nth_child);
}

template <RuleInvalidationDataVisitorType VisitorType>
RuleInvalidationDataVisitor<VisitorType>::InvalidationSetType*
RuleInvalidationDataVisitor<VisitorType>::EnsureAttributeInvalidationSet(
    const AtomicString& attribute_name,
    InvalidationType type,
    PositionType position,
    bool in_nth_child) {
  CHECK(!attribute_name.empty());
  return EnsureInvalidationSet(
      rule_invalidation_data_.attribute_invalidation_sets, attribute_name, type,
      position, in_nth_child);
}

template <RuleInvalidationDataVisitorType VisitorType>
RuleInvalidationDataVisitor<VisitorType>::InvalidationSetType*
RuleInvalidationDataVisitor<VisitorType>::EnsureIdInvalidationSet(
    const AtomicString& id,
    InvalidationType type,
    PositionType position,
    bool in_nth_child) {
  CHECK(!id.empty());
  return EnsureInvalidationSet(rule_invalidation_data_.id_invalidation_sets, id,
                               type, position, in_nth_child);
}

template <RuleInvalidationDataVisitorType VisitorType>
RuleInvalidationDataVisitor<VisitorType>::InvalidationSetType*
RuleInvalidationDataVisitor<VisitorType>::EnsurePseudoInvalidationSet(
    CSSSelector::PseudoType pseudo_type,
    InvalidationType type,
    PositionType position,
    bool in_nth_child) {
  CHECK_NE(pseudo_type, CSSSelector::kPseudoUnknown);
  return EnsureInvalidationSet(rule_invalidation_data_.pseudo_invalidation_sets,
                               pseudo_type, type, position, in_nth_child);
}

template <RuleInvalidationDataVisitorType VisitorType>
RuleInvalidationDataVisitor<VisitorType>::InvalidationSetType*
RuleInvalidationDataVisitor<VisitorType>::EnsureInvalidationSet(
    InvalidationSetMapType& map,
    const AtomicString& key,
    InvalidationType type,
    PositionType position,
    bool in_nth_child) {
  if constexpr (is_builder()) {
    scoped_refptr<InvalidationSet>& invalidation_set =
        map.insert(key, nullptr).stored_value->value;
    return &EnsureMutableInvalidationSet(type, position, in_nth_child,
                                         invalidation_set);
  } else {
    auto it = map.find(key);
    if (it != map.end()) {
      const InvalidationSet* invalidation_set = it->value.get();
      if (invalidation_set->GetType() == type) {
        return invalidation_set;
      } else {
        // The caller wanted descendant and we found sibling+descendant.
        CHECK(type == InvalidationType::kInvalidateDescendants);
        return To<SiblingInvalidationSet>(invalidation_set)->Descendants();
      }
    }
    // It is possible for the Tracer not to find an InvalidationSet we expect to
    // be there. One case where this can happen is when, at the time we run the
    // Tracer, a rule has been added to a stylesheet but not yet indexed. In
    // such a case, we'll pick up information about the new rule as it gets
    // indexed on the next document lifecycle update.
    return nullptr;
  }
}

template <RuleInvalidationDataVisitorType VisitorType>
RuleInvalidationDataVisitor<VisitorType>::InvalidationSetType*
RuleInvalidationDataVisitor<VisitorType>::EnsureInvalidationSet(
    PseudoTypeInvalidationSetMapType& map,
    CSSSelector::PseudoType key,
    InvalidationType type,
    PositionType position,
    bool in_nth_child) {
  if constexpr (is_builder()) {
    scoped_refptr<InvalidationSet>& invalidation_set =
        map.insert(key, nullptr).stored_value->value;
    return &EnsureMutableInvalidationSet(type, position, in_nth_child,
                                         invalidation_set);
  } else {
    auto it = map.find(key);
    if (it != map.end()) {
      const InvalidationSet* invalidation_set = it->value.get();
      if (invalidation_set->GetType() == type) {
        return invalidation_set;
      } else {
        // The caller wanted descendant and we found sibling+descendant.
        CHECK(type == InvalidationType::kInvalidateDescendants);
        return To<SiblingInvalidationSet>(invalidation_set)->Descendants();
      }
    }
    // It is possible for the Tracer not to find an InvalidationSet we expect to
    // be there. One case where this can happen is when, at the time we run the
    // Tracer, a rule has been added to a stylesheet but not yet indexed. In
    // such a case, we'll pick up information about the new rule as it gets
    // indexed on the next document lifecycle update.
    return nullptr;
  }
}

template <RuleInvalidationDataVisitorType VisitorType>
RuleInvalidationDataVisitor<VisitorType>::SiblingInvalidationSetType*
RuleInvalidationDataVisitor<
    VisitorType>::EnsureUniversalSiblingInvalidationSet() {
  if constexpr (is_builder()) {
    if (!rule_invalidation_data_.universal_sibling_invalidation_set) {
      rule_invalidation_data_.universal_sibling_invalidation_set =
          SiblingInvalidationSet::Create(nullptr);
    }
  }
  return rule_invalidation_data_.universal_sibling_invalidation_set.get();
}

template <RuleInvalidationDataVisitorType VisitorType>
RuleInvalidationDataVisitor<VisitorType>::SiblingInvalidationSetType*
RuleInvalidationDataVisitor<VisitorType>::EnsureNthInvalidationSet() {
  if constexpr (is_builder()) {
    if (!rule_invalidation_data_.nth_invalidation_set) {
      rule_invalidation_data_.nth_invalidation_set =
          NthSiblingInvalidationSet::Create();
    }
  }
  return rule_invalidation_data_.nth_invalidation_set.get();
}

// Add features extracted from the rightmost compound selector to descendant
// invalidation sets for features found in other compound selectors.
//
// We use descendant invalidation for descendants, sibling invalidation for
// siblings and their subtrees.
//
// As we encounter a descendant type of combinator, the features only need to be
// checked against descendants in the same subtree only. features.adjacent is
// set to false, and we start adding features to the descendant invalidation
// set.
template <RuleInvalidationDataVisitorType VisitorType>
void RuleInvalidationDataVisitor<VisitorType>::AddFeaturesToInvalidationSet(
    InvalidationSetType* invalidation_set,
    const InvalidationSetFeatures& features) {
  if (features.invalidation_flags.TreeBoundaryCrossing()) {
    if constexpr (is_builder()) {
      invalidation_set->SetTreeBoundaryCrossing();
    }
  }
  if (features.invalidation_flags.InsertionPointCrossing()) {
    if constexpr (is_builder()) {
      invalidation_set->SetInsertionPointCrossing();
    }
  }
  if (features.invalidation_flags.InvalidatesSlotted()) {
    if constexpr (is_builder()) {
      invalidation_set->SetInvalidatesSlotted();
    }
  }
  if (features.invalidation_flags.WholeSubtreeInvalid()) {
    SetWholeSubtreeInvalid(invalidation_set);
  }
  if (features.invalidation_flags.InvalidatesParts()) {
    if constexpr (is_builder()) {
      invalidation_set->SetInvalidatesParts();
    }
  }
  if (features.content_pseudo_crossing ||
      features.invalidation_flags.WholeSubtreeInvalid()) {
    return;
  }

  for (const auto& id : features.ids) {
    if constexpr (is_builder()) {
      invalidation_set->AddId(id);
    }
    InvalidationSetToSelectorMap::RecordInvalidationSetEntry(
        invalidation_set,
        InvalidationSetToSelectorMap::SelectorFeatureType::kId, id);
  }
  for (const auto& tag_name : features.tag_names) {
    if constexpr (is_builder()) {
      invalidation_set->AddTagName(tag_name);
    }
    InvalidationSetToSelectorMap::RecordInvalidationSetEntry(
        invalidation_set,
        InvalidationSetToSelectorMap::SelectorFeatureType::kTagName, tag_name);
  }
  for (const auto& emitted_tag_name : features.emitted_tag_names) {
    if constexpr (is_builder()) {
      invalidation_set->AddTagName(emitted_tag_name);
    }
    InvalidationSetToSelectorMap::RecordInvalidationSetEntry(
        invalidation_set,
        InvalidationSetToSelectorMap::SelectorFeatureType::kTagName,
        emitted_tag_name);
  }
  for (const auto& class_name : features.classes) {
    if constexpr (is_builder()) {
      invalidation_set->AddClass(class_name);
    }
    InvalidationSetToSelectorMap::RecordInvalidationSetEntry(
        invalidation_set,
        InvalidationSetToSelectorMap::SelectorFeatureType::kClass, class_name);
  }
  for (const auto& attribute : features.attributes) {
    if constexpr (is_builder()) {
      invalidation_set->AddAttribute(attribute);
    }
    InvalidationSetToSelectorMap::RecordInvalidationSetEntry(
        invalidation_set,
        InvalidationSetToSelectorMap::SelectorFeatureType::kAttribute,
        attribute);
  }
  if (features.invalidation_flags.InvalidateCustomPseudo()) {
    if constexpr (is_builder()) {
      invalidation_set->SetCustomPseudoInvalid();
    }
  }
}

template <RuleInvalidationDataVisitorType VisitorType>
void RuleInvalidationDataVisitor<VisitorType>::SetWholeSubtreeInvalid(
    InvalidationSetType* invalidation_set) {
  if constexpr (is_builder()) {
    invalidation_set->SetWholeSubtreeInvalid();
  }
  InvalidationSetToSelectorMap::RecordInvalidationSetEntry(
      invalidation_set,
      InvalidationSetToSelectorMap::SelectorFeatureType::kWholeSubtree,
      g_empty_atom);
}

template <RuleInvalidationDataVisitorType VisitorType>
void RuleInvalidationDataVisitor<VisitorType>::SetInvalidatesSelf(
    InvalidationSetType* invalidation_set) {
  if constexpr (is_builder()) {
    invalidation_set->SetInvalidatesSelf();
  }
}

template <RuleInvalidationDataVisitorType VisitorType>
void RuleInvalidationDataVisitor<VisitorType>::SetInvalidatesNth(
    InvalidationSetType* invalidation_set) {
  if constexpr (is_builder()) {
    invalidation_set->SetInvalidatesNth();
  }
}

template <RuleInvalidationDataVisitorType VisitorType>
void RuleInvalidationDataVisitor<VisitorType>::UpdateMaxDirectAdjacentSelectors(
    SiblingInvalidationSetType* invalidation_set,
    unsigned value) {
  if constexpr (is_builder()) {
    invalidation_set->UpdateMaxDirectAdjacentSelectors(value);
  }
}

template <RuleInvalidationDataVisitorType VisitorType>
bool RuleInvalidationDataVisitor<VisitorType>::
    InsertIntoSelfInvalidationBloomFilter(const AtomicString& value, int salt) {
  if constexpr (is_builder()) {
    if (rule_invalidation_data_.names_with_self_invalidation == nullptr) {
      if (rule_invalidation_data_.num_candidates_for_names_bloom_filter++ <
          50) {
        // It's not worth spending 2 kB on the Bloom filter for this
        // style sheet yet, so just insert a regular entry.
        return false;
      } else {
        rule_invalidation_data_.names_with_self_invalidation =
            std::make_unique<WTF::BloomFilter<14>>();
      }
    }
    rule_invalidation_data_.names_with_self_invalidation->Add(value.Hash() *
                                                              salt);
    return true;
  } else {
    // In the non-builder case, assume we did not add to the Bloom filter and
    // fall back to looking in the invalidation sets.
    return false;
  }
}

template <RuleInvalidationDataVisitorType VisitorType>
RuleInvalidationDataVisitor<VisitorType>::InvalidationSetType*
RuleInvalidationDataVisitor<VisitorType>::
    EnsureSiblingDescendantInvalidationSet(
        SiblingInvalidationSetType* invalidation_set) {
  if constexpr (is_builder()) {
    return &invalidation_set->EnsureSiblingDescendants();
  } else {
    return invalidation_set->SiblingDescendants();
  }
}

template <RuleInvalidationDataVisitorType VisitorType>
InvalidationSet&
RuleInvalidationDataVisitor<VisitorType>::EnsureMutableInvalidationSet(
    InvalidationType type,
    PositionType position,
    bool in_nth_child,
    scoped_refptr<InvalidationSet>& invalidation_set) {
  if (!invalidation_set) {
    // Create a new invalidation set of the right type.
    if (type == InvalidationType::kInvalidateDescendants) {
      if (position == kSubject && !in_nth_child) {
        invalidation_set = InvalidationSet::SelfInvalidationSet();
      } else {
        invalidation_set = DescendantInvalidationSet::Create();
      }
    } else {
      invalidation_set = SiblingInvalidationSet::Create(nullptr);
    }
    return *invalidation_set;
  }

  if (invalidation_set->IsSelfInvalidationSet() &&
      type == InvalidationType::kInvalidateDescendants &&
      position == kSubject && !in_nth_child) {
    // NOTE: This is fairly dodgy; we're returning the singleton
    // self-invalidation set (which is very much immutable) from a
    // function promising to return something mutable. We pretty much
    // rely on the caller to do the right thing and not mutate the
    // self-invalidation set if asking for it (ie., giving this
    // combination of type/position).
    return *invalidation_set;
  }

  // If the currently stored invalidation_set is shared with other
  // RuleInvalidationData instances, or it is the SelfInvalidationSet()
  // singleton, we must copy it before modifying it.
  //
  // If we are retrieving the invalidation set for a simple selector in a non-
  // rightmost compound, it means we plan to add features to the set. If so,
  // create a DescendantInvalidationSet we are allowed to modify.
  //
  // Note that we also construct a DescendantInvalidationSet instead of using
  // the SelfInvalidationSet() when we create a SiblingInvalidationSet. We may
  // be able to let SiblingInvalidationSets reference the singleton set for
  // descendants as well. TODO(futhark@chromium.org)
  if (invalidation_set->IsSelfInvalidationSet() ||
      !invalidation_set->HasOneRef()) {
    invalidation_set = CopyInvalidationSet(*invalidation_set);
    DCHECK(invalidation_set->HasOneRef());
  }

  if (invalidation_set->GetType() == type) {
    return *invalidation_set;
  }

  if (type == InvalidationType::kInvalidateDescendants) {
    // sibling → sibling+descendant.
    DescendantInvalidationSet& embedded_invalidation_set =
        To<SiblingInvalidationSet>(*invalidation_set).EnsureDescendants();
    return embedded_invalidation_set;
  } else {
    // descendant → sibling+descendant.
    scoped_refptr<InvalidationSet> descendants = invalidation_set;
    invalidation_set = SiblingInvalidationSet::Create(
        To<DescendantInvalidationSet>(descendants.get()));
    return *invalidation_set;
  }
}

template class RuleInvalidationDataVisitor<
    RuleInvalidationDataVisitorType::kBuilder>;
template class RuleInvalidationDataVisitor<
    RuleInvalidationDataVisitorType::kTracer>;

}  // namespace blink
