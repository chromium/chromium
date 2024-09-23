// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_type.h"

#include <tuple>

#include "base/check.h"
#include "content/browser/preloading/prefetch/prefetch_params.h"
#include "content/browser/preloading/preloading_trigger_type_impl.h"
#include "content/public/browser/preloading_trigger_type.h"
#include "third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom.h"

namespace content {

PrefetchType::PrefetchType(PreloadingTriggerType non_speculation_trigger_type,
                           bool use_prefetch_proxy)
    : trigger_type_(non_speculation_trigger_type),
      use_prefetch_proxy_(use_prefetch_proxy) {
  CHECK(PrefetchBrowserInitiatedTriggersEnabled());
  CHECK(!IsSpeculationRuleType(non_speculation_trigger_type));
}

PrefetchType::PrefetchType(PreloadingTriggerType trigger_type,
                           bool use_prefetch_proxy,
                           blink::mojom::SpeculationEagerness eagerness)
    : trigger_type_(trigger_type),
      use_prefetch_proxy_(use_prefetch_proxy),
      eagerness_(eagerness) {
  CHECK(IsSpeculationRuleType(trigger_type));
}

void PrefetchType::SetProxyBypassedForTest() {
  DCHECK(use_prefetch_proxy_);
  proxy_bypassed_for_testing_ = true;
}

blink::mojom::SpeculationEagerness PrefetchType::GetEagerness() const {
  CHECK(IsSpeculationRuleType(trigger_type_));
  return eagerness_.value();
}

bool PrefetchType::IsRendererInitiated() const {
  return IsSpeculationRuleType(trigger_type_);
}

}  // namespace content
