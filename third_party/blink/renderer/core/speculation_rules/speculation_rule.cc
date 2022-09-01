// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/speculation_rules/speculation_rule.h"

namespace blink {

SpeculationRule::SpeculationRule(
    Vector<KURL> urls,
    RequiresAnonymousClientIPWhenCrossOrigin requires_anonymous_client_ip,
    absl::optional<mojom::blink::SpeculationTargetHint> target_hint)
    : urls_(std::move(urls)),
      requires_anonymous_client_ip_(requires_anonymous_client_ip),
      target_browsing_context_name_hint_(target_hint) {}

SpeculationRule::~SpeculationRule() = default;

void SpeculationRule::Trace(Visitor*) const {}

}  // namespace blink
