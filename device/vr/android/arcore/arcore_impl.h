// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_ARCORE_ARCORE_IMPL_H_
#define DEVICE_VR_ANDROID_ARCORE_ARCORE_IMPL_H_

#include <optional>

#include "base/component_export.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/types/id_type.h"
#include "device/vr/android/arcore/arcore.h"
#include "device/vr/android/arcore/arcore_anchor_manager.h"
#include "device/vr/android/arcore/arcore_plane_manager.h"
#include "device/vr/android/arcore/arcore_sdk.h"
#include "device/vr/android/arcore/scoped_arcore_objects.h"
#include "device/vr/create_anchor_request.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/public/mojom/xr_session.mojom.h"
#include "device/vr/util/hit_test_subscription_data.h"

namespace device {

class ArCorePlaneManager;

using AnchorId = base::IdTypeU64<class AnchorTag>;

class CreatePlaneAttachedAnchorRequest {
 public:
  uint64_t GetPlaneId() const;
  const mojom::XRNativeOriginInformation& GetNativeOriginInformation() const;
  gfx::Transform GetNativeOriginFromAnchor() const;
  base::TimeTicks GetRequestStartTime() const;

  CreateAnchorCallback TakeCallback();

  CreatePlaneAttachedAnchorRequest(
      const mojom::XRNativeOriginInformation& native_origin_information,
      const gfx::Transform& native_origin_from_anchor,
      uint64_t plane_id,
      CreateAnchorCallback callback);
  CreatePlaneAttachedAnchorRequest(CreatePlaneAttachedAnchorRequest&& other);
  ~CreatePlaneAttachedAnchorRequest();

 private:
  mojom::XRNativeOriginInformationPtr native_origin_information_;
  const gfx::Transform native_origin_from_anchor_;
  const uint64_t plane_id_;
  const base::TimeTicks request_start_time_;

  CreateAnchorCallback callback_;
};

// This class should be created and accessed entirely on a Gl thread.
class ArCoreImpl : public ArCore {
 public:
  ArCoreImpl();

  ArCoreImpl(const ArCoreImpl&) = delete;
  ArCoreImpl& operator=(const ArCoreImpl&) = delete;

  ~ArCoreImpl() override;

  std::optional<ArCore::InitializeResult> Initialize(
      base::android::ScopedJavaLocalRef<jobject> application_context,
      const std::unordered_set<device::mojom::XRSessionFeature>&
          required_features,
      const std::unordered_set<device::mojom::XRSessionFeature>&
          optional_features,
      const std::vector<device::mojom::XRTrackedImagePtr>& tracked_images,
      std::optional<ArCore::DepthSensingConfiguration> depth_sensing_config)
      override;
  MinMaxRange GetTargetFramerateRange() override;
  void SetDisplayGeometry(const gfx::Size& frame_size,
                          display::Display::Rotation display_rotation) override;
  void SetCameraTexture(uint32_t camera_texture_id) override;
  gfx::Transform GetProjectionMatrix(float near, float far) override;
  mojom::VRPosePtr Update(bool* camera_updated) override;
  base::TimeDelta GetFrameTimestamp() override;

  mojom::XRPlaneDetectionDataPtr GetDetectedPlanesData() override;

  mojom::XRAnchorsDataPtr GetAnchorsData() override;

  mojom::XRLightEstimationDataPtr GetLightEstimationData() override;

  void Pause() override;
  void Resume() override;

  float GetEstimatedFloorHeight() override;

  bool RequestHitTest(const mojom::XRRayPtr& ray,
                      std::vector<mojom::XRHitResultPtr>* hit_results) override;

  // Helper.
  bool RequestHitTest(
      const gfx::Point3F& origin,
      const gfx::Vector3dF& direction,
      const std::vector<mojom::EntityTypeForHitTest>& entity_types,
      std::vector<mojom::XRHitResultPtr>* hit_results);

  std::optional<uint64_t> SubscribeToHitTest(
      mojom::XRNativeOriginInformationPtr nativeOriginInformation,
      const std::vector<mojom::EntityTypeForHitTest>& entity_types,
      mojom::XRRayPtr ray) override;
  std::optional<uint64_t> SubscribeToHitTestForTransientInput(
      const std::string& profile_name,
      const std::vector<mojom::EntityTypeForHitTest>& entity_types,
      mojom::XRRayPtr ray) override;

  mojom::XRHitTestSubscriptionResultsDataPtr GetHitTestSubscriptionResults(
      const gfx::Transform& mojo_from_viewer,
      const std::vector<mojom::XRInputSourceStatePtr>& input_state) override;

  void UnsubscribeFromHitTest(uint64_t subscription_id) override;

