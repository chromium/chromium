// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_spatial_anchor_manager.h"

#include <algorithm>
#include <array>
#include <optional>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/callback_helpers.h"
#include "base/types/expected.h"
#include "device/vr/openxr/openxr_api_wrapper.h"
#include "device/vr/openxr/openxr_extension_helper.h"
#include "device/vr/openxr/openxr_platform.h"
#include "device/vr/openxr/openxr_spatial_framework_manager.h"
#include "device/vr/openxr/openxr_spatial_plane_manager.h"
#include "device/vr/openxr/openxr_spatial_utils.h"
#include "device/vr/openxr/openxr_util.h"
#include "device/vr/openxr/scoped_openxr_object.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

// static
bool OpenXrSpatialAnchorManager::IsSupported(
    const std::vector<XrSpatialCapabilityEXT>& capabilities) {
  // The only component that we need to support anchors is
  // XR_SPATIAL_COMPONENT_TYPE_ANCHOR_EXT, which is guaranteed to be supported
  // if the XR_SPATIAL_CAPABILITY_ANCHOR_EXT is supported, so that's all we need
  // to check.
  return base::Contains(capabilities, XR_SPATIAL_CAPABILITY_ANCHOR_EXT);
}

OpenXrSpatialAnchorManager::OpenXrSpatialAnchorManager(
    const OpenXrExtensionHelper& extension_helper,
    const OpenXrSpatialFrameworkManager& spatial_framework_manager,
    OpenXrSpatialPlaneManager* plane_manager,
    XrSpace mojo_space)
    : extension_helper_(extension_helper),
      spatial_framework_manager_(spatial_framework_manager),
      plane_manager_(plane_manager),
      mojo_space_(mojo_space) {}

OpenXrSpatialAnchorManager::~OpenXrSpatialAnchorManager() {
  for (auto const& [id, anchor] : anchors_) {
    extension_helper_->ExtensionMethods().xrDestroySpatialEntityEXT(
        anchor.entity);
  }
}

void OpenXrSpatialAnchorManager::PopulateCapabilityConfiguration(
    absl::flat_hash_map<XrSpatialCapabilityEXT,
                        absl::flat_hash_set<XrSpatialComponentTypeEXT>>&
        capability_configuration) const {
  // Operator[] creates an empty entry if it does not exist.
  capability_configuration[XR_SPATIAL_CAPABILITY_ANCHOR_EXT].insert(
      XR_SPATIAL_COMPONENT_TYPE_ANCHOR_EXT);

  // In order to set a Plane as our Parent we need to also enable the parent
  // component type. Note that this component is guaranteed by the same
  // extension that allows the plane manager to say that they can be our parent.
  if (plane_manager_ && plane_manager_->can_parent_anchors()) {
    capability_configuration[XR_SPATIAL_CAPABILITY_ANCHOR_EXT].insert(
        XR_SPATIAL_COMPONENT_TYPE_PARENT_EXT);
  }
}

AnchorId OpenXrSpatialAnchorManager::CreateAnchor(
    XrPosef pose,
    XrSpace space,
    XrTime predicted_display_time,
    std::optional<PlaneId> plane_id) {
  XrSpatialAnchorCreateInfoEXT create_info{
      XR_TYPE_SPATIAL_ANCHOR_CREATE_INFO_EXT};
  create_info.baseSpace = space;
  create_info.time = predicted_display_time;
  create_info.pose = pose;

  XrSpatialEntityIdEXT parent_entity = XR_NULL_SPATIAL_ENTITY_ID_EXT;
  if (plane_manager_ && plane_manager_->can_parent_anchors() &&
      plane_id.has_value()) {
    parent_entity = plane_manager_->GetEntityId(*plane_id);
  }

  // This type is small enough to create even if we won't use it, since it needs
  // to be created outside the scope of the if block to attach it to the `next`
  // chain.
  XrSpatialAnchorParentANDROID parent_info{
      .type = XR_TYPE_SPATIAL_ANCHOR_PARENT_ANDROID, .parentId = parent_entity};

  if (parent_entity != XR_NULL_SPATIAL_ENTITY_ID_EXT) {
    create_info.next = &parent_info;
  }

  SpatialAnchorData anchor_data;
  if (XR_FAILED(extension_helper_->ExtensionMethods().xrCreateSpatialAnchorEXT(
          spatial_framework_manager_->GetSpatialContext(), &create_info,
          &anchor_data.entity_id, &anchor_data.entity))) {
    return kInvalidAnchorId;
  }

  AnchorId anchor_id = anchor_id_generator_.GenerateNextId();
  CHECK(anchor_id);
  anchors_.emplace(anchor_id, anchor_data);
  entity_id_to_anchor_id_.emplace(anchor_data.entity_id, anchor_id);
  return anchor_id;
}

