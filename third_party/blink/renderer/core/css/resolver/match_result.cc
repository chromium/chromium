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

#include "base/numerics/clamped_math.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

void MatchedProperties::Trace(Visitor* visitor) const {
  visitor->Trace(properties);
}

void MatchResult::AddMatchedProperties(const CSSPropertyValueSet* properties,
                                       const MatchedProperties::Data& types) {
  MatchedProperties::Data new_types = types;
  new_types.tree_order = current_tree_order_;
  matched_properties_.emplace_back(const_cast<CSSPropertyValueSet*>(properties),
                                   new_types);

#if DCHECK_IS_ON()
  DCHECK_NE(types.origin, CascadeOrigin::kNone);
  DCHECK_GE(types.origin, last_origin_);
  if (!tree_scopes_.empty()) {
    DCHECK_EQ(types.origin, CascadeOrigin::kAuthor);
  }
  last_origin_ = types.origin;
#endif
}

void MatchResult::BeginAddingAuthorRulesForTreeScope(
    const TreeScope& tree_scope) {
  current_tree_order_ =
      ClampTo<decltype(current_tree_order_)>(tree_scopes_.size());
  tree_scopes_.push_back(&tree_scope);
}

void MatchResult::Reset() {
  matched_properties_.clear();
  is_cacheable_ = true;
  depends_on_size_container_queries_ = false;
#if DCHECK_IS_ON()
  last_origin_ = CascadeOrigin::kNone;
#endif
  current_tree_order_ = 0;
  tree_scopes_.clear();
}

}  // namespace blink
