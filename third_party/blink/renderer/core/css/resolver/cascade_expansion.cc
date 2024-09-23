// Copyright 2020 The Chromium Authors
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
      matched_properties.data_.valid_property_filter)) {
    case ValidPropertyFilter::kNoFilter:
      return filter;
    case ValidPropertyFilter::kCue:
      return filter.Add(CSSProperty::kValidForCue, false);
    case ValidPropertyFilter::kFirstLetter:
      return filter.Add(CSSProperty::kValidForFirstLetter, false);
    case ValidPropertyFilter::kFirstLine:
      return filter.Add(CSSProperty::kValidForFirstLine, false);
    case ValidPropertyFilter::kMarker:
      return filter.Add(CSSProperty::kValidForMarker, false);
    case ValidPropertyFilter::kHighlightLegacy:
      return filter.Add(CSSProperty::kValidForHighlightLegacy, false);
    case ValidPropertyFilter::kHighlight:
      return filter.Add(CSSProperty::kValidForHighlight, false);
    case ValidPropertyFilter::kPositionTry:
      return filter.Add(CSSProperty::kValidForPositionTry, false);
    case ValidPropertyFilter::kLimitedPageContext:
      return filter.Add(CSSProperty::kValidForLimitedPageContext, false);
    case ValidPropertyFilter::kPageContext:
      return filter.Add(CSSProperty::kValidForPageContext, false);
  }
}

CascadeFilter AddLinkFilter(CascadeFilter filter,
                            const MatchedProperties& matched_properties) {
  switch (matched_properties.data_.link_match_type) {
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

}  // anonymous namespace

CORE_EXPORT CascadeFilter
CreateExpansionFilter(const MatchedProperties& matched_properties) {
  return AddLinkFilter(
      AddValidPropertiesFilter(CascadeFilter(), matched_properties),
      matched_properties);
}

CORE_EXPORT bool IsInAllExpansion(CSSPropertyID id) {
  const CSSProperty& property = CSSProperty::Get(id);
  // Only web-exposed properties are affected by 'all' (IsAffectedByAll).
  // This excludes -internal-visited properties from being affected, but for
  // the purposes of cascade expansion, they need to be included, otherwise
  // rules like :visited { all:unset; } will not work.
  const CSSProperty* unvisited = property.GetUnvisitedProperty();
  return !property.IsShorthand() &&
         (property.IsAffectedByAll() ||
          (unvisited && unvisited->IsAffectedByAll()));
}

}  // namespace blink
