// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_ARCORE_ARCORE_H_
#define DEVICE_VR_ANDROID_ARCORE_ARCORE_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/component_export.h"
#include "base/time/time.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/public/mojom/xr_session.mojom.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/transform.h"

namespace device {

// This allows a real or fake implementation of ArCore to
// be used as appropriate (i.e. for testing).
class COMPONENT_EXPORT(VR_ARCORE) ArCore {
 public:
  virtual ~ArCore() = default;

  // Represents an inclusive range from min to max. (This is different from
  // base::Interval which excludes the top end of the range, resulting in an
  // empty interval if min==max.)
  struct MinMaxRange {
    float min;
    float max;
  };

  struct InitializeResult {
    std::unordered_set<device::mojom::XRSessionFeature> enabled_features;

    // If the depth sensing API was requested, the depth_configuration will
    // contain the device-selected depth API usage and data format.

    std::optional<device::mojom::XRDepthConfig> depth_configuration;

    InitializeResult(
        const std::unordered_set<device::mojom::XRSessionFeature>&
            enabled_features,
        std::optional<device::mojom::XRDepthConfig> depth_configuration);
    InitializeResult(const InitializeResult& other);
    ~InitializeResult();
  };

  struct DepthSensingConfiguration {
    std::vector<device::mojom::XRDepthUsage> depth_usage_preference;
    std::vector<device::mojom::XRDepthDataFormat> depth_data_format_preference;

    DepthSensingConfiguration(
        std::vector<device::mojom::XRDepthUsage> depth_usage_preference,
        std::vector<device::mojom::XRDepthDataFormat>
            depth_data_format_preference);
    ~DepthSensingConfiguration();

    DepthSensingConfiguration(const DepthSensingConfiguration& other);
    DepthSensingConfiguration(DepthSensingConfiguration&& other);

    DepthSensingConfiguration& operator=(
        const DepthSensingConfiguration& other);
    DepthSensingConfiguration& operator=(DepthSensingConfiguration&& other);
  };

  // Initializes the runtime and returns whether it was successful.
  // If successful, the runtime must be paused when this method returns.
  virtual std::optional<InitializeResult> Initialize(
      base::android::ScopedJavaLocalRef<jobject> application_context,
      const std::unordered_set<device::mojom::XRSessionFeature>&
          required_features,
      const std::unordered_set<device::mojom::XRSessionFeature>&
          optional_features,
      const std::vector<device::mojom::XRTrackedImagePtr>& tracked_images,
      std::optional<DepthSensingConfiguration> depth_sensing_config) = 0;

  // Returns the target framerate range in Hz. Actual capture frame rate will
  // vary within this range, i.e. lower in low light to increase exposure time.
  virtual MinMaxRange GetTargetFramerateRange() = 0;

  virtual void SetDisplayGeometry(
      const gfx::Size& frame_size,
      display::Display::Rotation display_rotation) = 0;
  virtual void SetCameraTexture(uint32_t camera_texture_id) = 0;

  virtual gfx::Size GetUncroppedCameraImageSize() const = 0;

  gfx::Transform GetCameraUvFromScreenUvTransform() const;
  gfx::Transform GetDepthUvFromScreenUvTransform() const;

  virtual gfx::Transform GetProjectionMatrix(float near, float far) = 0;

  // Update ArCore state. This call blocks for up to 1/30s while waiting for a
  // new camera image. The output parameter |camera_updated| must be non-null,
  // the stored value indicates if the camera image was updated successfully.
  // The returned pose is nullptr if tracking was lost, this can happen even
  // when the camera image was updated successfully.
  virtual mojom::VRPosePtr Update(bool* camera_updated) = 0;

  // Camera image timestamp. This returns TimeDelta instead of TimeTicks since
  // ARCore internally uses an arbitrary and unspecified time base.
  virtual base::TimeDelta GetFrameTimestamp() = 0;

  // Return latest estimate for the floor height.
  virtual float GetEstimatedFloorHeight() = 0;

  // Returns information about all planes detected in the current frame.
  virtual mojom::XRPlaneDetectionDataPtr GetDetectedPlanesData() = 0;

  // Returns information about all anchors tracked in the current frame.
  virtual mojom::XRAnchorsDataPtr GetAnchorsData() = 0;

  // Returns information about lighting estimation.
  virtual mojom::XRLightEstimationDataPtr GetLightEstimationData() = 0;

