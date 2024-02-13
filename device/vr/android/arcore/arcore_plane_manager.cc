// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/arcore/arcore_plane_manager.h"

#include "base/containers/contains.h"
#include "device/vr/android/arcore/vr_service_type_converters.h"

namespace device {

std::pair<gfx::Quaternion, gfx::Point3F> GetPositionAndOrientationFromArPose(
    const ArSession* session,
    const ArPose* pose) {
  std::array<float, 7> pose_raw;  // 7 = orientation(4) + position(3).
  ArPose_getPoseRaw(session, pose, pose_raw.data());

  return {gfx::Quaternion(pose_raw[0], pose_raw[1], pose_raw[2], pose_raw[3]),
          gfx::Point3F(pose_raw[4], pose_raw[5], pose_raw[6])};
}

device::Pose GetPoseFromArPose(const ArSession* session, const ArPose* pose) {
  std::pair<gfx::Quaternion, gfx::Point3F> orientation_and_position =
      GetPositionAndOrientationFromArPose(session, pose);

  return device::Pose(orientation_and_position.second,
                      orientation_and_position.first);
}

device::internal::ScopedArCoreObject<ArPose*> GetArPoseFromMojomPose(
    const ArSession* session,
    const device::mojom::Pose& pose) {
  float pose_raw[7] = {};  // 7 = orientation(4) + position(3).

  pose_raw[0] = pose.orientation.x();
  pose_raw[1] = pose.orientation.y();
  pose_raw[2] = pose.orientation.z();
  pose_raw[3] = pose.orientation.w();

  pose_raw[4] = pose.position.x();
  pose_raw[5] = pose.position.y();
  pose_raw[6] = pose.position.z();

  device::internal::ScopedArCoreObject<ArPose*> result;

  ArPose_create(
      session, pose_raw,
      device::internal::ScopedArCoreObject<ArPose*>::Receiver(result).get());

  return result;
}

ArCorePlaneManager::ArCorePlaneManager(base::PassKey<ArCoreImpl> pass_key,
                                       ArSession* arcore_session)
    : arcore_session_(arcore_session) {
  DCHECK(arcore_session_);

  ArTrackableList_create(
      arcore_session_,
      internal::ScopedArCoreObject<ArTrackableList*>::Receiver(arcore_planes_)
          .get());
  DCHECK(arcore_planes_.is_valid());

  ArPose_create(
      arcore_session_, nullptr,
      internal::ScopedArCoreObject<ArPose*>::Receiver(ar_pose_).get());
  DCHECK(ar_pose_.is_valid());
}

ArCorePlaneManager::~ArCorePlaneManager() = default;

template <typename FunctionType>
void ArCorePlaneManager::ForEachArCorePlane(ArTrackableList* arcore_planes,
                                            FunctionType fn) {
  DCHECK(arcore_planes);

  int32_t trackable_list_size;
  ArTrackableList_getSize(arcore_session_, arcore_planes, &trackable_list_size);

  DVLOG(2) << __func__ << ": arcore_planes size=" << trackable_list_size;

  for (int i = 0; i < trackable_list_size; i++) {
    internal::ScopedArCoreObject<ArTrackable*> trackable;
    ArTrackableList_acquireItem(
        arcore_session_, arcore_planes, i,
        internal::ScopedArCoreObject<ArTrackable*>::Receiver(trackable).get());

    ArTrackingState tracking_state;
    ArTrackable_getTrackingState(arcore_session_, trackable.get(),
                                 &tracking_state);

    if (tracking_state == ArTrackingState::AR_TRACKING_STATE_STOPPED) {
      // Skip all planes that are not currently tracked.
      continue;
    }

#if DCHECK_IS_ON()
    {
      ArTrackableType type;
      ArTrackable_getType(arcore_session_, trackable.get(), &type);
      DCHECK(type == ArTrackableType::AR_TRACKABLE_PLANE)
          << "arcore_planes contains a trackable that is not an ArPlane!";
    }
#endif

    ArPlane* ar_plane =
        ArAsPlane(trackable.get());  // Naked pointer is fine here, ArAsPlane
                                     // does not increase ref count and the
                                     // object is owned by `trackable`.

    internal::ScopedArCoreObject<ArPlane*> subsuming_plane;
    ArPlane_acquireSubsumedBy(
        arcore_session_, ar_plane,
        internal::ScopedArCoreObject<ArPlane*>::Receiver(subsuming_plane)
            .get());

    if (subsuming_plane.is_valid()) {
      // Current plane was subsumed by other plane, skip this loop iteration.
      // Subsuming plane will be handled when its turn comes.
      continue;
    }
    // Pass the ownership of the |trackable| to the |fn|, along with the
    // |ar_plane| that points to the |trackable| but with appropriate type.
    fn(std::move(trackable), ar_plane, tracking_state);
  }
}

void ArCorePlaneManager::Update(ArFrame* ar_frame) {
#if DCHECK_IS_ON()
  DCHECK(was_plane_data_retrieved_in_current_frame_)
      << "Update() must not be called twice in a row without a call to "
         "GetDetectedPlanesData() in between";
  was_plane_data_retrieved_in_current_frame_ = false;
#endif

  ArTrackableType plane_tracked_type = AR_TRACKABLE_PLANE;

  // First, ask ARCore about all Plane trackables updated in the current frame.
  ArFrame_getUpdatedTrackables(arcore_session_, ar_frame, plane_tracked_type,
                               arcore_planes_.get());

  // Collect the IDs of the updated planes. |plane_address_to_id_| might grow.
  std::set<PlaneId> updated_plane_ids;
  ForEachArCorePlane(
      arcore_planes_.get(),
      [this, &updated_plane_ids](
          internal::ScopedArCoreObject<ArTrackable*> trackable,
          ArPlane* ar_plane, ArTrackingState tracking_state) {
        auto result = plane_address_to_id_.CreateOrGetId(ar_plane);

        DVLOG(3) << "Previously detected plane found, plane_id=" << result.id
                 << ", created?=" << result.created
                 << ", tracking_state=" << tracking_state;

        updated_plane_ids.insert(result.id);
      });

  DVLOG(3) << __func__
           << ": updated_plane_ids.size()=" << updated_plane_ids.size();

  // Then, ask about all Plane trackables that are still tracked and
  // non-subsumed.
  ArSession_getAllTrackables(arcore_session_, plane_tracked_type,
                             arcore_planes_.get());

  // Collect the objects of all currently tracked planes.
  // |plane_address_to_id_| should *not* grow.
  std::map<PlaneId, PlaneInfo> new_plane_id_to_plane_info;
  ForEachArCorePlane(
      arcore_planes_.get(),
      [this, &new_plane_id_to_plane_info, &updated_plane_ids](
          internal::ScopedArCoreObject<ArTrackable*> trackable,
          ArPlane* ar_plane, ArTrackingState tracking_state) {
        auto result = plane_address_to_id_.CreateOrGetId(ar_plane);

        DCHECK(!result.created)
            << "Newly detected planes should already be handled - new plane_id="
            << result.id;

        // Inspect the tracking state of this plane in the previous frame. If it
        // changed, mark the plane as updated.
        if (base::Contains(plane_id_to_plane_info_, result.id) &&
            plane_id_to_plane_info_.at(result.id).tracking_state !=
                tracking_state) {
          updated_plane_ids.insert(result.id);
        }

        PlaneInfo new_plane_info =
            PlaneInfo(std::move(trackable), tracking_state);

        new_plane_id_to_plane_info.emplace(result.id,
                                           std::move(new_plane_info));
      });

  DVLOG(3) << __func__ << ": new_plane_id_to_plane_info.size()="
           << new_plane_id_to_plane_info.size();

  // Shrink |plane_address_to_id_|, removing all planes that are no longer
  // tracked or were subsumed - if they do not show up in
  // |new_plane_id_to_plane_info| map, they are no longer tracked.
  plane_address_to_id_.EraseIf(
      [&new_plane_id_to_plane_info](const auto& plane_address_and_id) {
        return !base::Contains(new_plane_id_to_plane_info,
                               plane_address_and_id.second);
      });

  plane_id_to_plane_info_.swap(new_plane_id_to_plane_info);
  updated_plane_ids_.swap(updated_plane_ids);
}

mojom::XRPlaneDetectionDataPtr ArCorePlaneManager::GetDetectedPlanesData()
    const {
  DVLOG(3) << __func__ << ": plane_id_to_plane_info_.size()="
           << plane_id_to_plane_info_.size()
           << ", updated_plane_ids_.size()=" << updated_plane_ids_.size();

  std::vector<uint64_t> all_plane_ids;
  all_plane_ids.reserve(plane_id_to_plane_info_.size());
  for (const auto& plane_id_and_object : plane_id_to_plane_info_) {
    all_plane_ids.push_back(plane_id_and_object.first.GetUnsafeValue());
  }

  std::vector<mojom::XRPlaneDataPtr> updated_planes;
  updated_planes.reserve(updated_plane_ids_.size());
  for (const auto& plane_id : updated_plane_ids_) {
    const device::internal::ScopedArCoreObject<ArTrackable*>& trackable =
        plane_id_to_plane_info_.at(plane_id).plane;

    const ArPlane* ar_plane = ArAsPlane(trackable.get());

    if (plane_id_to_plane_info_.at(plane_id).tracking_state ==
        AR_TRACKING_STATE_TRACKING) {
      // orientation
      ArPlaneType plane_type;
      ArPlane_getType(arcore_session_, ar_plane, &plane_type);

      // pose
      ArPlane_getCenterPose(arcore_session_, ar_plane, ar_pose_.get());
      device::Pose pose = GetPoseFromArPose(arcore_session_, ar_pose_.get());

      // polygon
      int32_t polygon_size;
      ArPlane_getPolygonSize(arcore_session_, ar_plane, &polygon_size);
      // We are supposed to get 2*N floats describing (x, z) cooridinates of N
      // points.
      DCHECK(polygon_size % 2 == 0);

      std::unique_ptr<float[]> vertices_raw =
          std::make_unique<float[]>(polygon_size);
      ArPlane_getPolygon(arcore_session_, ar_plane, vertices_raw.get());

      std::vector<mojom::XRPlanePointDataPtr> vertices;
      for (int i = 0; i < polygon_size; i += 2) {
        vertices.push_back(
            mojom::XRPlanePointData::New(vertices_raw[i], vertices_raw[i + 1]));
      }

      DVLOG(3) << __func__ << ": plane_id: " << plane_id.GetUnsafeValue()
               << ", position=" << pose.position().ToString()
               << ", orientation=" << pose.orientation().ToString();

      updated_planes.push_back(mojom::XRPlaneData::New(
          plane_id.GetUnsafeValue(),
          mojo::ConvertTo<device::mojom::XRPlaneOrientation>(plane_type), pose,
          std::move(vertices)));
    } else {
      DVLOG(3) << __func__ << ": plane_id: " << plane_id.GetUnsafeValue()
               << ", position=untracked, orientation=untracked";

      updated_planes.push_back(mojom::XRPlaneData::New(
          plane_id.GetUnsafeValue(), device::mojom::XRPlaneOrientation::UNKNOWN,
          std::nullopt, std::vector<mojom::XRPlanePointDataPtr>{}));
    }
  }

#if DCHECK_IS_ON()
  was_plane_data_retrieved_in_current_frame_ = true;
#endif

  return mojom::XRPlaneDetectionData::New(std::move(all_plane_ids),
                                          std::move(updated_planes));
}

std::optional<PlaneId> ArCorePlaneManager::GetPlaneId(
    void* plane_address) const {
  return plane_address_to_id_.GetId(plane_address);
}

bool ArCorePlaneManager::PlaneExists(PlaneId id) const {
  return base::Contains(plane_id_to_plane_info_, id);
}

std::optional<gfx::Transform> ArCorePlaneManager::GetMojoFromPlane(
    PlaneId id) const {
  auto it = plane_id_to_plane_info_.find(id);
  if (it == plane_id_to_plane_info_.end()) {
    return std::nullopt;
  }

  // Naked pointer is fine here, ArAsPlane does not increase the internal
  // refcount:
  const ArPlane* plane = ArAsPlane(it->second.plane.get());

  ArPlane_getCenterPose(arcore_session_, plane, ar_pose_.get());
  device::Pose pose = GetPoseFromArPose(arcore_session_, ar_pose_.get());

  return pose.ToTransform();
}

device::internal::ScopedArCoreObject<ArAnchor*>
ArCorePlaneManager::CreateAnchor(base::PassKey<ArCoreAnchorManager> pass_key,
                                 PlaneId id,
                                 const device::mojom::Pose& pose) const {
  auto it = plane_id_to_plane_info_.find(id);
  if (it == plane_id_to_plane_info_.end()) {
    return {};
  }

  auto ar_pose = GetArPoseFromMojomPose(arcore_session_, pose);

  device::internal::ScopedArCoreObject<ArAnchor*> ar_anchor;
  ArStatus status = ArTrackable_acquireNewAnchor(
      arcore_session_, it->second.plane.get(), ar_pose.get(),
      device::internal::ScopedArCoreObject<ArAnchor*>::Receiver(ar_anchor)
          .get());

  if (status != AR_SUCCESS) {
    return {};
  }

  return ar_anchor;
}

ArCorePlaneManager::PlaneInfo::PlaneInfo(
    device::internal::ScopedArCoreObject<ArTrackable*> plane,
    ArTrackingState tracking_state)
    : plane(std::move(plane)), tracking_state(tracking_state) {}

ArCorePlaneManager::PlaneInfo::PlaneInfo(PlaneInfo&& other) = default;
ArCorePlaneManager::PlaneInfo::~PlaneInfo() = default;

}  // namespace device
