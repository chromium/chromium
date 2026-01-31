// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_RESOURCE_TIMING_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_RESOURCE_TIMING_CONTEXT_H_

#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

// `ResourceTimingContext` is propagated by `TaskAttributionTaskState`. Its
// content is included in PerformanceResourceTiming.
class CORE_EXPORT ResourceTimingContext final
    : public GarbageCollected<ResourceTimingContext> {
 public:
  explicit ResourceTimingContext(KURL initiator_url)
      : initiator_url_(initiator_url) {}
  ~ResourceTimingContext() = default;

  const KURL& InitiatorUrl() const { return initiator_url_; }
  void Trace(Visitor*) const {}

 private:
  KURL initiator_url_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_RESOURCE_TIMING_CONTEXT_H_