  void CreateAnchor(
      const mojom::XRNativeOriginInformation& native_origin_information,
      const device::Pose& native_origin_from_anchor,
      CreateAnchorCallback callback) override;
  void CreatePlaneAttachedAnchor(
      const mojom::XRNativeOriginInformation& native_origin_information,
      const device::Pose& native_origin_from_anchor,
      uint64_t plane_id,
      CreateAnchorCallback callback) override;

  void ProcessAnchorCreationRequests(
      const gfx::Transform& mojo_from_viewer,
      const std::vector<mojom::XRInputSourceStatePtr>& input_state,
      const base::TimeTicks& frame_time) override;

  void DetachAnchor(uint64_t anchor_id) override;

  mojom::XRDepthDataPtr GetDepthData() override;

  mojom::XRTrackedImagesDataPtr GetTrackedImages() override;

  gfx::Size GetUncroppedCameraImageSize() const override;

 protected:
  std::vector<float> TransformDisplayUvCoords(
      const base::span<const float> uvs) const override;

 private:
  void BuildImageDatabase(
      const ArSession*,
      ArAugmentedImageDatabase*,
      const std::vector<device::mojom::XRTrackedImagePtr>& tracked_images);
  bool IsOnGlThread() const;
  base::WeakPtr<ArCoreImpl> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  scoped_refptr<base::SingleThreadTaskRunner> gl_thread_task_runner_;

  // An ArCore session, which is distinct and independent of XRSessions.
  // There will only ever be one in Chrome even when supporting
  // multiple XRSessions.
  internal::ScopedArCoreObject<ArSession*> arcore_session_;
  internal::ScopedArCoreObject<ArFrame*> arcore_frame_;

  gfx::Size uncropped_camera_image_size_;

  // Target framerate reflecting the current camera configuration.
  MinMaxRange target_framerate_range_ = {30.f, 30.f};

  // ArCore light estimation data
  internal::ScopedArCoreObject<ArLightEstimate*> arcore_light_estimate_;

  // Plane manager. Valid after a call to Initialize.
  std::unique_ptr<ArCorePlaneManager> plane_manager_;
  // Anchor manager. Valid after a call to Initialize.
  std::unique_ptr<ArCoreAnchorManager> anchor_manager_;

  // For each image in the input list of images to track, store a true/false
  // score to indicate if it's trackable by ARCore or not. These are sent
  // to Blink only once, for the first frame, and the boolean tracks that.
  std::vector<bool> image_trackable_scores_;
  bool image_trackable_scores_sent_ = false;

  // Map from ARCore's internal image IDs to the index position in the input
  // list of images. The index values are needed for blink communication.
  std::unordered_map<int32_t, uint64_t> tracked_image_arcore_id_to_index_;

  std::optional<device::mojom::XRDepthConfig> depth_configuration_;

  uint64_t next_id_ = 1;

  std::map<HitTestSubscriptionId, HitTestSubscriptionData>
      hit_test_subscription_id_to_data_;
  std::map<HitTestSubscriptionId, TransientInputHitTestSubscriptionData>
      hit_test_subscription_id_to_transient_hit_test_data_;

  std::vector<CreateAnchorRequest> create_anchor_requests_;
  std::vector<CreatePlaneAttachedAnchorRequest>
      create_plane_attached_anchor_requests_;

  // The time delta (relative to ARCore's depth data time base) of the last
  // retrieved depth API data. Used to ensure that we do not return same data to
  // the renderer if there were no changes.
  base::TimeDelta previous_depth_data_time_;

  HitTestSubscriptionId CreateHitTestSubscriptionId();

  // Returns hit test subscription results for a single subscription given
  // current XRSpace transformation.
  device::mojom::XRHitTestSubscriptionResultDataPtr
  GetHitTestSubscriptionResult(
      HitTestSubscriptionId id,
      const mojom::XRRay& ray,
      const std::vector<mojom::EntityTypeForHitTest>& entity_types,
      const gfx::Transform& ray_transformation);

  // Returns transient hit test subscription results for a single subscription.
  // The results will be grouped by input source as there might be multiple
  // input sources that match input source profile name set on a transient hit
  // test subscription.
  // |input_source_ids_and_transforms| contains tuples with (input source id,
  // mojo from input source transform).
  device::mojom::XRHitTestTransientInputSubscriptionResultDataPtr
  GetTransientHitTestSubscriptionResult(
      HitTestSubscriptionId id,
      const mojom::XRRay& ray,
      const std::vector<mojom::EntityTypeForHitTest>& entity_types,
      const std::vector<std::pair<uint32_t, gfx::Transform>>&
          input_source_ids_and_transforms);

