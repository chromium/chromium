// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_type.h"

#include <tuple>

#include "base/check.h"
#include "third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom.h"

namespace content {

PrefetchType::PrefetchType(bool use_isolated_network_context,
                           bool use_prefetch_proxy,
                           blink::mojom::SpeculationEagerness eagerness)
    : use_isolated_network_context_(use_isolated_network_context),
      use_prefetch_proxy_(use_prefetch_proxy),
      eagerness_(eagerness) {
  // Checks that the given dimensions are a supported prefetch type.
  DCHECK(!(!use_isolated_network_context && use_prefetch_proxy));
}

PrefetchType::~PrefetchType() = default;
PrefetchType::PrefetchType(const PrefetchType& prefetch_type) = default;
PrefetchType& PrefetchType::operator=(const PrefetchType& prefetch_type) =
    default;

void PrefetchType::SetProxyBypassedForTest() {
  DCHECK(use_prefetch_proxy_);
  proxy_bypassed_for_testing_ = true;
}

bool operator==(const PrefetchType& prefetch_type_1,
                const PrefetchType& prefetch_type_2) {
  return std::tie(prefetch_type_1.use_isolated_network_context_,
                  prefetch_type_1.use_prefetch_proxy_,
                  prefetch_type_1.eagerness_) ==
         std::tie(prefetch_type_2.use_isolated_network_context_,
                  prefetch_type_2.use_prefetch_proxy_,
                  prefetch_type_2.eagerness_);
}

bool operator!=(const PrefetchType& prefetch_type_1,
                const PrefetchType& prefetch_type_2) {
  return !(prefetch_type_1 == prefetch_type_2);
}

}  // namespace content