void OpenXrSpatialAnchorManager::DetachAnchor(AnchorId anchor_id) {
  auto it = anchors_.find(anchor_id);
  if (it == anchors_.end()) {
    return;
  }

  extension_helper_->ExtensionMethods().xrDestroySpatialEntityEXT(
      it->second.entity);
  entity_id_to_anchor_id_.erase(it->second.entity_id);
  anchors_.erase(it);
  cached_anchor_poses_.erase(anchor_id);
}

std::optional<XrLocation> OpenXrSpatialAnchorManager::GetXrLocationFromAnchor(
    AnchorId anchor_id,
    const gfx::Transform& anchor_id_from_new_anchor) const {
  auto it = cached_anchor_poses_.find(anchor_id);
  if (it == cached_anchor_poses_.end() || !it->second.has_value()) {
    return std::nullopt;
  }

  // The cached anchor poses are in |mojo_space_| hence mojo_from_anchor_id.
  gfx::Transform mojo_from_anchor_id = it->second->ToTransform();
  gfx::Transform mojo_from_new_anchor =
      mojo_from_anchor_id * anchor_id_from_new_anchor;

  return XrLocation{GfxTransformToXrPose(mojo_from_new_anchor), mojo_space_};
}

mojom::XRAnchorsDataPtr OpenXrSpatialAnchorManager::GetCurrentAnchorsData(
    XrTime predicted_display_time) {
  cached_anchor_poses_.clear();
  if (anchors_.empty()) {
    return nullptr;
  }

  std::vector<XrSpatialEntityEXT> entities;
  entities.reserve(anchors_.size());
  for (auto const& [_, anchor] : anchors_) {
    entities.push_back(anchor.entity);
  }

  XrSpatialUpdateSnapshotCreateInfoEXT snapshot_create_info{
      XR_TYPE_SPATIAL_UPDATE_SNAPSHOT_CREATE_INFO_EXT};
  snapshot_create_info.entityCount = entities.size();
  snapshot_create_info.entities = entities.data();
  snapshot_create_info.baseSpace = mojo_space_;
  snapshot_create_info.time = predicted_display_time;

  ScopedOpenXrObject<XrSpatialSnapshotEXT> snapshot(extension_helper_.get());
  if (XR_FAILED(extension_helper_->ExtensionMethods()
                    .xrCreateSpatialUpdateSnapshotEXT(
                        spatial_framework_manager_->GetSpatialContext(),
                        &snapshot_create_info, snapshot.receive()))) {
    return nullptr;
  }

  constexpr std::array<XrSpatialComponentTypeEXT, 1> kComponentTypes = {
      XR_SPATIAL_COMPONENT_TYPE_ANCHOR_EXT};
  XrSpatialComponentDataQueryConditionEXT query_cond{
      XR_TYPE_SPATIAL_COMPONENT_DATA_QUERY_CONDITION_EXT};
  query_cond.componentTypeCount = kComponentTypes.size();
  query_cond.componentTypes = kComponentTypes.data();

  // Locations is an out parameter, so pre-fill it with empty poses.
  std::vector<XrPosef> locations(anchors_.size());
  XrSpatialComponentAnchorListEXT location_list{
      XR_TYPE_SPATIAL_COMPONENT_ANCHOR_LIST_EXT};
  location_list.locationCount = locations.size();
  location_list.locations = locations.data();

  XrSpatialComponentDataQueryResultEXT query_result{
      XR_TYPE_SPATIAL_COMPONENT_DATA_QUERY_RESULT_EXT};
  query_result.next = &location_list;

  // tracking_states is an out parameter, so pre-fill it with empty poses.
  std::vector<XrSpatialEntityTrackingStateEXT> tracking_states(anchors_.size());
  query_result.entityStateCapacityInput = tracking_states.size();
  query_result.entityStates = tracking_states.data();

  // entity_ids is an out parameter, so pre-fill it with empty poses.
  std::vector<XrSpatialEntityIdEXT> entity_ids(anchors_.size());
  query_result.entityIdCapacityInput = entity_ids.size();
  query_result.entityIds = entity_ids.data();

  if (XR_FAILED(
          extension_helper_->ExtensionMethods().xrQuerySpatialComponentDataEXT(
              snapshot.get(), &query_cond, &query_result))) {
    return nullptr;
  }

  // No matter what, we will send all current anchors up to the page, and then
  // we will remove any deleted ones for the next frame.
  std::vector<AnchorId> all_anchors_ids;
  std::vector<mojom::XRAnchorDataPtr> updated_anchors;
  all_anchors_ids.reserve(anchors_.size());
  updated_anchors.reserve(anchors_.size());
  absl::flat_hash_set<AnchorId> permanently_stopped_anchors;

  // Per the spec, the order of entities in the `entity_ids` list matches the
  // order of the entities in both the tracking states and locations lists.
  for (uint32_t i = 0; i < query_result.entityIdCountOutput; i++) {
    auto it = entity_id_to_anchor_id_.find(entity_ids[i]);
    if (it == entity_id_to_anchor_id_.end()) {
      continue;
    }

    AnchorId anchor_id = it->second;
    all_anchors_ids.push_back(anchor_id);

    switch (tracking_states[i]) {
      case XR_SPATIAL_ENTITY_TRACKING_STATE_TRACKING_EXT: {
        // If the anchor is tracked, it has a valid pose, and we should cache it
        // and then send that pose as the updated location.
        auto pose = XrPoseToDevicePose(locations[i]);
        cached_anchor_poses_.emplace(anchor_id, pose);
        updated_anchors.push_back(mojom::XRAnchorData::New(anchor_id, pose));
        break;
      }
      case XR_SPATIAL_ENTITY_TRACKING_STATE_PAUSED_EXT: {
        // If it's paused we'll still put it in the cache as it may come back,
        // but we don't know a pose for it at present.
        cached_anchor_poses_.emplace(anchor_id, std::nullopt);
        updated_anchors.push_back(
            mojom::XRAnchorData::New(anchor_id, std::nullopt));
        break;
      }
      case XR_SPATIAL_ENTITY_TRACKING_STATE_STOPPED_EXT: {
        // Permanently stopped anchors will never become locatable again. Still
        // send up a pose for this frame, but don't cache it for future lookups
        // and remove it from the list after we're done processing.
        permanently_stopped_anchors.insert(anchor_id);
        updated_anchors.push_back(
            mojom::XRAnchorData::New(anchor_id, std::nullopt));
        break;
      }
      case XR_SPATIAL_ENTITY_TRACKING_STATE_MAX_ENUM_EXT:
        NOTREACHED();
    }
  }

  // Check that every anchor we are tracking is accounted for in the query
  // results. If we are tracking an anchor that was not returned in the query,
  // it's in an unknown state, so we should treat it as paused.
  if (cached_anchor_poses_.size() + permanently_stopped_anchors.size() !=
      anchors_.size()) {
    DVLOG(1) << __func__ << " Not all tracked anchors were updated!";
    for (const auto& [id, anchor_data] : anchors_) {
      if (!base::Contains(cached_anchor_poses_, id) &&
          !base::Contains(permanently_stopped_anchors, id)) {
        DVLOG(3) << __func__
                 << " Did not receive an update for: " << id.GetUnsafeValue();
        cached_anchor_poses_.emplace(id, std::nullopt);
      }
    }
  }

  for (auto const& id : permanently_stopped_anchors) {
    DetachAnchor(id);
  }

  return mojom::XRAnchorsData::New(std::move(all_anchors_ids),
                                   std::move(updated_anchors));
}

}  // namespace device
