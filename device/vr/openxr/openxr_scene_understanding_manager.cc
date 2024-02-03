// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_scene_understanding_manager.h"

#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "device/vr/openxr/openxr_extension_helper.h"
#include "device/vr/openxr/openxr_util.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

OpenXRSceneUnderstandingManager::OpenXRSceneUnderstandingManager() = default;
OpenXRSceneUnderstandingManager::~OpenXRSceneUnderstandingManager() = default;

std::optional<HitTestSubscriptionId>
OpenXRSceneUnderstandingManager::SubscribeToHitTest(
    mojom::XRNativeOriginInformationPtr native_origin_information,
    const std::vector<mojom::EntityTypeForHitTest>& entity_types,
    mojom::XRRayPtr ray) {
  if (!OnNewHitTestSubscription()) {
    return std::nullopt;
  }

  auto subscription_id = hittest_id_generator_.GenerateNextId();

  hit_test_subscription_id_to_data_.emplace(
      subscription_id,
      HitTestSubscriptionData{std::move(native_origin_information),
                              entity_types, std::move(ray)});

  return subscription_id;
}

std::optional<HitTestSubscriptionId>
OpenXRSceneUnderstandingManager::SubscribeToHitTestForTransientInput(
    const std::string& profile_name,
    const std::vector<mojom::EntityTypeForHitTest>& entity_types,
    mojom::XRRayPtr ray) {
  if (!OnNewHitTestSubscription()) {
    return std::nullopt;
  }

  auto subscription_id = hittest_id_generator_.GenerateNextId();

  hit_test_subscription_id_to_transient_hit_test_data_.emplace(
      subscription_id, TransientInputHitTestSubscriptionData{
                           profile_name, entity_types, std::move(ray)});

  return subscription_id;
}

device::mojom::XRHitTestSubscriptionResultDataPtr
OpenXRSceneUnderstandingManager::GetHitTestSubscriptionResult(
    HitTestSubscriptionId id,
    const mojom::XRRay& native_origin_ray,
    const gfx::Transform& mojo_from_native_origin) {
  DVLOG(3) << __func__ << ": id=" << id;

  // Transform the ray according to the latest transform based on the XRSpace
  // used in hit test subscription.

  gfx::Point3F origin =
      mojo_from_native_origin.MapPoint(native_origin_ray.origin);

  gfx::Vector3dF direction =
      mojo_from_native_origin.MapVector(native_origin_ray.direction);

  return mojom::XRHitTestSubscriptionResultData::New(
      id.GetUnsafeValue(), RequestHitTest(origin, direction));
}

device::mojom::XRHitTestTransientInputSubscriptionResultDataPtr
OpenXRSceneUnderstandingManager::GetTransientHitTestSubscriptionResult(
    HitTestSubscriptionId id,
    const mojom::XRRay& input_source_ray,
    const std::vector<std::pair<uint32_t, gfx::Transform>>&
        input_source_ids_and_mojo_from_input_sources) {
  auto result =
      device::mojom::XRHitTestTransientInputSubscriptionResultData::New();

  result->subscription_id = id.GetUnsafeValue();

  for (const auto& input_source_id_and_mojo_from_input_source :
       input_source_ids_and_mojo_from_input_sources) {
    gfx::Point3F origin =
        input_source_id_and_mojo_from_input_source.second.MapPoint(
            input_source_ray.origin);

    gfx::Vector3dF direction =
        input_source_id_and_mojo_from_input_source.second.MapVector(
            input_source_ray.direction);

    result->input_source_id_to_hit_test_results.insert(
        {input_source_id_and_mojo_from_input_source.first,
         RequestHitTest(origin, direction)});
  }

  return result;
}

mojom::XRHitTestSubscriptionResultsDataPtr
OpenXRSceneUnderstandingManager::GetHitTestResults(
    const gfx::Transform& mojo_from_viewer,
    const std::vector<mojom::XRInputSourceStatePtr>& input_state) {
  mojom::XRHitTestSubscriptionResultsDataPtr result =
      mojom::XRHitTestSubscriptionResultsData::New();

  DVLOG(3) << __func__
           << ": calculating hit test subscription results, "
              "hit_test_subscription_id_to_data_.size()="
           << hit_test_subscription_id_to_data_.size();

  for (auto& subscription_id_and_data : hit_test_subscription_id_to_data_) {
    // First, check if we can find the current transformation for a ray. If not,
    // skip processing this subscription.
    auto maybe_mojo_from_native_origin = GetMojoFromNativeOrigin(
        *subscription_id_and_data.second.native_origin_information,
        mojo_from_viewer, input_state);

    if (!maybe_mojo_from_native_origin) {
      continue;
    }

    // Since we have a transform, let's use it to obtain hit test results.
    result->results.push_back(GetHitTestSubscriptionResult(
        HitTestSubscriptionId(subscription_id_and_data.first),
        *subscription_id_and_data.second.ray, *maybe_mojo_from_native_origin));
  }

  // Calculate results for transient input sources
  DVLOG(3)
      << __func__
      << ": calculating hit test subscription results for transient input, "
         "hit_test_subscription_id_to_transient_hit_test_data_.size()="
      << hit_test_subscription_id_to_transient_hit_test_data_.size();

  for (const auto& subscription_id_and_data :
       hit_test_subscription_id_to_transient_hit_test_data_) {
    auto input_source_ids_and_transforms = GetMojoFromInputSources(
        subscription_id_and_data.second.profile_name, input_state);

    result->transient_input_results.push_back(
        GetTransientHitTestSubscriptionResult(
            HitTestSubscriptionId(subscription_id_and_data.first),
            *subscription_id_and_data.second.ray,
            input_source_ids_and_transforms));
  }

  return result;
}

