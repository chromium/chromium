// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_ARCORE_ARCORE_ANCHOR_MANAGER_H_
#define DEVICE_VR_ANDROID_ARCORE_ARCORE_ANCHOR_MANAGER_H_

#include <map>

#include "base/types/pass_key.h"
#include "base/util/type_safety/id_type.h"
#include "device/vr/android/arcore/address_to_id_map.h"
#include "device/vr/android/arcore/arcore_plane_manager.h"
#include "device/vr/android/arcore/arcore_sdk.h"
#include "device/vr/android/arcore/scoped_arcore_objects.h"
#include "device/vr/public/mojom/vr_service.mojom.h"

namespace device {

class ArCoreImpl;

using AnchorId = util::IdTypeU64<class AnchorTag>;

class ArCoreAnchorManager {
 public:
  ArCoreAnchorManager(base::PassKey<ArCoreImpl> pass_key,
                      ArSession* arcore_session);
  ~ArCoreAnchorManager();

  // Updates anchor manager state - it should be called in every frame if the
  // ARCore session supports anchors. Currently, all ARCore sessions support
  // anchors.
  void Update(ArFrame* ar_frame);

  mojom::XRAnchorsDataPtr GetAnchorsData() const;

  bool AnchorExists(AnchorId id) const;

  // Returns base::nullopt if anchor with the given id does not exist.
  base::Optional<gfx::Transform> GetMojoFromAnchor(AnchorId id) const;

  // Creates Anchor object given a plane ID.
  base::Optional<AnchorId> CreateAnchor(ArCorePlaneManager* plane_manager,
                                        const device::mojom::Pose& pose,
                                        PlaneId plane_id);

  // Creates free-floating Anchor.
  base::Optional<AnchorId> CreateAnchor(const device::mojom::Pose& pose);

  void DetachAnchor(AnchorId anchor_id);

 private:
  struct AnchorInfo {
    device::internal::ScopedArCoreObject<ArAnchor*> anchor;
    ArTrackingState tracking_state;

    AnchorInfo(device::internal::ScopedArCoreObject<ArAnchor*> anchor,
               ArTrackingState tracking_state);
    AnchorInfo(AnchorInfo&& other);
    ~AnchorInfo();
  };

  // Executes |fn| for each still tracked, anchor present in |arcore_anchors|.
  // |fn| will receive a `device::internal::ScopedArCoreObject<ArAnchor*>` that
  // can be stored, as well as ArTrackingState of the passed in anchor. An
  // anchor is tracked if its state is not AR_TRACKING_STATE_STOPPED.
  template <typename FunctionType>
  void ForEachArCoreAnchor(ArAnchorList* arcore_anchors, FunctionType fn);

  // Owned by ArCoreImpl - non-owning pointer is fine since ArCoreAnchorManager
  // is also owned by ArCoreImpl.
  ArSession* arcore_session_;

  // Allows reuse of the pose object; ARCore will populate it with new data on
  // each call to the ARCore SDK.
  internal::ScopedArCoreObject<ArPose*> ar_pose_;

  internal::ScopedArCoreObject<ArAnchorList*> arcore_anchors_;

  // Mapping from anchor address to anchor ID. It should be modified only during
  // calls to |Update()| and anchor creation.
  AddressToIdMap<AnchorId> anchor_address_to_id_;
  // Mapping from anchor ID to ARCore anchor information. It should be modified
  // only during calls to |Update()|.
  std::map<AnchorId, AnchorInfo> anchor_id_to_anchor_info_;
  // Set containing IDs of anchors updated in the last frame. It should be
  // modified only during calls to |Update()|.
  std::set<AnchorId> updated_anchor_ids_;
};

}  // namespace device

#endif  // DEVICE_VR_ANDROID_ARCORE_ARCORE_ANCHOR_MANAGER_H_
