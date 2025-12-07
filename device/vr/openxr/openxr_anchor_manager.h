// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_ANCHOR_MANAGER_H_
#define DEVICE_VR_OPENXR_OPENXR_ANCHOR_MANAGER_H_

#include <optional>
#include <vector>

#include "base/types/expected.h"
#include "device/vr/create_anchor_request.h"
#include "device/vr/public/mojom/anchor_id.h"
#include "device/vr/public/mojom/plane_id.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace gfx {
class Transform;
}

namespace device {

class OpenXrApiWrapper;
struct XrLocation;

class OpenXrAnchorManager {
 public:
  OpenXrAnchorManager();
  virtual ~OpenXrAnchorManager();

  OpenXrAnchorManager(const OpenXrAnchorManager&) = delete;
  OpenXrAnchorManager& operator=(const OpenXrAnchorManager&) = delete;

  void AddCreateAnchorRequest(
      const mojom::XRNativeOriginInformation& native_origin_information,
      const device::Pose& native_origin_from_anchor,
      const std::optional<PlaneId>& plane_id,
      CreateAnchorCallback callback);

  device::mojom::XRAnchorsDataPtr ProcessAnchorsForFrame(
      OpenXrApiWrapper* openxr,
      XrTime predicted_display_time);

  virtual void DetachAnchor(AnchorId anchor_id) = 0;

  // Used to get the space and pose of the object (like a new anchor or layer)
  // given it's intended offset from the provided anchor_id. On some platforms
  // this is just an XrLocation of the XrSpace representing the Anchor and the
  // provided pose; but on others Anchors don't have their own XrSpace so the
  // pose needs to be translated to a common XrSpace. This will then be passed
  // in to create the anchor.
  virtual std::optional<XrLocation> GetXrLocationFromAnchor(
      AnchorId anchor_id,
      const gfx::Transform& anchor_id_from_object) const = 0;

 protected:
  enum class AnchorTrackingErrorType {
    kTemporary = 0,
    kPermanent = 1,
  };


  // Create a new Anchor at |pose| in |space| at |predicted_display_time|. Can
  // return an Invalid AnchorId on failure.
  // If present, will attempt to parent the anchor to the specified |plane_id|.
  virtual AnchorId CreateAnchor(XrPosef pose,
                                XrSpace space,
                                XrTime predicted_display_time,
                                std::optional<PlaneId> plane_id) = 0;

  virtual mojom::XRAnchorsDataPtr GetCurrentAnchorsData(
      XrTime predicted_display_time) = 0;

 private:
  void DisposeActiveAnchorCallbacks();
  void ProcessCreateAnchorRequests(OpenXrApiWrapper* openxr);
  bool IsSupportedOrigin(
      const mojom::XRNativeOriginInformation& native_origin_info) const;

  std::vector<CreateAnchorRequest> create_anchor_requests_;
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_ANCHOR_MANAGER_H_
