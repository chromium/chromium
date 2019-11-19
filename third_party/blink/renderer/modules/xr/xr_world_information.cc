// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_world_information.h"

#include "base/trace_event/trace_event.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"

namespace blink {

XRWorldInformation::XRWorldInformation(XRSession* session)
    : session_(session) {}

void XRWorldInformation::Trace(blink::Visitor* visitor) {
  visitor->Trace(plane_ids_to_planes_);
  visitor->Trace(session_);
  ScriptWrappable::Trace(visitor);
}

XRPlaneSet* XRWorldInformation::detectedPlanes() const {
  DVLOG(3) << __func__;

  HeapHashSet<Member<XRPlane>> result;

  if (is_detected_planes_null_)
    return nullptr;

  for (auto& plane_id_and_plane : plane_ids_to_planes_) {
    result.insert(plane_id_and_plane.value);
  }

  return MakeGarbageCollected<XRPlaneSet>(result);
}

void XRWorldInformation::ProcessPlaneInformation(
    const device::mojom::blink::XRPlaneDetectionDataPtr& detected_planes_data,
    double timestamp) {
  TRACE_EVENT0("xr", __FUNCTION__);

  if (!detected_planes_data) {
    DVLOG(3) << __func__ << ": detected_planes_data is null";

    // We have received a nullopt - plane detection is not supported or
    // disabled. Mark detected_planes as null & clear stored planes.
    is_detected_planes_null_ = true;
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

  is_detected_planes_null_ = false;

  HeapHashMap<uint64_t, Member<XRPlane>> updated_planes;

  // First, process all planes that had their information updated (new planes
  // are also processed here).
  for (const auto& plane : detected_planes_data->updated_planes_data) {
    auto it = plane_ids_to_planes_.find(plane->id);
    if (it != plane_ids_to_planes_.end()) {
      updated_planes.insert(plane->id, it->value);
      it->value->Update(plane, timestamp);
    } else {
      updated_planes.insert(
          plane->id,
          MakeGarbageCollected<XRPlane>(plane->id, session_, plane, timestamp));
    }
  }

  // Then, copy over the planes that were not updated but are still present.
  for (const auto& plane_id : detected_planes_data->all_planes_ids) {
    auto it_updated = updated_planes.find(plane_id);

    // If the plane was already updated, there is nothing to do as it was
    // already moved to |updated_planes|. Otherwise just copy it over as-is.
    if (it_updated == updated_planes.end()) {
      auto it = plane_ids_to_planes_.find(plane_id);
      DCHECK(it != plane_ids_to_planes_.end());
      updated_planes.insert(plane_id, it->value);
    }
  }

  plane_ids_to_planes_.swap(updated_planes);
}

}  // namespace blink