  virtual mojom::XRDepthDataPtr GetDepthData() = 0;

  virtual mojom::XRTrackedImagesDataPtr GetTrackedImages() = 0;

  virtual bool RequestHitTest(
      const mojom::XRRayPtr& ray,
      std::vector<mojom::XRHitResultPtr>* hit_results) = 0;

  // Subscribes to hit test. Returns std::nullopt if subscription failed.
  // This variant will subscribe for a hit test to a specific native origin
  // specified in |native_origin_information|. The native origin will be used
  // along with passed in ray to compute the hit test results as of latest
  // frame. The passed in |entity_types| will be used to filter out the results
  // that do not match anything in the vector.
  virtual std::optional<uint64_t> SubscribeToHitTest(
      mojom::XRNativeOriginInformationPtr native_origin_information,
      const std::vector<mojom::EntityTypeForHitTest>& entity_types,
      mojom::XRRayPtr ray) = 0;
  // Subscribes to hit test for transient input sources. Returns std::nullopt
  // if subscription failed. This variant will subscribe for a hit test to
  // transient input sources that match the |profile_name|. The passed in ray
  // will be used to compute the hit test results as of latest frame (relative
  // to the location of transient input source). The passed in |entity_types|
  // will be used to filter out the results that do not match anything in the
  // vector.
  virtual std::optional<uint64_t> SubscribeToHitTestForTransientInput(
      const std::string& profile_name,
      const std::vector<mojom::EntityTypeForHitTest>& entity_types,
      mojom::XRRayPtr ray) = 0;

  virtual mojom::XRHitTestSubscriptionResultsDataPtr
  GetHitTestSubscriptionResults(
      const gfx::Transform& mojo_from_viewer,
      const std::vector<mojom::XRInputSourceStatePtr>& input_state) = 0;

  virtual void UnsubscribeFromHitTest(uint64_t subscription_id) = 0;

  using CreateAnchorCallback =
      base::OnceCallback<void(device::mojom::CreateAnchorResult,
                              uint64_t anchor_id)>;

  // Creates free-floating anchor. This call will be deferred and the actual
  // call may be postponed until ARCore is in correct state and the pose of
  // native origin is known. The anchor pose passed in
  // |native_origin_from_anchor| is expressed relative to a native origin passed
  // in |native_origin_information|. The native origin will only be used to
  // determine most up-to-date pose (i.e. it will *not* be used to create
  // anchors attached to planes even if the native origin information describes
  // a plane).
  virtual void CreateAnchor(
      const mojom::XRNativeOriginInformation& native_origin_information,
      const device::Pose& native_origin_from_anchor,
      CreateAnchorCallback callback) = 0;
  // Creates plane-attached anchor. This call will be deferred and the actual
  // call may be postponed until ARCore is in correct state and the pose of
  // the plane is known.
  virtual void CreatePlaneAttachedAnchor(
      const mojom::XRNativeOriginInformation& native_origin_information,
      const device::Pose& native_origin_from_anchor,
      uint64_t plane_id,
      CreateAnchorCallback callback) = 0;

  // Starts processing anchor creation requests created by calls to
  // |CreateAnchor()| & |CreatePlaneAttachedAnchor()| (see above). It should be
  // called when ARCore is in appropriate state. This method must be called on a
  // regular basis (once per ARCore update is sufficient), otherwise the anchor
  // creation requests may be deferred for longer than they need to.
  // sufficient here. |frame_time| should contain the timestamp of the currently
  // processed frame and will be used to determine whether some anchor creation
  // requests are outdated and should be failed.
  virtual void ProcessAnchorCreationRequests(
      const gfx::Transform& mojo_from_viewer,
      const std::vector<mojom::XRInputSourceStatePtr>& input_state,
      const base::TimeTicks& frame_time) = 0;

  virtual void DetachAnchor(uint64_t anchor_id) = 0;

  virtual void Pause() = 0;
  virtual void Resume() = 0;

 protected:
  virtual std::vector<float> TransformDisplayUvCoords(
      const base::span<const float> uvs) const = 0;
};

class ArCoreFactory {
 public:
  virtual ~ArCoreFactory() = default;
  virtual std::unique_ptr<ArCore> Create() = 0;
};

}  // namespace device

#endif  // DEVICE_VR_ANDROID_ARCORE_ARCORE_H_