  // Returns true if the given native origin exists, false otherwise.
  bool NativeOriginExists(
      const mojom::XRNativeOriginInformation& native_origin_information,
      const std::vector<mojom::XRInputSourceStatePtr>& input_state);

  // Returns mojo_from_native_origin transform given native origin
  // information. If the transform cannot be found or is unknown, it will return
  // std::nullopt.
  std::optional<gfx::Transform> GetMojoFromNativeOrigin(
      const mojom::XRNativeOriginInformation& native_origin_information,
      const gfx::Transform& mojo_from_viewer,
      const std::vector<mojom::XRInputSourceStatePtr>& input_state);

  // Returns mojo_from_reference_space transform given reference space type.
  // Mojo_from_reference_space is equivalent to mojo_from_native_origin for
  // native origins that are reference spaces. If the transform cannot be found,
  // it will return std::nullopt.
  std::optional<gfx::Transform> GetMojoFromReferenceSpace(
      device::mojom::XRReferenceSpaceType type,
      const gfx::Transform& mojo_from_viewer);

  // Returns a collection of tuples (input_source_id,
  // mojo_from_input_source) for input sources that match the passed in
  // profile name. Mojo_from_input_source is equivalent to
  // mojo_from_native_origin for native origins that are input sources. If
  // there are no input sources that match the profile name, the result will
  // be empty.
  std::vector<std::pair<uint32_t, gfx::Transform>> GetMojoFromInputSources(
      const std::string& profile_name,
      const gfx::Transform& mojo_from_viewer,
      const std::vector<mojom::XRInputSourceStatePtr>& maybe_input_state);

  // Processes deferred anchor creation requests.
  // |mojo_from_viewer| - viewer pose in world space of the current frame.
  // |input_state| - current input state.
  // |anchor_creation_requests| - vector of deferred anchor creation requests
  // that are supposed to be processed now; post-call, the vector will only
  // contain the requests that have not been processed.
  // |create_anchor_function| - function to call to actually create the anchor;
  // it will receive the specific anchor creation request, along with position
  // and orientation for the anchor, and must return std::optional<AnchorId>.
  template <typename T, typename FunctionType>
  void ProcessAnchorCreationRequestsHelper(
      const gfx::Transform& mojo_from_viewer,
      const std::vector<mojom::XRInputSourceStatePtr>& input_state,
      std::vector<T>* anchor_creation_requests,
      const base::TimeTicks& frame_time,
      FunctionType&& create_anchor_function);

  // Helper, attempts to configure ArSession's camera for use. Note that this is
  // happening during initialization, before arcore_session_ is set.
  // Returns `true` if configuration succeeded, false otherwise.
  // It can modify `enabled_features` if the camera was configured such that
  // some features have been disabled.
  bool ConfigureCamera(
      ArSession* ar_session,
      const std::unordered_set<device::mojom::XRSessionFeature>&
          required_features,
      const std::unordered_set<device::mojom::XRSessionFeature>&
          optional_features,
      std::unordered_set<device::mojom::XRSessionFeature>& enabled_features);

  // Helper, attempts to configure ArSession's features based on required and
  // optional features. Note that this is happening during initialization,
  // before arcore_session_ is set. Returns `true` if feature configuration
  // succeeded (i.e. all required features have been configured), `false`
  // otherwise. It can modify `enabled_features` if some optional features could
  // not have been configured.
  bool ConfigureFeatures(
      ArSession* ar_session,
      const std::unordered_set<device::mojom::XRSessionFeature>&
          required_features,
      const std::unordered_set<device::mojom::XRSessionFeature>&
          optional_features,
      const std::vector<device::mojom::XRTrackedImagePtr>& tracked_images,
      const std::optional<ArCore::DepthSensingConfiguration>&
          depth_sensing_config,
      std::unordered_set<device::mojom::XRSessionFeature>& enabled_features);

  // Configures depth sensing API - selects depth sensing usage and mode that is
  // compatible with the device. Returns false if it was unable to pick a
  // supported combination of mode and data format. Affects
  // |depth_sensing_usage_| and |depth_sensing_data_format_| members.
  bool ConfigureDepthSensing(
      const std::optional<ArCore::DepthSensingConfiguration>&
          depth_sensing_config);

  // Must be last.
  base::WeakPtrFactory<ArCoreImpl> weak_ptr_factory_{this};
};

// TODO(crbug.com/41389193): Once the arcore_device class is moved,
// determine if this is still necessary or if we should have some other form of
// factory that can abstract this.
class COMPONENT_EXPORT(VR_ARCORE) ArCoreImplFactory : public ArCoreFactory {
 public:
  std::unique_ptr<ArCore> Create() override;
};

}  // namespace device

#endif  // DEVICE_VR_ANDROID_ARCORE_ARCORE_IMPL_H_
