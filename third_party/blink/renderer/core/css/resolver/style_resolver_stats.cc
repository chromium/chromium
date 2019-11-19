/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/css/resolver/style_resolver_stats.h"

#include <memory>

namespace blink {

void StyleResolverStats::Reset() {
  matched_property_apply = 0;
  matched_property_cache_hit = 0;
  matched_property_cache_inherited_hit = 0;
  matched_property_cache_added = 0;
  rules_fast_rejected = 0;
  rules_rejected = 0;
  rules_matched = 0;
  styles_changed = 0;
  styles_unchanged = 0;
  styles_animated = 0;
  elements_styled = 0;
  pseudo_elements_styled = 0;
  base_styles_used = 0;
  independent_inherited_styles_propagated = 0;
  custom_properties_applied = 0;
}

std::unique_ptr<TracedValue> StyleResolverStats::ToTracedValue() const {
  auto traced_value = std::make_unique<TracedValue>();
  traced_value->SetInteger("matchedPropertyApply", matched_property_apply);
  traced_value->SetInteger("matchedPropertyCacheHit",
                           matched_property_cache_hit);
  traced_value->SetInteger("matchedPropertyCacheInheritedHit",
                           matched_property_cache_inherited_hit);
  traced_value->SetInteger("matchedPropertyCacheAdded",
                           matched_property_cache_added);
  traced_value->SetInteger("rulesRejected", rules_rejected);
  traced_value->SetInteger("rulesFastRejected", rules_fast_rejected);
  traced_value->SetInteger("rulesMatched", rules_matched);
  traced_value->SetInteger("stylesChanged", styles_changed);
  traced_value->SetInteger("stylesUnchanged", styles_unchanged);
  traced_value->SetInteger("stylesAnimated", styles_animated);
  traced_value->SetInteger("elementsStyled", elements_styled);
  traced_value->SetInteger("pseudoElementsStyled", pseudo_elements_styled);
  traced_value->SetInteger("baseStylesUsed", base_styles_used);
  traced_value->SetInteger("independentInheritedStylesPropagated",
                           independent_inherited_styles_propagated);
  traced_value->SetInteger("customPropertiesApplied",
                           custom_properties_applied);
  return traced_value;
}

}  // namespace blink
