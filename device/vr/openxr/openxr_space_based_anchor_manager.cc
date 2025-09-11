// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_space_based_anchor_manager.h"

#include <utility>
#include <vector>

#include "base/logging.h"
#include "device/vr/openxr/openxr_util.h"
#include "device/vr/public/mojom/pose.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "ui/gfx/geometry/transform.h"

namespace device {

OpenXrSpaceBasedAnchorManager::OpenXrSpaceBasedAnchorManager() = default;
OpenXrSpaceBasedAnchorManager::~OpenXrSpaceBasedAnchorManager() {
  for (const auto& it : openxr_anchors_) {
    xrDestroySpace(it.second);
  }
}

AnchorId OpenXrSpaceBasedAnchorManager::CreateAnchor(
    XrPosef pose,
    XrSpace space,
    XrTime predicted_display_time) {
  XrSpace anchor_space =
      CreateAnchorInternal(pose, space, predicted_display_time);
  if (anchor_space == XR_NULL_HANDLE) {
    return kInvalidAnchorId;
  }

  AnchorId anchor_id = anchor_id_generator_.GenerateNextId();
  CHECK(anchor_id);
  openxr_anchors_.insert({anchor_id, anchor_space});
  return anchor_id;
}

AnchorId OpenXrSpaceBasedAnchorManager::CreatePlaneAnchor(
    PlaneId plane_id,
    XrPosef pose,
    XrTime predicted_display_time) {
  // None of the corresponding infrastructure supports plane detection at this
  // moment, so realistically this shouldn't be called.
  return kInvalidAnchorId;
}

void OpenXrSpaceBasedAnchorManager::DetachAnchor(AnchorId anchor_id) {
  DCHECK(anchor_id);
  auto it = openxr_anchors_.find(anchor_id);
  if (it == openxr_anchors_.end()) {
    return;
  }

  XrSpace anchor_space = it->second;
  OnDetachAnchor(anchor_space);
  xrDestroySpace(anchor_space);
  openxr_anchors_.erase(it);
}

std::optional<OpenXrAnchorManager::XrLocation>
OpenXrSpaceBasedAnchorManager::GetXrLocationFromAnchor(
    AnchorId anchor_id,
    const gfx::Transform& anchor_id_from_new_anchor) const {
  return XrLocation{GfxTransformToXrPose(anchor_id_from_new_anchor),
                    GetAnchorSpace(anchor_id)};
}

mojom::XRAnchorsDataPtr OpenXrSpaceBasedAnchorManager::GetCurrentAnchorsData(
    XrTime predicted_display_time) {
  std::vector<uint64_t> all_anchors_ids;
  all_anchors_ids.reserve(openxr_anchors_.size());
  std::vector<mojom::XRAnchorDataPtr> updated_anchors;
  updated_anchors.reserve(openxr_anchors_.size());
  absl::flat_hash_set<AnchorId> deleted_ids;

  for (const auto& [anchor_id, anchor_space] : openxr_anchors_) {
    all_anchors_ids.push_back(anchor_id.GetUnsafeValue());
    auto maybe_pose = GetAnchorFromMojom(anchor_space, predicted_display_time);
    if (maybe_pose.has_value()) {
      updated_anchors.push_back(mojom::XRAnchorData::New(
          anchor_id.GetUnsafeValue(), maybe_pose.value()));
    } else {
      // Regardless of why it failed, if we still have it tracked, send it up
      // this frame, but remove it for future frames.
      updated_anchors.push_back(
          mojom::XRAnchorData::New(anchor_id.GetUnsafeValue(), std::nullopt));
      if (maybe_pose.error() == AnchorTrackingErrorType::kPermanent) {
        deleted_ids.insert(anchor_id);
      }
    }
  }

  for (const auto& id : deleted_ids) {
    DetachAnchor(id);
  }

  DVLOG(3) << __func__ << " all_anchor_ids size: " << all_anchors_ids.size();
  return mojom::XRAnchorsData::New(std::move(all_anchors_ids),
                                   std::move(updated_anchors));
}

XrSpace OpenXrSpaceBasedAnchorManager::GetAnchorSpace(
    AnchorId anchor_id) const {
  DCHECK(anchor_id);
  auto it = openxr_anchors_.find(anchor_id);
  if (it == openxr_anchors_.end()) {
    return XR_NULL_HANDLE;
  }
  return it->second;
}

}  // namespace device
