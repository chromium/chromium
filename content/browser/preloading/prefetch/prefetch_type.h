// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_TYPE_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_TYPE_H_

#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom.h"

namespace content {

// The type of prefetch. This determines various details about how a prefetch is
// handled.
class CONTENT_EXPORT PrefetchType {
 public:
  PrefetchType(bool use_isolated_network_context,
               bool use_prefetch_proxy,
               blink::mojom::SpeculationEagerness eagerness);
  ~PrefetchType();

  PrefetchType(const PrefetchType& prefetch_type);
  PrefetchType& operator=(const PrefetchType& prefetch_type);

  // Whether prefetches of this type need to use an isolated network context, or
  // use the default network context.
  bool IsIsolatedNetworkContextRequired() const {
    return use_isolated_network_context_;
  }

  // Whether this prefetch should bypass the proxy even though it would need to
  // be proxied for anonymity. For use in test automation only.
  bool IsProxyBypassedForTesting() const { return proxy_bypassed_for_testing_; }

  void SetProxyBypassedForTest();

  // Whether prefetches of this type need to use the Prefetch Proxy.
  bool IsProxyRequired() const { return use_prefetch_proxy_; }

  // Returns the eagerness of the prefetch based on the speculation rules API.
  blink::mojom::SpeculationEagerness GetEagerness() const { return eagerness_; }

 private:
  friend CONTENT_EXPORT bool operator==(const PrefetchType& prefetch_type_1,
                                        const PrefetchType& prefetch_type_2);

  bool use_isolated_network_context_;
  bool use_prefetch_proxy_;
  bool proxy_bypassed_for_testing_ = false;
  blink::mojom::SpeculationEagerness eagerness_;
};

CONTENT_EXPORT bool operator==(const PrefetchType& prefetch_type_1,
                               const PrefetchType& prefetch_type_2);
CONTENT_EXPORT bool operator!=(const PrefetchType& prefetch_type_1,
                               const PrefetchType& prefetch_type_2);

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_TYPE_H_
