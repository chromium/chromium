// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_MESH_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_MESH_MANAGER_H_

#include "base/types/pass_key.h"
#include "device/vr/public/mojom/mesh_id.h"
#include "device/vr/public/mojom/vr_service.mojom-blink-forward.h"
#include "third_party/blink/renderer/modules/xr/xr_id_hash_traits.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/vector3d_f.h"

namespace blink {

class XRMesh;
class XRMeshSet;
class XRSession;

class XRMeshManager final : public GarbageCollected<XRMeshManager> {
 public:
  XRMeshManager(base::PassKey<XRSession> pass_key, XRSession* session);

  void ProcessMeshInformation(
      const device::mojom::blink::XRMeshDetectionData* detected_meshes_data,
      double timestamp);

  XRMeshSet* GetDetectedMeshes() const;

  void Trace(Visitor* visitor) const;

 private:
  Member<XRSession> session_;

  // Map from mesh IDs to their corresponding XRMesh objects.
  HeapHashMap<device::MeshId, Member<XRMesh>> mesh_ids_to_meshes_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_MESH_MANAGER_H_
