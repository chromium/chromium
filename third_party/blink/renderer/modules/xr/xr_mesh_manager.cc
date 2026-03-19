// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_mesh_manager.h"

#include "base/trace_event/trace_event.h"
#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "third_party/blink/renderer/modules/xr/xr_mesh.h"
#include "third_party/blink/renderer/modules/xr/xr_mesh_set.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/quaternion.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/decomposed_transform.h"

namespace blink {
XRMeshManager::XRMeshManager(base::PassKey<XRSession> pass_key,
                             XRSession* session)
    : session_(session) {}

void XRMeshManager::ProcessMeshInformation(
    const device::mojom::blink::XRMeshDetectionData* detected_meshes_data,
    double timestamp) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("xr.debug"), "ProcessMeshInformation");

  if (!detected_meshes_data) {
    DVLOG(3) << __func__ << ": detected_meshes_data is null";
    mesh_ids_to_meshes_.clear();
    return;
  }

  // Process UPDATED or NEW meshes.
  for (const auto& mesh_data : detected_meshes_data->updated_meshes_data) {
    auto mesh_id = mesh_data->id;
    auto result = mesh_ids_to_meshes_.insert(mesh_id, nullptr);
    if (result.is_new_entry) {
      result.stored_value->value = MakeGarbageCollected<XRMesh>(
          mesh_id, session_, *mesh_data, timestamp);
    } else {
      result.stored_value->value->Update(*mesh_data, timestamp);
    }
  }

  // Build set of all current mesh IDs
  HashSet<device::MeshId> current_mesh_ids;
  for (const auto& mesh_id : detected_meshes_data->all_meshes_ids) {
    current_mesh_ids.insert(mesh_id);
  }
  // Remove stale meshes
  mesh_ids_to_meshes_.erase_if([&current_mesh_ids](const auto& pair) {
    return !current_mesh_ids.Contains(pair.key);
  });
}

XRMeshSet* XRMeshManager::GetDetectedMeshes() const {
  if (!session_->IsFeatureEnabled(
          device::mojom::XRSessionFeature::MESH_DETECTION)) {
    return MakeGarbageCollected<XRMeshSet>(HeapHashSet<Member<XRMesh>>{});
  }

  HeapHashSet<Member<XRMesh>> visible_meshes;

  for (auto& id_and_mesh : mesh_ids_to_meshes_) {
    XRMesh* mesh = id_and_mesh.value;
    visible_meshes.insert(mesh);
  }

  return MakeGarbageCollected<XRMeshSet>(visible_meshes);
}

void XRMeshManager::Trace(Visitor* visitor) const {
  visitor->Trace(session_);
  visitor->Trace(mesh_ids_to_meshes_);
}

}  // namespace blink
