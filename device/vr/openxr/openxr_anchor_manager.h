// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_ANCHOR_MANAGER_H_
#define DEVICE_VR_OPENXR_OPENXR_ANCHOR_MANAGER_H_

#include <map>
#include <optional>

#include "base/types/expected.h"
#include "base/types/id_type.h"
#include "device/vr/create_anchor_request.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

class OpenXrApiWrapper;

using AnchorId = base::IdTypeU64<class AnchorTag>;
constexpr AnchorId kInvalidAnchorId;

class OpenXrAnchorManager {
 public:
  OpenXrAnchorManager();
  virtual ~OpenXrAnchorManager();

  OpenXrAnchorManager(const OpenXrAnchorManager&) = delete;
  OpenXrAnchorManager& operator=(const OpenXrAnchorManager&) = delete;


  void AddCreateAnchorRequest(
      const mojom::XRNativeOriginInformation& native_origin_information,
      const device::Pose& native_origin_from_anchor,
      CreateAnchorCallback callback);

  device::mojom::XRAnchorsDataPtr ProcessAnchorsForFrame(
      OpenXrApiWrapper* openxr,
      const mojom::VRStageParametersPtr& current_stage_parameters,
      const std::vector<mojom::XRInputSourceStatePtr>& input_state,
      XrTime predicted_display_time);

  void DetachAnchor(AnchorId anchor_id);

 protected:
  enum class AnchorTrackingErrorType {
    kTemporary = 0,
    kPermanent = 1,
  };

  // Called to create an anchor. `pose` in `space` at the
  // `predicted_display_time` should be the origin of the returned anchor's
  // space, and the space should adjust as necessary to keep that origin
  // aligned.
  virtual XrSpace CreateAnchor(XrPosef pose,
                               XrSpace space,
                               XrTime predicted_display_time) = 0;

  // Called when an anchor is detached, right before the corresponding space is
  // destroyed. This can be used by the subclass to clean up any additional
  // state that it may have stored. Note that this will not/cannoy be called in
  // the destructor, so any cleanup that needs to be done should also happen
  // in the subclass destructor.
  virtual void OnDetachAnchor(const XrSpace& anchor) = 0;

  virtual base::expected<device::Pose, AnchorTrackingErrorType>
  GetAnchorFromMojom(XrSpace anchor_space,
                     XrTime predicted_display_time) const = 0;

 private:
  void DisposeActiveAnchorCallbacks();
  XrSpace GetAnchorSpace(AnchorId anchor_id) const;
  void ProcessCreateAnchorRequests(
      OpenXrApiWrapper* openxr,
      const mojom::VRStageParametersPtr& current_stage_parameters,
      const std::vector<mojom::XRInputSourceStatePtr>& input_state);
  device::mojom::XRAnchorsDataPtr GetCurrentAnchorsData(
      XrTime predicted_display_time);

  // An XrPosef with the space it is relative to
  struct XrLocation {
    XrPosef pose;
    XrSpace space;
  };
  std::optional<XrLocation> GetXrLocationFromNativeOriginInformation(
      OpenXrApiWrapper* openxr,
      const mojom::VRStageParametersPtr& current_stage_parametersm,
      const mojom::XRNativeOriginInformation& native_origin_information,
      const gfx::Transform& native_origin_from_anchor,
      const std::vector<mojom::XRInputSourceStatePtr>& input_state) const;

  std::optional<XrLocation> GetXrLocationFromReferenceSpace(
      OpenXrApiWrapper* openxr,
      const mojom::VRStageParametersPtr& current_stage_parameters,
      const mojom::XRNativeOriginInformation& native_origin_information,
      const gfx::Transform& native_origin_from_anchor) const;

  std::vector<CreateAnchorRequest> create_anchor_requests_;

  AnchorId::Generator anchor_id_generator_;  // 0 is not a valid anchor ID
  std::map<AnchorId, XrSpace> openxr_anchors_;
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_ANCHOR_MANAGER_H_
