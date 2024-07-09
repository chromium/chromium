// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_plane_manager.h"

#include "base/containers/contains.h"
#include "base/not_fatal_until.h"
#include "base/trace_event/trace_event.h"
#include "third_party/blink/renderer/modules/xr/xr_plane.h"
#include "third_party/blink/renderer/modules/xr/xr_plane_set.h"

namespace blink {

XRPlaneManager::XRPlaneManager(base::PassKey<XRSession> pass_key,
                               XRSession* session)
    : session_(session) {}

void XRPlaneManager::ProcessPlaneInformation(
    const device::mojom::blink::XRPlaneDetectionData* detected_planes_data,
    double timestamp) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("xr.debug"), __func__);

  if (!detected_planes_data) {
    DVLOG(3) << __func__ << ": detected_planes_data is null";

    // We have received a nullopt - plane detection is not supported or
    // disabled. Clear stored planes (if any).
    // The device can send either null or empty data - in both cases, it means
    // that there are no planes available.
    plane_ids_to_planes_.clear();
    return;
  }

  TRACE_COUNTER2("xr", "Plane statistics", "All planes",
                 detected_planes_data->all_planes_ids.size(), "Updated planes",
                 detected_planes_data->updated_planes_data.size());

  DVLOG(3) << __func__ << ": updated planes size="
           << detected_planes_data->updated_planes_data.size()
           << ", all planes size="
           << detected_planes_data->all_planes_ids.size();

  HeapHashMap<uint64_t, Member<XRPlane>> updated_planes;

  // First, process all planes that had their information updated (new planes
  // are also processed here).
  for (const auto& plane : detected_planes_data->updated_planes_data) {
    DCHECK(plane);

    auto it = plane_ids_to_planes_.find(plane->id);
    if (it != plane_ids_to_planes_.end()) {
      updated_planes.insert(plane->id, it->value);
      it->value->Update(*plane, timestamp);
    } else {
      updated_planes.insert(
          plane->id, MakeGarbageCollected<XRPlane>(plane->id, session_, *plane,
                                                   timestamp));
    }
  }

  // Then, copy over the planes that were not updated but are still present.
  for (const auto& plane_id : detected_planes_data->all_planes_ids) {
    // If the plane was already updated, there is nothing to do as it was
    // already moved to |updated_planes|. If it's not updated, just copy it over
    // as-is.
    if (!base::Contains(updated_planes, plane_id)) {
      auto it = plane_ids_to_planes_.find(plane_id);
      CHECK(it != plane_ids_to_planes_.end(), base::NotFatalUntil::M130);
      updated_planes.insert(plane_id, it->value);
    }
  }

  plane_ids_to_planes_.swap(updated_planes);
}

XRPlaneSet* XRPlaneManager::GetDetectedPlanes() const {
  if (!session_->IsFeatureEnabled(
          device::mojom::XRSessionFeature::PLANE_DETECTION)) {
    return MakeGarbageCollected<XRPlaneSet>(HeapHashSet<Member<XRPlane>>{});
  }

  HeapHashSet<Member<XRPlane>> result;
  for (auto& plane_id_and_plane : plane_ids_to_planes_) {
    result.insert(plane_id_and_plane.value);
  }

  return MakeGarbageCollected<XRPlaneSet>(result);
}

void XRPlaneManager::Trace(Visitor* visitor) const {
  visitor->Trace(session_);
  visitor->Trace(plane_ids_to_planes_);
}

}  // namespace blink
