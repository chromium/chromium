// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_ANDROID_OPENXR_MESH_MANAGER_ANDROID_H_
#define DEVICE_VR_OPENXR_ANDROID_OPENXR_MESH_MANAGER_ANDROID_H_

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <functional>
#include <vector>

#include "base/memory/raw_ref.h"
#include "base/uuid.h"
#include "third_party/openxr/dev/xr_android.h"
#include "device/vr/openxr/openxr_extension_helper.h"
#include "device/vr/openxr/openxr_mesh_manager.h"
#include "third_party/openxr/src/include/openxr/openxr.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace device {

class OpenXrExtensionHelper;

struct SubmeshCacheEntry {
  XrTime last_updated_time = 0;
  MeshId mesh_id;
};

class OpenXrMeshManagerAndroid : public OpenXrMeshManager {
 public:
  OpenXrMeshManagerAndroid(const OpenXrExtensionHelper& extension_helper,
                           XrSession session,
                           XrSpace mojo_space);
  ~OpenXrMeshManagerAndroid() override;

  mojom::XRMeshDetectionDataPtr GetDetectedMeshesData(
      XrTime frame_time) override;

  std::optional<XrLocation> GetXrLocationFromMesh(
      MeshId mesh_id,
      const gfx::Transform& mesh_id_from_object) const override;

  void OnReferenceSpaceChanged() override;

 private:
  struct PerSubmeshBuffers {
    PerSubmeshBuffers();
    ~PerSubmeshBuffers();

    std::vector<XrVector3f> positions;
    std::vector<XrVector3f> normals;
    std::vector<uint8_t> semantics;
    std::vector<uint32_t> indices;
  };

  // Helper function to map XrUuid to a local MeshId. Returns the existing ID
  // if the UUID was seen before, or creates a new sequential ID.
  MeshId GetOrCreateMeshId(const XrUuid& uuid);

  // Performs the two-call OpenXR pattern to fetch geometry for |submeshes|,
  // allocating per-submesh buffers and converting directly into Mojo structs.
  // Returns false on API failure.
  bool FetchAndConvertSubmeshes(
      XrSceneMeshSnapshotANDROID snapshot,
      std::vector<XrSceneSubmeshDataANDROID>& submeshes,
      const std::vector<XrPosef>& submesh_poses,
      std::vector<mojom::XRMeshDataPtr>& out_meshes);

  const raw_ref<const OpenXrExtensionHelper> extension_helper_;
  XrSession session_;
  XrSpace mojo_space_;
  XrSceneMeshingTrackerANDROID mesh_tracker_ = XR_NULL_HANDLE;

  // Local ID generation
  MeshId::Generator mesh_id_generator_;
  absl::flat_hash_map<std::string, MeshId> uuid_to_id_;
  absl::flat_hash_map<std::string, SubmeshCacheEntry> submesh_cache_;
  mojom::XRMeshDetectionDataPtr last_valid_mesh_data_;
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_ANDROID_OPENXR_MESH_MANAGER_ANDROID_H_
