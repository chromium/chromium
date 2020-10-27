// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/resolver/cascade_expansion.h"

#include "third_party/blink/renderer/core/css/resolver/match_result.h"
#include "third_party/blink/renderer/core/css/rule_set.h"

namespace blink {

namespace {

CascadeFilter AddValidPropertiesFilter(
    CascadeFilter filter,
    const MatchedProperties& matched_properties) {
  switch (static_cast<ValidPropertyFilter>(
      matched_properties.types_.valid_property_filter)) {
    case ValidPropertyFilter::kNoFilter:
      return filter;
    case ValidPropertyFilter::kCue:
      return filter.Add(CSSProperty::kValidForCue, false);
    case ValidPropertyFilter::kFirstLetter:
      return filter.Add(CSSProperty::kValidForFirstLetter, false);
    case ValidPropertyFilter::kMarker:
      return filter.Add(CSSProperty::kValidForMarker, false);
    case ValidPropertyFilter::kHighlight:
      return filter.Add(CSSProperty::kValidForHighlight, false);
  }
}

CascadeFilter AddLinkFilter(CascadeFilter filter,
                            const MatchedProperties& matched_properties) {
  switch (matched_properties.types_.link_match_type) {
    case CSSSelector::kMatchVisited:
      return filter.Add(CSSProperty::kVisited, false);
    case CSSSelector::kMatchLink:
      return filter.Add(CSSProperty::kVisited, true);
    case CSSSelector::kMatchAll:
      return filter;
    default:
      return filter.Add(CSSProperty::kProperty, true);
  }
}

CascadeFilter AmendFilter(CascadeFilter filter,
                          const MatchedProperties& matched_properties) {
  return AddLinkFilter(AddValidPropertiesFilter(filter, matched_properties),
                       matched_properties);
}

}  // anonymous namespace

CascadeExpansion::CascadeExpansion(const MatchedProperties& matched_properties,
                                   const Document& document,
                                   CascadeFilter filter,
                                   size_t matched_properties_index)
    : document_(document),
      matched_properties_(matched_properties),
      size_(matched_properties.properties->PropertyCount()),
      filter_(AmendFilter(filter, matched_properties)),
      matched_properties_index_(matched_properties_index) {
  // We can't handle a MatchResult with more than 0xFFFF MatchedProperties,
  // or a MatchedProperties object with more than 0xFFFF declarations. If this
  // happens, we skip right to the end, and emit nothing.
  if (size_ > kMaxDeclarationIndex + 1 ||
      matched_properties_index_ > kMaxMatchedPropertiesIndex) {
    index_ = size_;
  } else {
    Next();
  }
}

CascadeExpansion::CascadeExpansion(const CascadeExpansion& o)
    : document_(o.document_),
      state_(o.state_),
      matched_properties_(o.matched_properties_),
      priority_(o.priority_),
      index_(o.index_),
      size_(o.size_),
      filter_(o.filter_),
      matched_properties_index_(o.matched_properties_index_),
      id_(o.id_),
      property_(id_ == CSSPropertyID::kVariable ? &custom_ : o.property_),
      custom_(o.custom_) {}

void CascadeExpansion::Next() {
  do {
    switch (state_) {
      case State::kInit:
        AdvanceNormal();
        break;
      case State::kNormal:
        if (ShouldEmitVisited() && AdvanceVisited())
          break;
        AdvanceNormal();
        break;
      case State::kVisited:
        AdvanceNormal();
        break;
      case State::kAll:
        AdvanceAll();
        break;
    }
  } while (!AtEnd() && filter_.Rejects(*property_));
}

bool CascadeExpansion::IsAffectedByAll(CSSPropertyID id) {
  const CSSProperty& property = CSSProperty::Get(id);
  return !property.IsShorthand() && property.IsAffectedByAll();
}

bool CascadeExpansion::ShouldEmitVisited() const {
  // This check is slightly redundant, as the emitted property would anyway
  // be skipped by the do-while in Next(). However, it's probably good to avoid
  // entering State::kVisited at all, if we can avoid it.
  return !filter_.Rejects(CSSProperty::kVisited, true);
}

void CascadeExpansion::AdvanceNormal() {
  state_ = State::kNormal;
  ++index_;
  if (AtEnd())
    return;
  auto reference = PropertyAt(index_);
  const auto& metadata = reference.PropertyMetadata();
  id_ = metadata.PropertyID();
  priority_ = CascadePriority(
      matched_properties_.types_.origin, metadata.important_,
      matched_properties_.types_.tree_order,
      EncodeMatchResultPosition(matched_properties_index_, index_));

  switch (id_) {
    case CSSPropertyID::kVariable:
      custom_ = CustomProperty(reference.Name().ToAtomicString(), document_);
      property_ = &custom_;
      break;
    case CSSPropertyID::kAll:
      state_ = State::kAll;
      id_ = firstCSSProperty;
      property_ = &CSSProperty::Get(id_);
      // If this DCHECK is triggered, it means firstCSSProperty is not affected
      // by 'all', and we need a function for figuring out the first property
      // that _is_ affected by 'all'.
      DCHECK(IsAffectedByAll(id_));
      break;
    default:
      property_ = &CSSProperty::Get(id_);
      break;
  }

  DCHECK(property_);
}

bool CascadeExpansion::AdvanceVisited() {
  DCHECK(ShouldEmitVisited());
  DCHECK(property_);
  const CSSProperty* visited = property_->GetVisitedProperty();
  if (!visited)
    return false;
  property_ = visited;
  id_ = visited->PropertyID();
  state_ = State::kVisited;
  return true;
}

void CascadeExpansion::AdvanceAll() {
  state_ = State::kAll;

  int i = static_cast<int>(id_) + 1;
  int end = kIntLastCSSProperty + 1;

  for (; i < end; ++i) {
    id_ = convertToCSSPropertyID(i);
    if (IsAffectedByAll(id_))
      break;
  }

  if (i >= end)
    AdvanceNormal();
  else
    property_ = &CSSProperty::Get(id_);
}

CSSPropertyValueSet::PropertyReference CascadeExpansion::PropertyAt(
    size_t index) const {
  DCHECK(!AtEnd());
  return matched_properties_.properties->PropertyAt(index_);
}

uint16_t CascadeExpansion::TreeOrder() const {
  return matched_properties_.types_.tree_order;
}

}  // namespace blink
