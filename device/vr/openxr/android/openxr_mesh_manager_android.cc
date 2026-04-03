// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/android/openxr_mesh_manager_android.h"

#include <limits>
#include <numeric>
#include <vector>

#include "base/containers/map_util.h"
#include "device/vr/openxr/openxr_api_wrapper.h"
#include "device/vr/openxr/openxr_extension_helper.h"
#include "device/vr/openxr/openxr_util.h"
#include "device/vr/public/mojom/xr_session.mojom.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/quaternion.h"
#include "ui/gfx/geometry/transform.h"
#include "base/strings/stringprintf.h"
#include "base/uuid.h"

namespace device {

namespace {

// Half-extents for the scene query bounding box. Using max/2 effectively
// queries the entire scene without any spatial restriction.
constexpr float kUnboundedSceneHalfExtent =
    std::numeric_limits<float>::max() / 2.0f;

std::string XrUuidToString(const XrUuid& uuid) {
  auto d = base::span(uuid.data);
  return base::StringPrintf(
      "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
      d[0],  d[1],  d[2],  d[3],
      d[4],  d[5],
      d[6],  d[7],
      d[8],  d[9],
      d[10], d[11], d[12], d[13], d[14], d[15]);
}

// RAII wrapper for XrSceneMeshSnapshotANDROID. We cannot use
// ScopedOpenXrObject because on 32-bit ARM the handle type collides with
// XrSpatialSnapshotEXT (both are uint64_t), making template specialization
// of Free() ambiguous.
class ScopedMeshSnapshot {
 public:
  ScopedMeshSnapshot(PFN_xrDestroySceneMeshSnapshotANDROID destroy_fn,
                     XrSceneMeshSnapshotANDROID handle)
      : destroy_fn_(destroy_fn), handle_(handle) {}
  ~ScopedMeshSnapshot() {
    if (handle_ != XR_NULL_HANDLE) {
      destroy_fn_(handle_);
    }
  }

  ScopedMeshSnapshot(const ScopedMeshSnapshot&) = delete;
  ScopedMeshSnapshot& operator=(const ScopedMeshSnapshot&) = delete;

  XrSceneMeshSnapshotANDROID get() const { return handle_; }

