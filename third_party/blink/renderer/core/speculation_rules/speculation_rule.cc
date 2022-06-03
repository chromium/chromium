// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/speculation_rules/speculation_rule.h"

namespace blink {

SpeculationRule::SpeculationRule(
    Vector<KURL> urls,
    RequiresAnonymousClientIPWhenCrossOrigin requires_anonymous_client_ip)
    : urls_(urls),
      requires_anonymous_client_ip_(requires_anonymous_client_ip) {}

SpeculationRule::~SpeculationRule() = default;

void SpeculationRule::Trace(Visitor*) const {}

}  // namespace blink
