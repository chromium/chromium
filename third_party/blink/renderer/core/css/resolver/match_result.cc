/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013 Apple Inc.
 * All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/css/resolver/match_result.h"

#include <memory>
#include <type_traits>

#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

MatchedProperties::MatchedProperties() {
  memset(&types_, 0, sizeof(types_));
}

MatchedProperties::~MatchedProperties() = default;

void MatchedProperties::Trace(blink::Visitor* visitor) {
  visitor->Trace(properties);
}

void MatchResult::AddMatchedProperties(
    const CSSPropertyValueSet* properties,
    unsigned link_match_type,
    ValidPropertyFilter valid_property_filter) {
  matched_properties_.Grow(matched_properties_.size() + 1);
  MatchedProperties& new_properties = matched_properties_.back();
  new_properties.properties = const_cast<CSSPropertyValueSet*>(properties);
  new_properties.types_.link_match_type = link_match_type;
  new_properties.types_.valid_property_filter =
      static_cast<std::underlying_type_t<ValidPropertyFilter>>(
          valid_property_filter);
  // TODO(andruud): MatchedProperties are stored here in reverse order.
  // Reevaluate this when cascade has shipped.
  new_properties.types_.tree_order =
      std::numeric_limits<uint16_t>::max() - current_tree_order_;
}

void MatchResult::FinishAddingUARules() {
  ua_range_end_ = matched_properties_.size();
}

void MatchResult::FinishAddingUserRules() {
  // Don't add empty ranges.
  if (user_range_ends_.IsEmpty() &&
      ua_range_end_ == matched_properties_.size())
    return;
  if (!user_range_ends_.IsEmpty() &&
      user_range_ends_.back() == matched_properties_.size())
    return;
  user_range_ends_.push_back(matched_properties_.size());
  current_tree_order_ = clampTo<uint16_t>(user_range_ends_.size());
}

void MatchResult::FinishAddingAuthorRulesForTreeScope() {
  // Don't add empty ranges.
  if (author_range_ends_.IsEmpty() && user_range_ends_.IsEmpty() &&
      ua_range_end_ == matched_properties_.size())
    return;
  if (author_range_ends_.IsEmpty() && !user_range_ends_.IsEmpty() &&
      user_range_ends_.back() == matched_properties_.size())
    return;
  if (!author_range_ends_.IsEmpty() &&
      author_range_ends_.back() == matched_properties_.size())
    return;
  author_range_ends_.push_back(matched_properties_.size());
  current_tree_order_ = clampTo<uint16_t>(author_range_ends_.size());
}

}  // namespace blink
