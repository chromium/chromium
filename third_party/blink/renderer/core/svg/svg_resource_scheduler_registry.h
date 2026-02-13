// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_RESOURCE_SCHEDULER_REGISTRY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_RESOURCE_SCHEDULER_REGISTRY_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/svg/svg_document_resource_tracker.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/scheduler/public/agent_group_scheduler.h"

namespace blink {

// SVGResourceSchedulerRegistry is responsible for managing the mapping between
// AgentGroupScheduler instances and their corresponding
// SVGDocumentResourceTracker instances. This ties the SVG resource lifecycle
// with the AgentGroupScheduler.
class CORE_EXPORT SVGResourceSchedulerRegistry final {
 public:
  SVGResourceSchedulerRegistry() = default;
  ~SVGResourceSchedulerRegistry() = default;

  SVGResourceSchedulerRegistry(const SVGResourceSchedulerRegistry&) = delete;
  SVGResourceSchedulerRegistry& operator=(const SVGResourceSchedulerRegistry&) =
      delete;

  // Returns the tracker for the given scheduler, creating one if the tracker
  // doesn't exist.
  static SVGDocumentResourceTracker* GetTracker(AgentGroupScheduler& scheduler);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_RESOURCE_SCHEDULER_REGISTRY_H_
