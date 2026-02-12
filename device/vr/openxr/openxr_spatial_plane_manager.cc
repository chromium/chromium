// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_spatial_plane_manager.h"

#include <algorithm>

#include "device/vr/openxr/openxr_api_wrapper.h"
#include "device/vr/openxr/openxr_extension_helper.h"
#include "device/vr/openxr/openxr_spatial_framework_manager.h"
#include "device/vr/openxr/openxr_spatial_utils.h"
#include "device/vr/openxr/openxr_util.h"
#include "ui/gfx/geometry/transform.h"

namespace device {

namespace {
mojom::XRPlaneOrientation ToMojomPlaneOrientation(
    const XrSpatialPlaneAlignmentEXT& alignment) {
  switch (alignment) {
    case XR_SPATIAL_PLANE_ALIGNMENT_HORIZONTAL_UPWARD_EXT:
    case XR_SPATIAL_PLANE_ALIGNMENT_HORIZONTAL_DOWNWARD_EXT:
      return mojom::XRPlaneOrientation::HORIZONTAL;
    case XR_SPATIAL_PLANE_ALIGNMENT_VERTICAL_EXT:
      return mojom::XRPlaneOrientation::VERTICAL;
    default:
      return mojom::XRPlaneOrientation::UNKNOWN;
  }
}

mojom::XRSemanticLabel ToMojomSemanticLabel(
    const XrSpatialPlaneSemanticLabelEXT& label) {
  switch (label) {
    case XR_SPATIAL_PLANE_SEMANTIC_LABEL_FLOOR_EXT:
      return mojom::XRSemanticLabel::kFloor;
    case XR_SPATIAL_PLANE_SEMANTIC_LABEL_WALL_EXT:
      return mojom::XRSemanticLabel::kWall;
    case XR_SPATIAL_PLANE_SEMANTIC_LABEL_CEILING_EXT:
      return mojom::XRSemanticLabel::kCeiling;
    case XR_SPATIAL_PLANE_SEMANTIC_LABEL_TABLE_EXT:
      return mojom::XRSemanticLabel::kTable;
    case XR_SPATIAL_PLANE_SEMANTIC_LABEL_UNCATEGORIZED_EXT:
    default:
      return mojom::XRSemanticLabel::kOther;
  }
}

std::vector<XrSpatialComponentTypeEXT> GetAttachableComponentTypes(
    const OpenXrExtensionMethods& extension_methods,
    XrInstance instance,
    XrSystemId system) {
  // This is an optional extension.
  if (extension_methods.xrEnumerateSpatialAnchorAttachableComponentsANDROID ==
      nullptr) {
    return {};
  }

  uint32_t attachable_component_count;
  if (XR_FAILED(
          extension_methods.xrEnumerateSpatialAnchorAttachableComponentsANDROID(
              instance, system, 0, &attachable_component_count, nullptr))) {
    return {};
  }

  std::vector<XrSpatialComponentTypeEXT> attachable_components(
      attachable_component_count);
  if (XR_FAILED(
          extension_methods.xrEnumerateSpatialAnchorAttachableComponentsANDROID(
              instance, system,
              static_cast<uint32_t>(attachable_components.size()),
              &attachable_component_count, attachable_components.data()))) {
    attachable_components.clear();
  }

  return attachable_components;
}
}  // namespace

// static
bool OpenXrSpatialPlaneManager::IsSupported(
    const std::vector<XrSpatialCapabilityEXT>& capabilities) {
  // The only components that we need to support planes are
  // XR_SPATIAL_COMPONENT_TYPE_BOUNDED_2D_EXT and
  // XR_SPATIAL_COMPONENT_TYPE_PLANE_ALIGNMENT_EXT which are guaranteed to be
  // supported if the XR_SPATIAL_CAPABILITY_PLANE_TRACKING_EXT is supported, so
  //  that's all we need to check.
  return std::ranges::contains(capabilities,
                               XR_SPATIAL_CAPABILITY_PLANE_TRACKING_EXT);
}

OpenXrSpatialPlaneManager::OpenXrSpatialPlaneManager(
    XrSpace mojo_space,
    const OpenXrExtensionHelper& extension_helper,
    const OpenXrSpatialFrameworkManager& framework_manager,
    XrInstance instance,
    XrSystemId system)
    : mojo_space_(mojo_space),
      extension_helper_(extension_helper),
      framework_manager_(framework_manager),
      enabled_components_({// Begin by enabling the two components required to
                           // be present for the
                           // XR_SPATIAL_CAPABILITY_PLANE_TRACKING_EXT
                           XR_SPATIAL_COMPONENT_TYPE_BOUNDED_2D_EXT,
                           XR_SPATIAL_COMPONENT_TYPE_PLANE_ALIGNMENT_EXT}) {
  std::vector<XrSpatialComponentTypeEXT> plane_tracking_components =
      GetSupportedComponentTypes(
          extension_helper_->ExtensionMethods()
              .xrEnumerateSpatialCapabilityComponentTypesEXT,
          instance, system, XR_SPATIAL_CAPABILITY_PLANE_TRACKING_EXT);

  polygon_enabled_ = std::ranges::contains(
      plane_tracking_components, XR_SPATIAL_COMPONENT_TYPE_POLYGON_2D_EXT);
  if (polygon_enabled_) {
    enabled_components_.insert(XR_SPATIAL_COMPONENT_TYPE_POLYGON_2D_EXT);
  }

  semantic_label_enabled_ =
      std::ranges::contains(plane_tracking_components,
                            XR_SPATIAL_COMPONENT_TYPE_PLANE_SEMANTIC_LABEL_EXT);
  if (semantic_label_enabled_) {
    enabled_components_.insert(
        XR_SPATIAL_COMPONENT_TYPE_PLANE_SEMANTIC_LABEL_EXT);
  }

  std::vector<XrSpatialComponentTypeEXT> attachable_components =
      GetAttachableComponentTypes(extension_helper_->ExtensionMethods(),
                                  instance, system);

  // If any of our attachable components are in the supportable plane components
  // list, we can attach anchors to planes.
  auto first_attachable_component = std::find_if(
      attachable_components.begin(), attachable_components.end(),
      [&plane_tracking_components](XrSpatialComponentTypeEXT component) {
        return std::ranges::contains(plane_tracking_components, component);
      });

  if (first_attachable_component != attachable_components.end()) {
    // In order to properly support parenting, we have to ensure the component
    // of ours that is attachable is enabled. First check if any of our planned
    // to be enabled components are attachable.
    bool attachable_component_enabled = std::any_of(
        enabled_components_.begin(), enabled_components_.end(),
        [&attachable_components](XrSpatialComponentTypeEXT component) {
          return std::ranges::contains(attachable_components, component);
        });

    // If not, let's enable the first attachable component that we found, since
    // any of them will do. It's only use will be to enable parenting, we don't
    // actually need to query for it.
    if (!attachable_component_enabled) {
      enabled_components_.insert(*first_attachable_component);
    }

    can_parent_anchors_ = true;
  }
}

OpenXrSpatialPlaneManager::~OpenXrSpatialPlaneManager() = default;

void OpenXrSpatialPlaneManager::PopulateCapabilityConfiguration(
    absl::flat_hash_map<XrSpatialCapabilityEXT,
                        absl::flat_hash_set<XrSpatialComponentTypeEXT>>&
        capability_configuration) const {
  capability_configuration[XR_SPATIAL_CAPABILITY_PLANE_TRACKING_EXT].insert(
      enabled_components_.begin(), enabled_components_.end());
}

void OpenXrSpatialPlaneManager::OnSnapshotChanged() {
  const XrSpatialSnapshotEXT snapshot =
      framework_manager_->GetDiscoverySnapshot();
  if (snapshot == XR_NULL_HANDLE) {
    return;
  }

  // Query the snapshot for all entities that have the necessary plane
  // components. Note that we don't use enabled_components_ here, because these
  // are the only values we actually care about.
  XrSpatialComponentDataQueryConditionEXT query_condition{
      XR_TYPE_SPATIAL_COMPONENT_DATA_QUERY_CONDITION_EXT};
  std::vector<XrSpatialComponentTypeEXT> component_types = {
      XR_SPATIAL_COMPONENT_TYPE_BOUNDED_2D_EXT,
      XR_SPATIAL_COMPONENT_TYPE_PLANE_ALIGNMENT_EXT};

  if (polygon_enabled_) {
    component_types.push_back(XR_SPATIAL_COMPONENT_TYPE_POLYGON_2D_EXT);
  }

  if (semantic_label_enabled_) {
    component_types.push_back(
        XR_SPATIAL_COMPONENT_TYPE_PLANE_SEMANTIC_LABEL_EXT);
  }

  query_condition.componentTypeCount = component_types.size();
  query_condition.componentTypes = component_types.data();

  // First need to query for how many results there are, then we can build the
  // arrays to populate.
  XrSpatialComponentDataQueryResultEXT query_result{
      XR_TYPE_SPATIAL_COMPONENT_DATA_QUERY_RESULT_EXT};
  if (XR_FAILED(
          extension_helper_->ExtensionMethods().xrQuerySpatialComponentDataEXT(
              snapshot, &query_condition, &query_result))) {
    return;
  }

  std::vector<XrSpatialEntityIdEXT> entity_ids(
      query_result.entityIdCountOutput);
  query_result.entityIdCapacityInput = entity_ids.size();
  query_result.entityIds = entity_ids.data();

  std::vector<XrSpatialEntityTrackingStateEXT> entity_states(
      query_result.entityIdCountOutput);
  query_result.entityStateCapacityInput = entity_states.size();
  query_result.entityStates = entity_states.data();

  XrNextChainBuilder next_chain(&query_result);

  std::vector<XrSpatialPlaneAlignmentEXT> plane_alignments(
      query_result.entityIdCountOutput);
  XrSpatialComponentPlaneAlignmentListEXT plane_alignment_list{
      .type = XR_TYPE_SPATIAL_COMPONENT_PLANE_ALIGNMENT_LIST_EXT,
      .planeAlignmentCount = static_cast<uint32_t>(plane_alignments.size()),
      .planeAlignments = plane_alignments.data()};
  next_chain.Add(&plane_alignment_list);

  std::vector<XrSpatialBounded2DDataEXT> bounded_2d_data(
      query_result.entityIdCountOutput);
  XrSpatialComponentBounded2DListEXT bounded_2d_list{
      .type = XR_TYPE_SPATIAL_COMPONENT_BOUNDED_2D_LIST_EXT,
      .boundCount = static_cast<uint32_t>(bounded_2d_data.size()),
      .bounds = bounded_2d_data.data()};
  next_chain.Add(&bounded_2d_list);

  std::vector<XrSpatialPolygon2DDataEXT> polygons;
  XrSpatialComponentPolygon2DListEXT polygon_list{
      XR_TYPE_SPATIAL_COMPONENT_POLYGON_2D_LIST_EXT};
  if (polygon_enabled_) {
    polygons.resize(query_result.entityIdCountOutput);
    polygon_list.polygonCount = static_cast<uint32_t>(polygons.size());
    polygon_list.polygons = polygons.data();
    next_chain.Add(&polygon_list);
  }

  std::vector<XrSpatialPlaneSemanticLabelEXT> semantic_labels;
  XrSpatialComponentPlaneSemanticLabelListEXT semantic_label_list{
      XR_TYPE_SPATIAL_COMPONENT_PLANE_SEMANTIC_LABEL_LIST_EXT};
  if (semantic_label_enabled_) {
    semantic_labels.resize(query_result.entityIdCountOutput);
    semantic_label_list.semanticLabelCount =
        static_cast<uint32_t>(semantic_labels.size());
    semantic_label_list.semanticLabels = semantic_labels.data();
    next_chain.Add(&semantic_label_list);
  }

  if (XR_FAILED(
          extension_helper_->ExtensionMethods().xrQuerySpatialComponentDataEXT(
              snapshot, &query_condition, &query_result))) {
    return;
  }

  // Reset our list of updated planes. We could potentially be clearing a plane
  // that we said had a pending update but now we don't know about. Since we no
  // longer know about it, then we shouldn't be reporting it.
  updated_entity_ids_.clear();
  absl::flat_hash_set<XrSpatialEntityIdEXT> paused_entity_ids;
  for (uint32_t i = 0; i < query_result.entityIdCountOutput; i++) {
    XrSpatialEntityIdEXT entity_id = entity_ids[i];
    // We don't need to send up any information about stopped planes, and since
    // planes could be subsumed, we'll just process and clear outdated entries
    // every time as well.
    // We'll note paused planes differently. They won't count as updated this
    // frame, but we'll keep them around/existing.
    if (entity_states[i] == XR_SPATIAL_ENTITY_TRACKING_STATE_PAUSED_EXT) {
      paused_entity_ids.insert(entity_id);
      continue;
    }

    if (entity_states[i] != XR_SPATIAL_ENTITY_TRACKING_STATE_TRACKING_EXT) {
      continue;
    }

    // If we don't have an entry for this entity ID, populate the data for it.
    if (!entity_id_to_data_.contains(entity_id)) {
      entity_id_to_data_[entity_id] = mojom::XRPlaneData::New();
    }

    updated_entity_ids_.insert(entity_id);
    mojom::XRPlaneDataPtr& plane_data = entity_id_to_data_[entity_id];

    // Can't use `GetPlaneId` until our entity_id is in the map.
    plane_data->id = GetPlaneId(entity_id);
    plane_data->orientation = ToMojomPlaneOrientation(plane_alignments[i]);

    if (semantic_label_enabled_) {
      plane_data->semantic_label = ToMojomSemanticLabel(semantic_labels[i]);
    }

    plane_data->polygon.clear();
    bool has_polygon = false;
    if (polygon_enabled_ && i < polygons.size()) {
      has_polygon = GetPolygonFromBuffer(snapshot, polygons[i], plane_data);
    }

    if (!has_polygon) {
      GetPolygonFromExtent(bounded_2d_data[i], plane_data);
    }
  }

  // Remove any planes that are no longer being tracked.
  auto it = entity_id_to_data_.begin();
  while (it != entity_id_to_data_.end()) {
    // If a plane was updated or marked as paused, keep it around. Otherwise,
    // it was either not reported or reported as stopped, so delete it.
    if (updated_entity_ids_.contains(it->first) ||
        paused_entity_ids.contains(it->first)) {
      it++;
    } else {
      entity_id_to_data_.erase(it++);
    }
  }
}

bool OpenXrSpatialPlaneManager::GetPolygonFromBuffer(
    XrSpatialSnapshotEXT snapshot,
    const XrSpatialPolygon2DDataEXT& polygon_data,
    mojom::XRPlaneDataPtr& plane_data) const {
  XrSpatialBufferGetInfoEXT buffer_info{XR_TYPE_SPATIAL_BUFFER_GET_INFO_EXT};
  buffer_info.bufferId = polygon_data.vertexBuffer.bufferId;
  uint32_t buffer_count_output = 0;
  if (XR_FAILED(
          extension_helper_->ExtensionMethods().xrGetSpatialBufferVector2fEXT(
              snapshot, &buffer_info, 0, &buffer_count_output, nullptr))) {
    return false;
  }

  std::vector<XrVector2f> vertices(buffer_count_output);
  if (XR_FAILED(
          extension_helper_->ExtensionMethods().xrGetSpatialBufferVector2fEXT(
              snapshot, &buffer_info, buffer_count_output, &buffer_count_output,
              vertices.data()))) {
    return false;
  }

  // The incoming pose has the Z axis as the normal, but WebXR expects the Y
  // axis to be the normal.
  plane_data->mojo_from_plane =
      ZNormalXrPoseToYNormalDevicePose(polygon_data.origin);

  // OpenXR provides a counterclockwise polygon that is guaranteed to not self
  // intersect; however, we do need to transform from XY space to XZ space as
  // expected by the spec.
  for (const auto& vertex : vertices) {
    // Vertices are 2D (X, Y) in the polygon's space (Z=0).
    // We need to transform them to WebXR's plane space (Y-up).
    // Construct a point for the vertex.
    gfx::Point3F vertex_point = {vertex.x, vertex.y, 0};
    // Transform from Z-normal to Y-normal.
    auto webxr_vertex_pose = ZNormalPositionToYNormalPosition(vertex_point);
    // Now that it's Y-Normal, Y should be 0, and we send up the expected XZ
    // coordinates.
    plane_data->polygon.push_back(mojom::XRPlanePointData::New(
        webxr_vertex_pose.x(), webxr_vertex_pose.z()));
  }

  return true;
}

void OpenXrSpatialPlaneManager::GetPolygonFromExtent(
    const XrSpatialBounded2DDataEXT& bounded_2d_data,
    mojom::XRPlaneDataPtr& plane_data) const {
  // The incoming pose has the Z axis as the normal, but WebXR expects the Y
  // axis to be the normal.
  plane_data->mojo_from_plane =
      ZNormalXrPoseToYNormalDevicePose(bounded_2d_data.center);
  plane_data->polygon.clear();

  // Create a rectangle from the extents with a counter-clockwise winding.
  const auto& extents = bounded_2d_data.extents;
  plane_data->polygon.push_back(
      mojom::XRPlanePointData::New(-extents.width / 2, -extents.height / 2));
  plane_data->polygon.push_back(
      mojom::XRPlanePointData::New(extents.width / 2, -extents.height / 2));
  plane_data->polygon.push_back(
      mojom::XRPlanePointData::New(extents.width / 2, extents.height / 2));
  plane_data->polygon.push_back(
      mojom::XRPlanePointData::New(-extents.width / 2, extents.height / 2));
}

mojom::XRPlaneDetectionDataPtr
OpenXrSpatialPlaneManager::GetDetectedPlanesData() {
  auto planes_data = mojom::XRPlaneDetectionData::New();

  for (const auto& [entity_id, data] : entity_id_to_data_) {
    planes_data->all_planes_ids.push_back(GetPlaneId(entity_id));
    if (updated_entity_ids_.contains(entity_id)) {
      planes_data->updated_planes_data.push_back(data.Clone());
    }
  }

  updated_entity_ids_.clear();
  return planes_data;
}

std::optional<device::Pose> OpenXrSpatialPlaneManager::TryGetMojoFromPlane(
    PlaneId plane_id) const {
  auto it = entity_id_to_data_.find(GetEntityId(plane_id));
  if (it == entity_id_to_data_.end() || !it->second->mojo_from_plane) {
    return std::nullopt;
  }

  return it->second->mojo_from_plane;
}

PlaneId OpenXrSpatialPlaneManager::GetPlaneId(
    XrSpatialEntityIdEXT entity_id) const {
  if (entity_id == XR_NULL_SPATIAL_ENTITY_ID_EXT ||
      !entity_id_to_data_.contains(entity_id)) {
    return kInvalidPlaneId;
  }

  return PlaneId(static_cast<uint64_t>(entity_id));
}

XrSpatialEntityIdEXT OpenXrSpatialPlaneManager::GetEntityId(
    PlaneId plane_id) const {
  if (plane_id == kInvalidPlaneId) {
    return XR_NULL_SPATIAL_ENTITY_ID_EXT;
  }

  auto entity_id = static_cast<XrSpatialEntityIdEXT>(plane_id.GetUnsafeValue());
  if (!entity_id_to_data_.contains(entity_id)) {
    return XR_NULL_SPATIAL_ENTITY_ID_EXT;
  }

  return entity_id;
}

std::optional<XrLocation> OpenXrSpatialPlaneManager::GetXrLocationFromPlane(
    PlaneId plane_id,
    const gfx::Transform& plane_id_from_object) const {
  // We don't have an xr_space_ for the plane, so we'll just locate the pose
  // in mojo_space_ and send that up as the base of the XrLocation.
  std::optional<device::Pose> mojo_from_plane = TryGetMojoFromPlane(plane_id);
  if (!mojo_from_plane) {
    return std::nullopt;
  }

  gfx::Transform mojo_from_new_anchor =
      mojo_from_plane->ToTransform() * plane_id_from_object;
  return XrLocation{GfxTransformToXrPose(mojo_from_new_anchor), mojo_space_};
}

}  // namespace device
