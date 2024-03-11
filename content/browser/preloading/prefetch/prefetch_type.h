// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_TYPE_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_TYPE_H_

#include "content/common/content_export.h"
#include "content/public/browser/preloading_trigger_type.h"
#include "third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom.h"

namespace content {

// The type of prefetch. This determines various details about how a prefetch is
// handled.
class CONTENT_EXPORT PrefetchType {
 public:
  // Construct a PrefetchType for non-SpeculationRules triggers.
  PrefetchType(PreloadingTriggerType non_specrules_trigger_type,
               bool use_prefetch_proxy);

  // Construct a PrefetchType for SpeculationRules triggers.
  PrefetchType(PreloadingTriggerType trigger_type,
               bool use_prefetch_proxy,
               blink::mojom::SpeculationEagerness eagerness);

  ~PrefetchType() = default;

  PrefetchType(const PrefetchType& prefetch_type) = default;
  PrefetchType& operator=(const PrefetchType& prefetch_type) = delete;

  bool operator==(const PrefetchType& rhs) const = default;
  bool operator!=(const PrefetchType& rhs) const = default;

  PreloadingTriggerType trigger_type() const { return trigger_type_; }

  // Whether this prefetch should bypass the proxy even though it would need to
  // be proxied for anonymity. For use in test automation only.
  bool IsProxyBypassedForTesting() const { return proxy_bypassed_for_testing_; }

  void SetProxyBypassedForTest();

  // Whether cross-origin prefetches of this type need to use the Prefetch
  // Proxy.
  bool IsProxyRequiredWhenCrossOrigin() const { return use_prefetch_proxy_; }

  // Returns the eagerness of the prefetch based on the speculation rules API.
  blink::mojom::SpeculationEagerness GetEagerness() const;

  // Whether this prefetch is initiated by renderer processes.
  // Currently this is equivalent to whether the trigger type is Speculation
  // Rules or not.
  bool IsRendererInitiated() const;

 private:
  const PreloadingTriggerType trigger_type_;
  const bool use_prefetch_proxy_;
  bool proxy_bypassed_for_testing_ = false;
  const std::optional<blink::mojom::SpeculationEagerness> eagerness_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_TYPE_H_
