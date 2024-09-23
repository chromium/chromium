/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/css/rule_feature_set.h"

#include <algorithm>
#include <bitset>

#include "base/auto_reset.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/css/css_selector.h"
#include "third_party/blink/renderer/core/css/css_selector_list.h"
#include "third_party/blink/renderer/core/css/invalidation/invalidation_set.h"
#include "third_party/blink/renderer/core/css/invalidation/rule_invalidation_data_builder.h"
#include "third_party/blink/renderer/core/css/invalidation/rule_invalidation_data_tracer.h"
#include "third_party/blink/renderer/core/css/style_scope.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

bool RuleFeatureSet::operator==(const RuleFeatureSet& other) const {
  return rule_invalidation_data_ == other.rule_invalidation_data_ &&
         media_query_result_flags_ == other.media_query_result_flags_;
}

SelectorPreMatch RuleFeatureSet::CollectFeaturesFromSelector(
    const CSSSelector& selector,
    const StyleScope* style_scope) {
  RuleInvalidationDataBuilder builder(rule_invalidation_data_);
  return builder.CollectFeaturesFromSelector(selector, style_scope);
}

void RuleFeatureSet::RevisitSelectorForInspector(
    const CSSSelector& selector) const {
  RuleInvalidationDataTracer tracer(rule_invalidation_data_);
  tracer.TraceInvalidationSetsForSelector(selector);
}

void RuleFeatureSet::Merge(const RuleFeatureSet& other) {
  CHECK_NE(&other, this);
  RuleInvalidationDataBuilder builder(rule_invalidation_data_);
  builder.Merge(other.rule_invalidation_data_);
  media_query_result_flags_.Add(other.media_query_result_flags_);
}

void RuleFeatureSet::Clear() {
  rule_invalidation_data_.Clear();
  media_query_result_flags_.Clear();
}

bool RuleFeatureSet::HasViewportDependentMediaQueries() const {
  return media_query_result_flags_.is_viewport_dependent;
}

bool RuleFeatureSet::HasDynamicViewportDependentMediaQueries() const {
  return media_query_result_flags_.unit_flags &
         MediaQueryExpValue::UnitFlags::kDynamicViewport;
}

const RuleInvalidationData& RuleFeatureSet::GetRuleInvalidationData() const {
  return rule_invalidation_data_;
}

String RuleFeatureSet::ToString() const {
  return rule_invalidation_data_.ToString();
}

std::ostream& operator<<(std::ostream& ostream, const RuleFeatureSet& set) {
  return ostream << set.ToString().Utf8();
}

}  // namespace blink
