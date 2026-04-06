// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_MESH_MANAGER_H_
#define DEVICE_VR_OPENXR_OPENXR_MESH_MANAGER_H_

#include "device/vr/public/mojom/vr_service.mojom.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

struct XrLocation;

// Interface for OpenXR Mesh Detection logic.
class OpenXrMeshManager {
 public:
  virtual ~OpenXrMeshManager() = default;
  virtual mojom::XRMeshDetectionDataPtr GetDetectedMeshesData(
      XrTime frame_time,
      XrSpace view_space) = 0;
  virtual std::optional<XrLocation> GetXrLocationFromMesh(
      MeshId mesh_id,
      const gfx::Transform& mesh_id_from_object) const = 0;

  // Called when a reference space change event is received (e.g. recenter),
  // so the manager can invalidate cached poses.
  virtual void OnReferenceSpaceChanged() = 0;
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_MESH_MANAGER_H_
