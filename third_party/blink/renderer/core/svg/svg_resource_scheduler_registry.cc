// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/svg/svg_resource_scheduler_registry.h"

#include "base/check.h"
#include "base/uuid.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

namespace {

using MapType = HeapHashMap<UntracedMember<AgentGroupScheduler>,
                            Member<SVGDocumentResourceTracker>>;

String GenerateCacheIdentifier() {
  base::Uuid guid = base::Uuid::GenerateRandomV4();
  return String(guid.AsLowercaseString());
}

class RegistryHolder : public GarbageCollected<RegistryHolder> {
 public:
  RegistryHolder() = default;

  MapType& GetMap() { return map_; }

  void Trace(Visitor* visitor) const {
    visitor->RegisterWeakCallbackMethod<RegistryHolder,
                                        &RegistryHolder::ProcessCustomWeakness>(
        this);
    visitor->Trace(map_);
  }

  void ProcessCustomWeakness(const LivenessBroker& info) {
    Vector<UntracedMember<AgentGroupScheduler>> dead_schedulers;
    for (const auto& pair : map_) {
      if (!info.IsHeapObjectAlive(pair.key)) {
        dead_schedulers.push_back(pair.key);
        DCHECK(pair.value);
        pair.value->Dispose();
      }
    }
    map_.RemoveAll(dead_schedulers);
  }

 private:
  MapType map_;
};

MapType& GetTrackerMap() {
  DCHECK(IsMainThread());
  DEFINE_STATIC_LOCAL(Persistent<RegistryHolder>, holder,
                      (MakeGarbageCollected<RegistryHolder>()));
  return holder->GetMap();
}

}  // namespace

SVGDocumentResourceTracker* SVGResourceSchedulerRegistry::GetTracker(
    AgentGroupScheduler& scheduler) {
  MapType& map = GetTrackerMap();

  auto it = map.find(&scheduler);
  if (it != map.end()) {
    return it->value;
  }

  auto* tracker = MakeGarbageCollected<SVGDocumentResourceTracker>(
      scheduler.DefaultTaskRunner(), GenerateCacheIdentifier());
  map.Set(&scheduler, tracker);
  return tracker;
}

}  // namespace blink