void OpenXRSceneUnderstandingManager::UnsubscribeFromHitTest(
    HitTestSubscriptionId subscription_id) {
  // Hit test subscription ID space is the same for transient and non-transient
  // hit test sources, so we can attempt to remove it from both collections (it
  // will succeed only for one of them anyway).
  hit_test_subscription_id_to_data_.erase(
      HitTestSubscriptionId(subscription_id));
  hit_test_subscription_id_to_transient_hit_test_data_.erase(
      HitTestSubscriptionId(subscription_id));
  if (hit_test_subscription_id_to_data_.empty() &&
      hit_test_subscription_id_to_transient_hit_test_data_.empty()) {
    OnAllHitTestSubscriptionsRemoved();
  }
}

std::optional<gfx::Transform>
OpenXRSceneUnderstandingManager::GetMojoFromNativeOrigin(
    const mojom::XRNativeOriginInformation& native_origin_information,
    const gfx::Transform& mojo_from_viewer,
    const std::vector<mojom::XRInputSourceStatePtr>& input_state) {
  switch (native_origin_information.which()) {
    case mojom::XRNativeOriginInformation::Tag::kInputSourceSpaceInfo:
      for (auto& input_source_state : input_state) {
        mojom::XRInputSourceSpaceInfo* input_source_space_info =
            native_origin_information.get_input_source_space_info().get();
        if (input_source_state->source_id ==
            input_source_space_info->input_source_id) {
          return GetMojoFromPointerInput(input_source_state);
        }
      }
      return std::nullopt;
    case mojom::XRNativeOriginInformation::Tag::kReferenceSpaceType:
      return GetMojoFromReferenceSpace(
          native_origin_information.get_reference_space_type(),
          mojo_from_viewer);
    case mojom::XRNativeOriginInformation::Tag::kPlaneId:
      return std::nullopt;
    case mojom::XRNativeOriginInformation::Tag::kAnchorId:
      return std::nullopt;
    case mojom::XRNativeOriginInformation::Tag::kHandJointSpaceInfo:
      return std::nullopt;
    case mojom::XRNativeOriginInformation::Tag::kImageIndex:
      return std::nullopt;
  }
}

std::optional<gfx::Transform>
OpenXRSceneUnderstandingManager::GetMojoFromReferenceSpace(
    device::mojom::XRReferenceSpaceType type,
    const gfx::Transform& mojo_from_viewer) {
  DVLOG(3) << __func__ << ": type=" << type;

  switch (type) {
    case device::mojom::XRReferenceSpaceType::kLocal:
      return gfx::Transform{};
    case device::mojom::XRReferenceSpaceType::kLocalFloor:
      return std::nullopt;
    case device::mojom::XRReferenceSpaceType::kViewer:
      return mojo_from_viewer;
    case device::mojom::XRReferenceSpaceType::kBoundedFloor:
      return std::nullopt;
    case device::mojom::XRReferenceSpaceType::kUnbounded:
      return std::nullopt;
  }
}

std::vector<std::pair<uint32_t, gfx::Transform>>
OpenXRSceneUnderstandingManager::GetMojoFromInputSources(
    const std::string& profile_name,
    const std::vector<mojom::XRInputSourceStatePtr>& input_state) {
  std::vector<std::pair<uint32_t, gfx::Transform>> result;

  for (const auto& input_source_state : input_state) {
    if (input_source_state && input_source_state->description) {
      if (base::Contains(input_source_state->description->profiles,
                         profile_name)) {
        // Input source represented by input_state matches the profile, find
        // the transform and grab input source id.
        std::optional<gfx::Transform> maybe_mojo_from_input_source =
            GetMojoFromPointerInput(input_source_state);

        if (!maybe_mojo_from_input_source)
          continue;

        result.push_back(
            {input_source_state->source_id, *maybe_mojo_from_input_source});
      }
    }
  }

  return result;
}

std::optional<gfx::Transform>
OpenXRSceneUnderstandingManager::GetMojoFromPointerInput(
    const device::mojom::XRInputSourceStatePtr& input_source_state) {
  if (!input_source_state->mojo_from_input ||
      !input_source_state->description ||
      !input_source_state->description->input_from_pointer) {
    return std::nullopt;
  }

  gfx::Transform mojo_from_input = *input_source_state->mojo_from_input;

  gfx::Transform input_from_pointer =
      *input_source_state->description->input_from_pointer;

  return mojo_from_input * input_from_pointer;
}

}  // namespace device