 private:
  PFN_xrDestroySceneMeshSnapshotANDROID destroy_fn_;
  XrSceneMeshSnapshotANDROID handle_;
};

}  // namespace

OpenXrMeshManagerAndroid::PerSubmeshBuffers::PerSubmeshBuffers() = default;
OpenXrMeshManagerAndroid::PerSubmeshBuffers::~PerSubmeshBuffers() = default;

OpenXrMeshManagerAndroid::OpenXrMeshManagerAndroid(
    const OpenXrExtensionHelper& extension_helper,
    XrSession session,
    XrSpace mojo_space)
    : extension_helper_(extension_helper),
      session_(session),
      mojo_space_(mojo_space) {
  XrSceneMeshingTrackerCreateInfoANDROID create_info{
      .type = XR_TYPE_SCENE_MESHING_TRACKER_CREATE_INFO_ANDROID,
      .semanticLabelSet = XR_SCENE_MESH_SEMANTIC_LABEL_SET_DEFAULT_ANDROID,
      .enableNormals = XR_TRUE,
  };
  extension_helper_->ExtensionMethods().xrCreateSceneMeshingTrackerANDROID(
      session_, &create_info, &mesh_tracker_);
}

OpenXrMeshManagerAndroid::~OpenXrMeshManagerAndroid() {
  if (mesh_tracker_ != XR_NULL_HANDLE) {
    XrResult result = extension_helper_->ExtensionMethods()
                          .xrDestroySceneMeshingTrackerANDROID(mesh_tracker_);
    if (XR_FAILED(result)) {
      DLOG(ERROR) << __func__ << " Failed to destroy Android Mesh Tracker.";
    }
    mesh_tracker_ = XR_NULL_HANDLE;
  }
}

MeshId OpenXrMeshManagerAndroid::GetOrCreateMeshId(const XrUuid& uuid) {
  std::string uuid_str = XrUuidToString(uuid);
  auto it = uuid_to_id_.find(uuid_str);
  if (it != uuid_to_id_.end()) {
    return it->second;
  }
  MeshId new_id = mesh_id_generator_.GenerateNextId();
  uuid_to_id_[uuid_str] = new_id;
  DVLOG(3) << "Created new mesh ID " << new_id.GetUnsafeValue()
           << " for UUID " << uuid_str;
  return new_id;
}


bool OpenXrMeshManagerAndroid::FetchAndConvertSubmeshes(
    XrSceneMeshSnapshotANDROID snapshot,
    std::vector<XrSceneSubmeshDataANDROID>& submeshes,
    const std::vector<XrPosef>& submesh_poses,
    std::vector<mojom::XRMeshDataPtr>& out_meshes) {
  // First call: query vertex/index counts per submesh.
  XrResult result =
      extension_helper_->ExtensionMethods().xrGetSubmeshDataANDROID(
          snapshot, static_cast<uint32_t>(submeshes.size()), submeshes.data());
  if (XR_FAILED(result)) {
    DLOG(ERROR) << "xrGetSubmeshDataANDROID (size query) failed with result="
                << result;
    return false;
  }

  // Allocate per-submesh buffers and point each submesh's data pointers at
  // its own storage. This avoids a flat intermediate buffer and lets us
  // convert directly to Mojo after the data retrieval call.
  std::vector<PerSubmeshBuffers> buffers(submeshes.size());

  for (size_t i = 0; i < submeshes.size(); ++i) {
    auto& submesh = submeshes[i];
    auto& buffer = buffers[i];
    if (submesh.vertexCountOutput > 0) {
      buffer.positions.resize(submesh.vertexCountOutput);
      buffer.normals.resize(submesh.vertexCountOutput);
      buffer.semantics.resize(submesh.vertexCountOutput);
      submesh.vertexCapacityInput = submesh.vertexCountOutput;
      submesh.vertexPositions = buffer.positions.data();
      submesh.vertexNormals = buffer.normals.data();
      submesh.vertexSemantics = buffer.semantics.data();
    } else {
      submesh.vertexCapacityInput = 0;
      submesh.vertexPositions = nullptr;
      submesh.vertexNormals = nullptr;
      submesh.vertexSemantics = nullptr;
    }
    if (submesh.indexCountOutput > 0) {
      buffer.indices.resize(submesh.indexCountOutput);
      submesh.indexCapacityInput = submesh.indexCountOutput;
      submesh.indices = buffer.indices.data();
    } else {
      submesh.indexCapacityInput = 0;
      submesh.indices = nullptr;
    }
  }

  // Second call: retrieve actual mesh data into per-submesh buffers.
  result = extension_helper_->ExtensionMethods().xrGetSubmeshDataANDROID(
      snapshot, static_cast<uint32_t>(submeshes.size()), submeshes.data());
  if (XR_FAILED(result)) {
    DLOG(ERROR) << "xrGetSubmeshDataANDROID (data retrieval) failed with "
                << "result=" << result;
    return false;
  }

  // Convert directly to Mojo structs.
  for (size_t i = 0; i < submeshes.size(); ++i) {
    const auto& submesh = submeshes[i];
    const auto& buffer = buffers[i];
    if (submesh.vertexCountOutput == 0) {
      continue;
    }

    auto mesh = mojom::XRMeshData::New();
    mesh->id = GetOrCreateMeshId(submesh.submeshId);
    mesh->mojo_from_mesh = XrPoseToDevicePose(submesh_poses[i]);

    const uint32_t v_count = submesh.vertexCountOutput;
    mesh->vertices.reserve(v_count * 3);
    for (const auto& pos : buffer.positions) {
      mesh->vertices.push_back(pos.x);
      mesh->vertices.push_back(pos.y);
      mesh->vertices.push_back(pos.z);
    }

    if (submesh.indexCountOutput > 0) {
      mesh->indices.assign(buffer.indices.begin(), buffer.indices.end());
    } else {
      mesh->indices.resize(v_count);
      std::iota(mesh->indices.begin(), mesh->indices.end(), 0u);
    }

    auto label_enum =
        static_cast<XrSceneMeshSemanticLabelANDROID>(buffer.semantics[0]);
    mesh->semantic_label = ToMojomSemanticLabel(label_enum);
    out_meshes.push_back(std::move(mesh));
  }

  return true;
}

mojom::XRMeshDetectionDataPtr OpenXrMeshManagerAndroid::GetDetectedMeshesData(
    XrTime frame_time) {
  if (mesh_tracker_ == XR_NULL_HANDLE) {
    return nullptr;
  }

  // Create a snapshot of the current mesh state. Use a bounding box centered
  // at the origin covering the entire scene so spatial filtering is a no-op.
  // XrBoxf extents are full width/height/depth, not half-extents.
  XrBoxf bounding_box = {
      .center = PoseIdentity(),
      .extents = {kUnboundedSceneHalfExtent * 2, kUnboundedSceneHalfExtent * 2,
                  kUnboundedSceneHalfExtent * 2},
  };
  XrSceneMeshSnapshotCreateInfoANDROID snapshot_create_info = {
      .type = XR_TYPE_SCENE_MESH_SNAPSHOT_CREATE_INFO_ANDROID,
      .next = nullptr,
      .baseSpace = mojo_space_,
      .time = frame_time,
      .boundingBox = bounding_box,
  };
  XrSceneMeshSnapshotCreationResultANDROID snapshot_result = {
      .type = XR_TYPE_SCENE_MESH_SNAPSHOT_CREATION_RESULT_ANDROID,
      .next = nullptr,
      .snapshot = XR_NULL_HANDLE,
      .trackingState = XR_SCENE_MESH_TRACKING_STATE_INITIALIZING_ANDROID,
  };
  XrResult result =
      extension_helper_->ExtensionMethods().xrCreateSceneMeshSnapshotANDROID(
          mesh_tracker_, &snapshot_create_info, &snapshot_result);
  if (XR_FAILED(result)) {
    DLOG(ERROR) << "xrCreateSceneMeshSnapshotANDROID failed with result="
                << result;
    return last_valid_mesh_data_ ? last_valid_mesh_data_->Clone() : nullptr;
  }
  ScopedMeshSnapshot snapshot(
      extension_helper_->ExtensionMethods().xrDestroySceneMeshSnapshotANDROID,
      snapshot_result.snapshot);
  if (snapshot_result.trackingState ==
      XR_SCENE_MESH_TRACKING_STATE_ERROR_ANDROID) {
    DLOG(ERROR) << "Mesh snapshot in ERROR state, unrecoverable";
    return last_valid_mesh_data_ ? last_valid_mesh_data_->Clone() : nullptr;
  }
  if (snapshot_result.trackingState !=
      XR_SCENE_MESH_TRACKING_STATE_TRACKING_ANDROID) {
    DVLOG(2) << "Mesh snapshot not ready, state="
             << snapshot_result.trackingState;
    return last_valid_mesh_data_ ? last_valid_mesh_data_->Clone() : nullptr;
  }

  // Query submesh states.
  uint32_t submesh_count = 0;
  extension_helper_->ExtensionMethods().xrGetAllSubmeshStatesANDROID(
      snapshot.get(), 0, &submesh_count, nullptr);
  if (submesh_count == 0) {
    DVLOG(2) << "Snapshot contains no submeshes";
    return last_valid_mesh_data_ ? last_valid_mesh_data_->Clone() : nullptr;
  }

  std::vector<XrSceneSubmeshStateANDROID> states(submesh_count);
  for (auto& state : states) {
    state.type = XR_TYPE_SCENE_SUBMESH_STATE_ANDROID;
    state.next = nullptr;
  }
  extension_helper_->ExtensionMethods().xrGetAllSubmeshStatesANDROID(
      snapshot.get(), submesh_count, &submesh_count, states.data());

  // Update the cache and build the list of submeshes that need geometry
  // fetched (new or changed since last frame).
  std::vector<XrSceneSubmeshDataANDROID> submeshes;
  std::vector<XrPosef> submesh_poses;
  for (const auto& state : states) {
    std::string uuid_str = XrUuidToString(state.submeshId);
    auto* entry = base::FindOrNull(submesh_cache_, uuid_str);
    if (!entry) {
      entry = &submesh_cache_[uuid_str];
      entry->mesh_id = GetOrCreateMeshId(state.submeshId);
      DVLOG(2) << "New submesh detected";
    }
    if (entry->last_updated_time == state.lastUpdatedTime) {
      continue;
    }
    DVLOG(2) << "Submesh updated";
    entry->last_updated_time = state.lastUpdatedTime;
    submeshes.push_back({
        .type = XR_TYPE_SCENE_SUBMESH_DATA_ANDROID,
        .submeshId = state.submeshId,
    });
    submesh_poses.push_back(state.submeshPoseInBaseSpace);
  }
  DVLOG(1) << "Incremental update - total: " << states.size();

  // Remove stale cache entries (submeshes no longer reported by the runtime).
  absl::flat_hash_set<std::string> current_frame_uuids;
  for (const auto& state : states) {
    current_frame_uuids.insert(XrUuidToString(state.submeshId));
  }
  std::vector<std::string> uuids_to_remove;
  for (const auto& [uuid_str, _] : submesh_cache_) {
    if (!current_frame_uuids.contains(uuid_str)) {
      uuids_to_remove.push_back(uuid_str);
    }
  }
  for (const std::string& uuid_str : uuids_to_remove) {
    submesh_cache_.erase(uuid_str);
  }
  if (!uuids_to_remove.empty()) {
    DVLOG(1) << "Cleaned up " << uuids_to_remove.size()
             << " stale submesh entries from cache";
  }

  auto mesh_data_result = mojom::XRMeshDetectionData::New();
  if (submeshes.empty()) {
    DVLOG(2) << "No submeshes need update, returning cached data";
    for (const auto& [uuid, cache_entry] : submesh_cache_) {
      mesh_data_result->all_meshes_ids.emplace_back(cache_entry.mesh_id);
    }
    return mesh_data_result;
  }

  if (!FetchAndConvertSubmeshes(snapshot.get(), submeshes, submesh_poses,
                                mesh_data_result->updated_meshes_data)) {
    return nullptr;
  }

  for (const auto& [uuid, cache_entry] : submesh_cache_) {
    mesh_data_result->all_meshes_ids.emplace_back(cache_entry.mesh_id);
  }

  if (!mesh_data_result->updated_meshes_data.empty()) {
    last_valid_mesh_data_ = mesh_data_result->Clone();
  }

  return mesh_data_result;
}

void OpenXrMeshManagerAndroid::OnReferenceSpaceChanged() {
  submesh_cache_.clear();
  last_valid_mesh_data_.reset();
}

std::optional<XrLocation> OpenXrMeshManagerAndroid::GetXrLocationFromMesh(
    MeshId mesh_id,
    const gfx::Transform& mesh_id_from_object) const {
  return std::nullopt;
}

}  // namespace device
