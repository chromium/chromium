// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/arcore/arcore_anchor_manager.h"

#include "base/containers/contains.h"
#include "device/vr/android/arcore/vr_service_type_converters.h"

namespace device {

ArCoreAnchorManager::ArCoreAnchorManager(base::PassKey<ArCoreImpl> pass_key,
                                         ArSession* arcore_session)
    : arcore_session_(arcore_session) {
  DCHECK(arcore_session_);

  ArAnchorList_create(
      arcore_session_,
      internal::ScopedArCoreObject<ArAnchorList*>::Receiver(arcore_anchors_)
          .get());
  DCHECK(arcore_anchors_.is_valid());

  ArPose_create(
      arcore_session_, nullptr,
      internal::ScopedArCoreObject<ArPose*>::Receiver(ar_pose_).get());
  DCHECK(ar_pose_.is_valid());
}

ArCoreAnchorManager::~ArCoreAnchorManager() = default;

mojom::XRAnchorsDataPtr ArCoreAnchorManager::GetAnchorsData() const {
  DVLOG(3) << __func__ << ": anchor_id_to_anchor_info_.size()="
           << anchor_id_to_anchor_info_.size()
           << ", updated_anchor_ids_.size()=" << updated_anchor_ids_.size();

#if DCHECK_IS_ON()
  DCHECK(was_anchor_data_retrieved_in_current_frame_)
      << "Update() must not be called twice in a row without a call to "
         "GetAnchorsData() in between";
  was_anchor_data_retrieved_in_current_frame_ = false;
#endif

  std::vector<uint64_t> all_anchors_ids;
  all_anchors_ids.reserve(anchor_id_to_anchor_info_.size());
  for (const auto& anchor_id_and_object : anchor_id_to_anchor_info_) {
    all_anchors_ids.push_back(anchor_id_and_object.first.GetUnsafeValue());
  }

  std::vector<mojom::XRAnchorDataPtr> updated_anchors;
  updated_anchors.reserve(updated_anchor_ids_.size());
  for (const auto& anchor_id : updated_anchor_ids_) {
    const device::internal::ScopedArCoreObject<ArAnchor*>& anchor =
        anchor_id_to_anchor_info_.at(anchor_id).anchor;

    if (anchor_id_to_anchor_info_.at(anchor_id).tracking_state ==
        AR_TRACKING_STATE_TRACKING) {
      // pose
      ArAnchor_getPose(arcore_session_, anchor.get(), ar_pose_.get());
      device::Pose pose = GetPoseFromArPose(arcore_session_, ar_pose_.get());

      DVLOG(3) << __func__ << ": anchor_id: " << anchor_id.GetUnsafeValue()
               << ", position=" << pose.position().ToString()
               << ", orientation=" << pose.orientation().ToString();

      updated_anchors.push_back(
          mojom::XRAnchorData::New(anchor_id.GetUnsafeValue(), pose));
    } else {
      DVLOG(3) << __func__ << ": anchor_id: " << anchor_id.GetUnsafeValue()
               << ", position=untracked, orientation=untracked";

      updated_anchors.push_back(
          mojom::XRAnchorData::New(anchor_id.GetUnsafeValue(), std::nullopt));
    }
  }

#if DCHECK_IS_ON()
  was_anchor_data_retrieved_in_current_frame_ = true;
#endif

  return mojom::XRAnchorsData::New(std::move(all_anchors_ids),
                                   std::move(updated_anchors));
}

template <typename FunctionType>
void ArCoreAnchorManager::ForEachArCoreAnchor(ArAnchorList* arcore_anchors,
                                              FunctionType fn) {
  DCHECK(arcore_anchors);

  int32_t anchor_list_size;
  ArAnchorList_getSize(arcore_session_, arcore_anchors, &anchor_list_size);

  for (int i = 0; i < anchor_list_size; i++) {
    internal::ScopedArCoreObject<ArAnchor*> anchor;
    ArAnchorList_acquireItem(
        arcore_session_, arcore_anchors, i,
        internal::ScopedArCoreObject<ArAnchor*>::Receiver(anchor).get());

    ArTrackingState tracking_state;
    ArAnchor_getTrackingState(arcore_session_, anchor.get(), &tracking_state);

    if (tracking_state == ArTrackingState::AR_TRACKING_STATE_STOPPED) {
      // Skip all anchors that are not currently tracked.
      continue;
    }

    fn(std::move(anchor), tracking_state);
  }
}

void ArCoreAnchorManager::Update(ArFrame* ar_frame) {
  // First, ask ARCore about all Anchors updated in the current frame.
  ArFrame_getUpdatedAnchors(arcore_session_, ar_frame, arcore_anchors_.get());

  // Collect the IDs of updated anchors.
  std::set<AnchorId> updated_anchor_ids;
  ForEachArCoreAnchor(arcore_anchors_.get(), [this, &updated_anchor_ids](
                                                 device::internal::
                                                     ScopedArCoreObject<
                                                         ArAnchor*> ar_anchor,
                                                 ArTrackingState
                                                     tracking_state) {
    auto result = anchor_address_to_id_.CreateOrGetId(ar_anchor.get());

    DVLOG(3) << __func__
             << ": anchor updated, anchor_id=" << result.id.GetUnsafeValue()
             << ", tracking_state=" << tracking_state;

    DCHECK(!result.created)
        << "Anchor creation is app-initiated - we should never encounter an "
           "anchor that was created outside of `ArCoreImpl::CreateAnchor()`.";

    updated_anchor_ids.insert(result.id);
  });

  DVLOG(3) << __func__
           << ": updated_anchor_ids.size()=" << updated_anchor_ids.size();

  // Then, ask about all Anchors that are still tracked.
  ArSession_getAllAnchors(arcore_session_, arcore_anchors_.get());

  // Collect the objects of all currently tracked anchors.
  // |anchor_address_to_id_| should *not* grow.
  std::map<AnchorId, AnchorInfo> new_anchor_id_to_anchor_info;
  ForEachArCoreAnchor(arcore_anchors_.get(), [this,
                                              &new_anchor_id_to_anchor_info,
                                              &updated_anchor_ids](
                                                 device::internal::
                                                     ScopedArCoreObject<
                                                         ArAnchor*> anchor,
                                                 ArTrackingState
                                                     tracking_state) {
    // ID
    auto result = anchor_address_to_id_.CreateOrGetId(anchor.get());

    DVLOG(3) << __func__
             << ": anchor present, anchor_id=" << result.id.GetUnsafeValue()
             << ", tracking state=" << tracking_state;

    DCHECK(!result.created)
        << "Anchor creation is app-initiated - we should never encounter an "
           "anchor that was created outside of `ArCoreImpl::CreateAnchor()`.";

    // Inspect the tracking state of this anchor in the previous frame. If it
    // changed, mark the anchor as updated.
    if (base::Contains(anchor_id_to_anchor_info_, result.id) &&
        anchor_id_to_anchor_info_.at(result.id).tracking_state !=
            tracking_state) {
      updated_anchor_ids.insert(result.id);
    }

    AnchorInfo new_anchor_info = AnchorInfo(std::move(anchor), tracking_state);

    new_anchor_id_to_anchor_info.emplace(result.id, std::move(new_anchor_info));
  });

  DVLOG(3) << __func__ << ": new_anchor_id_to_anchor_info.size()="
           << new_anchor_id_to_anchor_info.size();

  // Shrink |anchor_address_to_id_|, removing all anchors that are no longer
  // tracked - if they do not show up in |anchor_id_to_anchor_info| map, they
  // are no longer tracked.
  anchor_address_to_id_.EraseIf(
      [&new_anchor_id_to_anchor_info](const auto& anchor_address_and_id) {
        return !base::Contains(new_anchor_id_to_anchor_info,
                               anchor_address_and_id.second);
      });
  anchor_id_to_anchor_info_.swap(new_anchor_id_to_anchor_info);
  updated_anchor_ids_.swap(updated_anchor_ids);
}

std::optional<AnchorId> ArCoreAnchorManager::CreateAnchor(
    const device::mojom::Pose& pose) {
  auto ar_pose = GetArPoseFromMojomPose(arcore_session_, pose);

  device::internal::ScopedArCoreObject<ArAnchor*> ar_anchor;
  ArStatus status = ArSession_acquireNewAnchor(
      arcore_session_, ar_pose.get(),
      device::internal::ScopedArCoreObject<ArAnchor*>::Receiver(ar_anchor)
          .get());

  if (status != AR_SUCCESS) {
    return std::nullopt;
  }

  auto result = anchor_address_to_id_.CreateOrGetId(ar_anchor.get());

  DCHECK(result.created) << "This should always be a new anchor, not something "
                            "we've seen previously.";

  // Mark new anchor as updated to ensure we send its information over to blink:
  updated_anchor_ids_.insert(result.id);

  ArTrackingState tracking_state;
  ArAnchor_getTrackingState(arcore_session_, ar_anchor.get(), &tracking_state);

  anchor_id_to_anchor_info_.emplace(
      result.id, AnchorInfo(std::move(ar_anchor), tracking_state));

  return result.id;
}

std::optional<AnchorId> ArCoreAnchorManager::CreateAnchor(
    ArCorePlaneManager* plane_manager,
    const device::mojom::Pose& pose,
    PlaneId plane_id) {
  DCHECK(plane_manager);

  DVLOG(2) << __func__ << ": plane_id=" << plane_id;

  auto ar_anchor = plane_manager->CreateAnchor(
      base::PassKey<ArCoreAnchorManager>(), plane_id, pose);
  if (!ar_anchor.is_valid()) {
    return std::nullopt;
  }

  auto result = anchor_address_to_id_.CreateOrGetId(ar_anchor.get());

  DCHECK(result.created) << "This should always be a new anchor, not something "
                            "we've seen previously.";

  // Mark new anchor as updated to ensure we send its information over to blink:
  updated_anchor_ids_.insert(result.id);

  ArTrackingState tracking_state;
  ArAnchor_getTrackingState(arcore_session_, ar_anchor.get(), &tracking_state);

  anchor_id_to_anchor_info_.emplace(
      result.id, AnchorInfo(std::move(ar_anchor), tracking_state));

  return result.id;
}

void ArCoreAnchorManager::DetachAnchor(AnchorId anchor_id) {
  auto it = anchor_id_to_anchor_info_.find(anchor_id);
  if (it == anchor_id_to_anchor_info_.end()) {
    return;
  }

  ArAnchor_detach(arcore_session_, it->second.anchor.get());

  anchor_id_to_anchor_info_.erase(it);
}

bool ArCoreAnchorManager::AnchorExists(AnchorId id) const {
  return base::Contains(anchor_id_to_anchor_info_, id);
}

std::optional<gfx::Transform> ArCoreAnchorManager::GetMojoFromAnchor(
    AnchorId id) const {
  auto it = anchor_id_to_anchor_info_.find(id);
  if (it == anchor_id_to_anchor_info_.end()) {
    return std::nullopt;
  }

  ArAnchor_getPose(arcore_session_, it->second.anchor.get(), ar_pose_.get());
  device::Pose pose = GetPoseFromArPose(arcore_session_, ar_pose_.get());

  return pose.ToTransform();
}

ArCoreAnchorManager::AnchorInfo::AnchorInfo(
    device::internal::ScopedArCoreObject<ArAnchor*> anchor,
    ArTrackingState tracking_state)
    : anchor(std::move(anchor)), tracking_state(tracking_state) {}

ArCoreAnchorManager::AnchorInfo::AnchorInfo(AnchorInfo&& other) = default;
ArCoreAnchorManager::AnchorInfo::~AnchorInfo() = default;

}  // namespace device
