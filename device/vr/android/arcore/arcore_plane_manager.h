// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_ARCORE_ARCORE_PLANE_MANAGER_H_
#define DEVICE_VR_ANDROID_ARCORE_ARCORE_PLANE_MANAGER_H_

#include <map>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/types/id_type.h"
#include "base/types/pass_key.h"
#include "device/vr/android/arcore/address_to_id_map.h"
#include "device/vr/android/arcore/arcore_sdk.h"
#include "device/vr/android/arcore/scoped_arcore_objects.h"
#include "device/vr/public/mojom/vr_service.mojom.h"

namespace device {

class ArCoreImpl;
class ArCoreAnchorManager;

using PlaneId = base::IdTypeU64<class PlaneTag>;

std::pair<gfx::Quaternion, gfx::Point3F> GetPositionAndOrientationFromArPose(
    const ArSession* session,
    const ArPose* pose);

device::Pose GetPoseFromArPose(const ArSession* session, const ArPose* pose);

device::internal::ScopedArCoreObject<ArPose*> GetArPoseFromMojomPose(
    const ArSession* session,
    const device::mojom::Pose& pose);

class ArCorePlaneManager {
 public:
  ArCorePlaneManager(base::PassKey<ArCoreImpl> pass_key,
                     ArSession* arcore_session);
  ~ArCorePlaneManager();

  // Updates plane manager state - it should be called in every frame if the
  // ARCore session supports plane detection. Currently, if the WebXR session
  // supports hit test feature or plane detection feature, the ARCore session
  // needs to be configured with planes enabled and this method needs to be
  // called.
  void Update(ArFrame* ar_frame);

  mojom::XRPlaneDetectionDataPtr GetDetectedPlanesData() const;

  bool PlaneExists(PlaneId id) const;

  // Returns std::nullopt if plane with the given address does not exist.
  std::optional<PlaneId> GetPlaneId(void* plane_address) const;

  // Returns std::nullopt if plane with the given id does not exist.
  std::optional<gfx::Transform> GetMojoFromPlane(PlaneId id) const;

  // Creates Anchor object given a plane ID. This is needed since Plane objects
  // are managed by this class in its entirety and are not accessible outside
  // it. Callable only from ArCoreAnchorManager.
  device::internal::ScopedArCoreObject<ArAnchor*> CreateAnchor(
      base::PassKey<ArCoreAnchorManager> pass_key,
      PlaneId id,
      const device::mojom::Pose& pose) const;

 private:
  struct PlaneInfo {
    device::internal::ScopedArCoreObject<ArTrackable*> plane;
    ArTrackingState tracking_state;

    PlaneInfo(device::internal::ScopedArCoreObject<ArTrackable*> plane,
              ArTrackingState tracking_state);
    PlaneInfo(PlaneInfo&& other);
    ~PlaneInfo();
  };

  // Executes |fn| for each still tracked, non-subsumed plane present in
  // |arcore_planes|. |fn| will receive 3 parameters - a
  // `ScopedArCoreObject<ArAnchor*>` that can be stored, the non-owning ArPlane*
  // typecast from the first parameter, and ArTrackingState. A plane is tracked
  // if its state is not AR_TRACKING_STATE_STOPPED.
  template <typename FunctionType>
  void ForEachArCorePlane(ArTrackableList* arcore_planes, FunctionType fn);

  // Owned by ArCoreImpl - non-owning pointer is fine since ArCorePlaneManager
  // is also owned by ArCoreImpl.
  raw_ptr<ArSession> arcore_session_;

  // List of trackables - used for retrieving planes detected by ARCore.
  // Allows reuse of the list across updates; ARCore clears the list on each
  // call to the ARCore SDK.
  internal::ScopedArCoreObject<ArTrackableList*> arcore_planes_;
  // Allows reuse of the pose object; ARCore will populate it with new data on
  // each call to the ARCore SDK.
  internal::ScopedArCoreObject<ArPose*> ar_pose_;

  // Mapping from plane address to plane ID. It should be modified only during
  // calls to |Update()|.
  AddressToIdMap<PlaneId> plane_address_to_id_;
  // Mapping from plane ID to ARCore plane information. It should be modified
  // only during calls to |Update()|.
  std::map<PlaneId, PlaneInfo> plane_id_to_plane_info_;
  // Set containing IDs of planes updated in the last frame. It should be
  // modified only during calls to |Update()|.
  std::set<PlaneId> updated_plane_ids_;

#if DCHECK_IS_ON()
  // True if |GetDetectedPlanesData()| was called after |Update()|. It is used
  // to track if |Update()| was called twice in a row w/o a call to
  // |GetDetectedPlanesData()| in between. Initially true since we expect the
  // call to |Update()| to happen next.
  // TODO(crbug.com/40757459): remove the assumption that the calls to
  // |Update()| will always be followed by at least one call to
  // |GetDetectedPlanesData()| before the next call to |Update()| happens.
  mutable bool was_plane_data_retrieved_in_current_frame_ = true;
#endif
};

}  // namespace device

#endif  // DEVICE_VR_ANDROID_ARCORE_ARCORE_PLANE_MANAGER_H_
