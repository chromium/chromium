// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/arcore/arcore_impl.h"

#include "base/android/jni_android.h"
#include "base/bind.h"
#include "base/containers/span.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/math_constants.h"
#include "base/optional.h"
#include "base/trace_event/trace_event.h"
#include "base/util/type_safety/pass_key.h"
#include "device/vr/android/arcore/arcore_math_utils.h"
#include "device/vr/android/arcore/arcore_plane_manager.h"
#include "device/vr/android/arcore/type_converters.h"
#include "device/vr/public/mojom/pose.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/transform_util.h"

using base::android::JavaRef;

namespace {

// Anchor creation requests that are older than 3 seconds are considered
// outdated and should be failed.
constexpr base::TimeDelta kOutdatedAnchorCreationRequestThreshold =
    base::TimeDelta::FromSeconds(3);

// Helper, returns new VRPosePtr with position and orientation set to match the
// position and orientation of passed in |pose|.
device::mojom::VRPosePtr GetMojomVRPoseFromArPose(const ArSession* session,
                                                  const ArPose* pose) {
  device::mojom::VRPosePtr result = device::mojom::VRPose::New();
  std::tie(result->orientation, result->position) =
      device::GetPositionAndOrientationFromArPose(session, pose);

  return result;
}

// Helper, creates new ArPose* with position and orientation set to match the
// position and orientation of passed in |pose|.

ArTrackableType GetArCoreEntityType(
    device::mojom::EntityTypeForHitTest entity_type) {
  switch (entity_type) {
    case device::mojom::EntityTypeForHitTest::PLANE:
      return AR_TRACKABLE_PLANE;
    case device::mojom::EntityTypeForHitTest::POINT:
      return AR_TRACKABLE_POINT;
  }
}

std::set<ArTrackableType> GetArCoreEntityTypes(
    const std::vector<device::mojom::EntityTypeForHitTest>& entity_types) {
  std::set<ArTrackableType> result;

  std::transform(entity_types.begin(), entity_types.end(),
                 std::inserter(result, result.end()), GetArCoreEntityType);

  return result;
}

// Helper, computes mojo_from_input_source transform based on mojo_from_viever
// pose and input source state (containing input_from_pointer transform, which
// in case of input sources is equivalent to viewer_from_pointer).
// TODO(https://crbug.com/1043389): this currently assumes that the input source
// ray mode is "tapping", which is OK for input sources available for AR on
// Android, but is not true in the general case. This method should duplicate
// the logic found in XRTargetRaySpace::MojoFromNative().
base::Optional<gfx::Transform> GetMojoFromInputSource(
    const device::mojom::XRInputSourceStatePtr& input_source_state,
    const gfx::Transform& mojo_from_viewer) {
  if (!input_source_state->description ||
      !input_source_state->description->input_from_pointer) {
    return base::nullopt;
  }

  gfx::Transform viewer_from_pointer =
      *input_source_state->description->input_from_pointer;

  return mojo_from_viewer * viewer_from_pointer;
}

void ReleaseArCoreCubemap(ArImageCubemap* cube_map) {
  for (auto* image : *cube_map) {
    ArImage_release(image);
  }

  memset(cube_map, 0, sizeof(*cube_map));
}

// Helper, copies ARCore image to the passed in buffer, assuming that the caller
// allocated the buffer to fit all the data.
template <typename T>
void CopyArCoreImage(const ArSession* session,
                     const ArImage* image,
                     int32_t plane_index,
                     base::span<T> out_pixels,
                     uint32_t width,
                     uint32_t height) {
  DVLOG(3) << __func__ << ": width=" << width << ", height=" << height
           << ", out_pixels.size()=" << out_pixels.size();

  DCHECK_GE(out_pixels.size(), width * height);

  int32_t src_row_stride = 0, src_pixel_stride = 0;
  ArImage_getPlaneRowStride(session, image, plane_index, &src_row_stride);
  ArImage_getPlanePixelStride(session, image, plane_index, &src_pixel_stride);

  // Naked pointer since ArImage_getPlaneData does not transfer ownership to us.
  uint8_t const* src_buffer = nullptr;
  int32_t src_buffer_length = 0;
  ArImage_getPlaneData(session, image, plane_index, &src_buffer,
                       &src_buffer_length);

  // Fast path: Source and destination have the same layout
  bool const fast_path =
      static_cast<size_t>(src_row_stride) == width * sizeof(T);
  TRACE_EVENT1("xr", "CopyArCoreImage: memcpy", "fastPath", fast_path);

  DVLOG(3) << __func__ << ": plane_index=" << plane_index
           << ", src_buffer_length=" << src_buffer_length
           << ", src_row_stride=" << src_row_stride
           << ", src_pixel_stride=" << src_pixel_stride
           << ", fast_path=" << fast_path << ", sizeof(T)=" << sizeof(T);

  // If they have the same layout, we can copy the entire buffer at once
  if (fast_path) {
    CHECK_EQ(out_pixels.size() * sizeof(T),
             static_cast<size_t>(src_buffer_length));
    memcpy(out_pixels.data(), src_buffer, src_buffer_length);
    return;
  }

  CHECK_EQ(sizeof(T), static_cast<size_t>(src_pixel_stride));

  // Slow path: copy pixel by pixel, row by row
  for (uint32_t row = 0; row < height; ++row) {
    auto* src = src_buffer + src_row_stride * row;
    auto* dest = out_pixels.data() + width * row;

    // For each pixel
    for (uint32_t x = 0; x < width; ++x) {
      memcpy(dest, src, sizeof(T));

      src += src_pixel_stride;
      dest += 1;
    }
  }
}

// Helper, copies ARCore image to the passed in vector, discovering the buffer
// size and resizing the vector first.
template <typename T>
void CopyArCoreImage(const ArSession* session,
                     const ArImage* image,
                     int32_t plane_index,
                     std::vector<T>* out_pixels,
                     uint32_t* out_width,
                     uint32_t* out_height) {
  // Get source image information
  int32_t width = 0, height = 0;
  ArImage_getWidth(session, image, &width);
  ArImage_getHeight(session, image, &height);

  *out_width = width;
  *out_height = height;

  // Allocate memory for the output.
  out_pixels->resize(width * height);

  CopyArCoreImage(session, image, plane_index, base::span<T>(*out_pixels),
                  width, height);
}

device::mojom::XRLightProbePtr GetLightProbe(
    ArSession* arcore_session,
    ArLightEstimate* arcore_light_estimate) {
  // ArCore hands out 9 sets of RGB spherical harmonics coefficients
  // https://developers.google.com/ar/reference/c/group/light#arlightestimate_getenvironmentalhdrambientsphericalharmonics
  constexpr size_t kNumShCoefficients = 9;

  auto light_probe = device::mojom::XRLightProbe::New();

  light_probe->spherical_harmonics = device::mojom::XRSphericalHarmonics::New();
  light_probe->spherical_harmonics->coefficients =
      std::vector<device::RgbTupleF32>(kNumShCoefficients,
                                       device::RgbTupleF32{});

  ArLightEstimate_getEnvironmentalHdrAmbientSphericalHarmonics(
      arcore_session, arcore_light_estimate,
      light_probe->spherical_harmonics->coefficients.data()->components);

  float main_light_direction[3] = {0};
  ArLightEstimate_getEnvironmentalHdrMainLightDirection(
      arcore_session, arcore_light_estimate, main_light_direction);
  light_probe->main_light_direction.set_x(main_light_direction[0]);
  light_probe->main_light_direction.set_y(main_light_direction[1]);
  light_probe->main_light_direction.set_z(main_light_direction[2]);

  ArLightEstimate_getEnvironmentalHdrMainLightIntensity(
      arcore_session, arcore_light_estimate,
      light_probe->main_light_intensity.components);

  return light_probe;
}

device::mojom::XRReflectionProbePtr GetReflectionProbe(
    ArSession* arcore_session,
    ArLightEstimate* arcore_light_estimate) {
  ArImageCubemap arcore_cube_map = {nullptr};
  ArLightEstimate_acquireEnvironmentalHdrCubemap(
      arcore_session, arcore_light_estimate, arcore_cube_map);

  auto cube_map = device::mojom::XRCubeMap::New();
  std::vector<device::RgbaTupleF16>* const cube_map_faces[] = {
      &cube_map->positive_x, &cube_map->negative_x, &cube_map->positive_y,
      &cube_map->negative_y, &cube_map->positive_z, &cube_map->negative_z};

  static_assert(
      base::size(cube_map_faces) == base::size(arcore_cube_map),
      "`ArImageCubemap` and `device::mojom::XRCubeMap` are expected to "
      "have the same number of faces (6).");

  static_assert(device::mojom::XRCubeMap::kNumComponentsPerPixel == 4,
                "`device::mojom::XRCubeMap::kNumComponentsPerPixel` is "
                "expected to be 4 (RGBA)`, as that's the format ArCore uses.");

  for (size_t i = 0; i < base::size(arcore_cube_map); ++i) {
    auto* arcore_cube_map_face = arcore_cube_map[i];
    if (!arcore_cube_map_face) {
      DVLOG(1) << "`ArLightEstimate_acquireEnvironmentalHdrCubemap` failed to "
                  "return all faces";
      ReleaseArCoreCubemap(&arcore_cube_map);
      return nullptr;
    }

    auto* cube_map_face = cube_map_faces[i];

    // Make sure we only have a single image plane
    int32_t num_planes = 0;
    ArImage_getNumberOfPlanes(arcore_session, arcore_cube_map_face,
                              &num_planes);
    if (num_planes != 1) {
      DVLOG(1) << "ArCore cube map face " << i
               << " does not have exactly 1 plane.";
      ReleaseArCoreCubemap(&arcore_cube_map);
      return nullptr;
    }

    // Make sure the format for the image is in RGBA16F
    ArImageFormat format = AR_IMAGE_FORMAT_INVALID;
    ArImage_getFormat(arcore_session, arcore_cube_map_face, &format);
    if (format != AR_IMAGE_FORMAT_RGBA_FP16) {
      DVLOG(1) << "ArCore cube map face " << i
               << " not in expected image format.";
      ReleaseArCoreCubemap(&arcore_cube_map);
      return nullptr;
    }

    // Copy the cubemap
    uint32_t face_width = 0, face_height = 0;
    CopyArCoreImage(arcore_session, arcore_cube_map_face, 0, cube_map_face,
                    &face_width, &face_height);

    // Make sure the cube map is square
    if (face_width != face_height) {
      DVLOG(1) << "ArCore cube map contains non-square image.";
      ReleaseArCoreCubemap(&arcore_cube_map);
      return nullptr;
    }

    // Make sure all faces have the same dimensions
    if (i == 0) {
      cube_map->width_and_height = face_width;
    } else if (face_width != cube_map->width_and_height ||
               face_height != cube_map->width_and_height) {
      DVLOG(1) << "ArCore cube map faces not all of the same dimensions.";
      ReleaseArCoreCubemap(&arcore_cube_map);
      return nullptr;
    }
  }

  ReleaseArCoreCubemap(&arcore_cube_map);

  auto reflection_probe = device::mojom::XRReflectionProbe::New();
  reflection_probe->cube_map = std::move(cube_map);
  return reflection_probe;
}

constexpr float kDefaultFloorHeightEstimation = 1.2;

}  // namespace

namespace device {

HitTestSubscriptionData::HitTestSubscriptionData(
    mojom::XRNativeOriginInformationPtr native_origin_information,
    const std::vector<mojom::EntityTypeForHitTest>& entity_types,
    mojom::XRRayPtr ray)
    : native_origin_information(std::move(native_origin_information)),
      entity_types(entity_types),
      ray(std::move(ray)) {}

HitTestSubscriptionData::HitTestSubscriptionData(
    HitTestSubscriptionData&& other) = default;
HitTestSubscriptionData::~HitTestSubscriptionData() = default;

TransientInputHitTestSubscriptionData::TransientInputHitTestSubscriptionData(
    const std::string& profile_name,
    const std::vector<mojom::EntityTypeForHitTest>& entity_types,
    mojom::XRRayPtr ray)
    : profile_name(profile_name),
      entity_types(entity_types),
      ray(std::move(ray)) {}

TransientInputHitTestSubscriptionData::TransientInputHitTestSubscriptionData(
    TransientInputHitTestSubscriptionData&& other) = default;
TransientInputHitTestSubscriptionData::
    ~TransientInputHitTestSubscriptionData() = default;

ArCoreImpl::ArCoreImpl()
    : gl_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()) {}

ArCoreImpl::~ArCoreImpl() {
  for (auto& create_anchor : create_anchor_requests_) {
    create_anchor.TakeCallback().Run(mojom::CreateAnchorResult::FAILURE, 0);
  }

  for (auto& create_anchor : create_plane_attached_anchor_requests_) {
    create_anchor.TakeCallback().Run(mojom::CreateAnchorResult::FAILURE, 0);
  }
}

bool ArCoreImpl::Initialize(
    base::android::ScopedJavaLocalRef<jobject> context,
    const std::unordered_set<device::mojom::XRSessionFeature>&
        enabled_features) {
  DCHECK(IsOnGlThread());
  DCHECK(!arcore_session_.is_valid());

  // TODO(https://crbug.com/837944): Notify error earlier if this will fail.

  JNIEnv* env = base::android::AttachCurrentThread();
  if (!env) {
    DLOG(ERROR) << "Unable to get JNIEnv for ArCore";
    return false;
  }

  // Use a local scoped ArSession for the next steps, we want the
  // arcore_session_ member to remain null until we complete successful
  // initialization.
  internal::ScopedArCoreObject<ArSession*> session;

  ArStatus status = ArSession_create(
      env, context.obj(),
      internal::ScopedArCoreObject<ArSession*>::Receiver(session).get());
  if (status != AR_SUCCESS) {
    DLOG(ERROR) << "ArSession_create failed: " << status;
    return false;
  }

  // Set incognito mode for ARCore session - this is done unconditionally as we
  // always want to limit the amount of logging done by ARCore.
  ArSession_enableIncognitoMode_private(session.get());
  DVLOG(1) << __func__ << ": ARCore incognito mode enabled";

  internal::ScopedArCoreObject<ArConfig*> arcore_config;
  ArConfig_create(
      session.get(),
      internal::ScopedArCoreObject<ArConfig*>::Receiver(arcore_config).get());
  if (!arcore_config.is_valid()) {
    DLOG(ERROR) << "ArConfig_create failed";
    return false;
  }

  // Enable lighting estimation with spherical harmonics
  ArConfig_setLightEstimationMode(session.get(), arcore_config.get(),
                                  AR_LIGHT_ESTIMATION_MODE_ENVIRONMENTAL_HDR);

  if (base::Contains(enabled_features,
                     device::mojom::XRSessionFeature::DEPTH)) {
    ArConfig_setDepthMode(session.get(), arcore_config.get(),
                          AR_DEPTH_MODE_AUTOMATIC);
  }

  status = ArSession_configure(session.get(), arcore_config.get());
  if (status != AR_SUCCESS) {
    DLOG(ERROR) << "ArSession_configure failed: " << status;
    return false;
  }

  if (!ConfigureCamera(session.get())) {
    DLOG(ERROR) << "Failed to configure camera";
    return false;
  }

  internal::ScopedArCoreObject<ArFrame*> frame;
  ArFrame_create(session.get(),
                 internal::ScopedArCoreObject<ArFrame*>::Receiver(frame).get());
  if (!frame.is_valid()) {
    DLOG(ERROR) << "ArFrame_create failed";
    return false;
  }

  internal::ScopedArCoreObject<ArLightEstimate*> light_estimate;
  ArLightEstimate_create(
      session.get(),
      internal::ScopedArCoreObject<ArLightEstimate*>::Receiver(light_estimate)
          .get());
  if (!light_estimate.is_valid()) {
    DVLOG(1) << "ArLightEstimate_create failed";
    return false;
  }

  // Success, we now have a valid session and a valid frame.
  arcore_frame_ = std::move(frame);
  arcore_session_ = std::move(session);
  arcore_light_estimate_ = std::move(light_estimate);
  anchor_manager_ = std::make_unique<ArCoreAnchorManager>(
      util::PassKey<ArCoreImpl>(), arcore_session_.get());
  plane_manager_ = std::make_unique<ArCorePlaneManager>(
      util::PassKey<ArCoreImpl>(), arcore_session_.get());
  return true;
}

bool ArCoreImpl::ConfigureCamera(ArSession* ar_session) const {
  internal::ScopedArCoreObject<ArCameraConfigFilter*> camera_config_filter;
  ArCameraConfigFilter_create(
      ar_session, internal::ScopedArCoreObject<ArCameraConfigFilter*>::Receiver(
                      camera_config_filter)
                      .get());
  if (!camera_config_filter.is_valid()) {
    DLOG(ERROR) << "ArCameraConfigFilter_create failed";
    return false;
  }

  // We only want to work at 30fps for now.
  ArCameraConfigFilter_setTargetFps(ar_session, camera_config_filter.get(),
                                    AR_CAMERA_CONFIG_TARGET_FPS_30);
  // We do not care if depth sensor is available or not for now.
  // The default depth sensor usage of the newly created filter is not
  // documented, so let's set the filter explicitly to accept both cameras with
  // and without depth sensors.
  ArCameraConfigFilter_setDepthSensorUsage(
      ar_session, camera_config_filter.get(),
      AR_CAMERA_CONFIG_DEPTH_SENSOR_USAGE_REQUIRE_AND_USE |
          AR_CAMERA_CONFIG_DEPTH_SENSOR_USAGE_DO_NOT_USE);

  internal::ScopedArCoreObject<ArCameraConfigList*> camera_config_list;
  ArCameraConfigList_create(
      ar_session, internal::ScopedArCoreObject<ArCameraConfigList*>::Receiver(
                      camera_config_list)
                      .get());

  if (!camera_config_list.is_valid()) {
    DLOG(ERROR) << "ArCameraConfigList_create failed";
    return false;
  }

  ArSession_getSupportedCameraConfigsWithFilter(
      ar_session, camera_config_filter.get(), camera_config_list.get());
  if (!camera_config_list.is_valid()) {
    DLOG(ERROR) << "ArSession_getSupportedCameraConfigsWithFilter failed";
    return false;
  }

  int32_t available_configs_count;
  ArCameraConfigList_getSize(ar_session, camera_config_list.get(),
                             &available_configs_count);

  DVLOG(2) << __func__ << ": ARCore reported " << available_configs_count
           << " available configurations";

  std::vector<internal::ScopedArCoreObject<ArCameraConfig*>> available_configs;
  available_configs.reserve(available_configs_count);
  for (int32_t i = 0; i < available_configs_count; ++i) {
    internal::ScopedArCoreObject<ArCameraConfig*> camera_config;
    ArCameraConfig_create(
        ar_session,
        internal::ScopedArCoreObject<ArCameraConfig*>::Receiver(camera_config)
            .get());

    if (!camera_config.is_valid()) {
      DVLOG(1) << __func__
               << ": ArCameraConfig_create failed for camera config at index "
               << i;
      continue;
    }

    ArCameraConfigList_getItem(ar_session, camera_config_list.get(), i,
                               camera_config.get());

    ArCameraConfigFacingDirection facing_direction;
    ArCameraConfig_getFacingDirection(ar_session, camera_config.get(),
                                      &facing_direction);

    if (facing_direction != AR_CAMERA_CONFIG_FACING_DIRECTION_BACK) {
      DVLOG(2)
          << __func__
          << ": camera config does not refer to back-facing camera, ignoring";
      continue;
    }

#if DCHECK_IS_ON()
    {
      int32_t tex_width, tex_height;
      ArCameraConfig_getTextureDimensions(ar_session, camera_config.get(),
                                          &tex_width, &tex_height);

      int32_t img_width, img_height;
      ArCameraConfig_getImageDimensions(ar_session, camera_config.get(),
                                        &img_width, &img_height);

      uint32_t depth_sensor_usage;
      ArCameraConfig_getDepthSensorUsage(ar_session, camera_config.get(),
                                         &depth_sensor_usage);

      DVLOG(3) << __func__
               << ": matching camera config found, texture dimensions="
               << tex_width << "x" << tex_height
               << ", image dimensions= " << img_width << "x" << img_height
               << ", depth sensor usage=" << depth_sensor_usage;
    }
#endif

    available_configs.push_back(std::move(camera_config));
  }

  if (available_configs.empty()) {
    DLOG(ERROR) << "No matching configs found";
    return false;
  }

  auto best_config = std::max_element(
      available_configs.begin(), available_configs.end(),
      [ar_session](const internal::ScopedArCoreObject<ArCameraConfig*>& lhs,
                   const internal::ScopedArCoreObject<ArCameraConfig*>& rhs) {
        // true means that lhs is less than rhs

        // We'll prefer the configs with higher total resolution (GPU first,
        // then CPU), everything else does not matter for us now, but we will
        // weakly prefer the cameras that support depth sensor (it will be used
        // as a tie-breaker).

        {
          int32_t lhs_tex_width, lhs_tex_height;
          int32_t rhs_tex_width, rhs_tex_height;

          ArCameraConfig_getTextureDimensions(ar_session, lhs.get(),
                                              &lhs_tex_width, &lhs_tex_height);
          ArCameraConfig_getTextureDimensions(ar_session, rhs.get(),
                                              &rhs_tex_width, &rhs_tex_height);

          if (lhs_tex_width * lhs_tex_height !=
              rhs_tex_width * rhs_tex_height) {
            return lhs_tex_width * lhs_tex_height <
                   rhs_tex_width * rhs_tex_height;
          }
        }

        {
          int32_t lhs_img_width, lhs_img_height;
          int32_t rhs_img_width, rhs_img_height;

          ArCameraConfig_getImageDimensions(ar_session, lhs.get(),
                                            &lhs_img_width, &lhs_img_height);
          ArCameraConfig_getImageDimensions(ar_session, rhs.get(),
                                            &rhs_img_width, &rhs_img_height);

          if (lhs_img_width * lhs_img_height !=
              rhs_img_width * rhs_img_height) {
            return lhs_img_width * lhs_img_height <
                   rhs_img_width * rhs_img_height;
          }
        }

        {
          uint32_t lhs_depth_sensor_usage;
          uint32_t rhs_depth_sensor_usage;

          ArCameraConfig_getDepthSensorUsage(ar_session, lhs.get(),
                                             &lhs_depth_sensor_usage);
          ArCameraConfig_getDepthSensorUsage(ar_session, rhs.get(),
                                             &rhs_depth_sensor_usage);

          bool lhs_has_depth =
              lhs_depth_sensor_usage &
              AR_CAMERA_CONFIG_DEPTH_SENSOR_USAGE_REQUIRE_AND_USE;
          bool rhs_has_depth =
              rhs_depth_sensor_usage &
              AR_CAMERA_CONFIG_DEPTH_SENSOR_USAGE_REQUIRE_AND_USE;

          return lhs_has_depth < rhs_has_depth;
        }
      });

#if DCHECK_IS_ON()
  {
    int32_t tex_width, tex_height;
    ArCameraConfig_getTextureDimensions(ar_session, best_config->get(),
                                        &tex_width, &tex_height);

    int32_t img_width, img_height;
    ArCameraConfig_getImageDimensions(ar_session, best_config->get(),
                                      &img_width, &img_height);

    uint32_t depth_sensor_usage;
    ArCameraConfig_getDepthSensorUsage(ar_session, best_config->get(),
                                       &depth_sensor_usage);
    DVLOG(3) << __func__
             << ": selected camera config with texture dimensions=" << tex_width
             << "x" << tex_height << ", image dimensions=" << img_width << "x"
             << img_height << ", depth sensor usage=" << depth_sensor_usage;
  }
#endif

  ArStatus status = ArSession_setCameraConfig(ar_session, best_config->get());
  if (status != AR_SUCCESS) {
    DLOG(ERROR) << "ArSession_setCameraConfig failed: " << status;
    return false;
  }

  return true;
}

void ArCoreImpl::SetCameraTexture(uint32_t camera_texture_id) {
  DCHECK(IsOnGlThread());
  DCHECK(arcore_session_.is_valid());
  ArSession_setCameraTextureName(arcore_session_.get(), camera_texture_id);
}

void ArCoreImpl::SetDisplayGeometry(
    const gfx::Size& frame_size,
    display::Display::Rotation display_rotation) {
  DCHECK(IsOnGlThread());
  DCHECK(arcore_session_.is_valid());
  // Display::Rotation is the same as Android's rotation and is compatible with
  // what ArCore is expecting.
  ArSession_setDisplayGeometry(arcore_session_.get(), display_rotation,
                               frame_size.width(), frame_size.height());
}

std::vector<float> ArCoreImpl::TransformDisplayUvCoords(
    const base::span<const float> uvs) const {
  DCHECK(IsOnGlThread());
  DCHECK(arcore_session_.is_valid());
  DCHECK(arcore_frame_.is_valid());

  size_t num_elements = uvs.size();
  DCHECK(num_elements % 2 == 0);
  DCHECK_GE(num_elements, 6u);

  std::vector<float> uvs_out(num_elements);
  ArFrame_transformCoordinates2d(
      arcore_session_.get(), arcore_frame_.get(),
      AR_COORDINATES_2D_VIEW_NORMALIZED, num_elements / 2, &uvs[0],
      AR_COORDINATES_2D_TEXTURE_NORMALIZED, &uvs_out[0]);

  DVLOG(3) << __func__ << ": transformed uvs=[ " << uvs_out[0] << " , "
           << uvs_out[1] << " , " << uvs_out[2] << " , " << uvs_out[3] << " , "
           << uvs_out[4] << " , " << uvs_out[5] << " ]";

  return uvs_out;
}

mojom::VRPosePtr ArCoreImpl::Update(bool* camera_updated) {
  TRACE_EVENT0("gpu", "ArCoreImpl Update");

  DCHECK(IsOnGlThread());
  DCHECK(arcore_session_.is_valid());
  DCHECK(arcore_frame_.is_valid());
  DCHECK(camera_updated);

  ArStatus status;

  TRACE_EVENT_BEGIN0("gpu", "ArCore Update");
  status = ArSession_update(arcore_session_.get(), arcore_frame_.get());
  TRACE_EVENT_END0("gpu", "ArCore Update");

  if (status != AR_SUCCESS) {
    DLOG(ERROR) << "ArSession_update failed: " << status;
    *camera_updated = false;
    return nullptr;
  }

  // If we get here, assume we have a valid camera image, but we don't know yet
  // if tracking is working.
  *camera_updated = true;
  internal::ScopedArCoreObject<ArCamera*> arcore_camera;
  ArFrame_acquireCamera(
      arcore_session_.get(), arcore_frame_.get(),
      internal::ScopedArCoreObject<ArCamera*>::Receiver(arcore_camera).get());
  if (!arcore_camera.is_valid()) {
    DLOG(ERROR) << "ArFrame_acquireCamera failed!";
    return nullptr;
  }

  ArTrackingState tracking_state;
  ArCamera_getTrackingState(arcore_session_.get(), arcore_camera.get(),
                            &tracking_state);
  if (tracking_state != AR_TRACKING_STATE_TRACKING) {
    DVLOG(1) << "Tracking state is not AR_TRACKING_STATE_TRACKING: "
             << tracking_state;
    return nullptr;
  }

  internal::ScopedArCoreObject<ArPose*> arcore_pose;
  ArPose_create(
      arcore_session_.get(), nullptr,
      internal::ScopedArCoreObject<ArPose*>::Receiver(arcore_pose).get());
  if (!arcore_pose.is_valid()) {
    DLOG(ERROR) << "ArPose_create failed!";
    return nullptr;
  }

  ArCamera_getDisplayOrientedPose(arcore_session_.get(), arcore_camera.get(),
                                  arcore_pose.get());

  auto mojo_from_viewer =
      GetMojomVRPoseFromArPose(arcore_session_.get(), arcore_pose.get());

  TRACE_EVENT_BEGIN0("gpu", "ArCorePlaneManager Update");
  plane_manager_->Update(arcore_frame_.get());
  TRACE_EVENT_END0("gpu", "ArCorePlaneManager Update");

  TRACE_EVENT_BEGIN0("gpu", "ArCoreAnchorManager Update");
  anchor_manager_->Update(arcore_frame_.get());
  TRACE_EVENT_END0("gpu", "ArCoreAnchorManager Update");

  return mojo_from_viewer;
}

base::TimeDelta ArCoreImpl::GetFrameTimestamp() {
  DCHECK(arcore_session_.is_valid());
  DCHECK(arcore_frame_.is_valid());
  int64_t out_timestamp_ns;
  ArFrame_getTimestamp(arcore_session_.get(), arcore_frame_.get(),
                       &out_timestamp_ns);
  return base::TimeDelta::FromNanoseconds(out_timestamp_ns);
}

mojom::XRPlaneDetectionDataPtr ArCoreImpl::GetDetectedPlanesData() {
  DVLOG(2) << __func__;

  TRACE_EVENT0("gpu", __func__);

  return plane_manager_->GetDetectedPlanesData();
}

mojom::XRAnchorsDataPtr ArCoreImpl::GetAnchorsData() {
  DVLOG(2) << __func__;

  TRACE_EVENT0("gpu", __func__);

  return anchor_manager_->GetAnchorsData();
}

mojom::XRLightEstimationDataPtr ArCoreImpl::GetLightEstimationData() {
  TRACE_EVENT0("gpu", __func__);

  ArFrame_getLightEstimate(arcore_session_.get(), arcore_frame_.get(),
                           arcore_light_estimate_.get());

  ArLightEstimateState light_estimate_state = AR_LIGHT_ESTIMATE_STATE_NOT_VALID;
  ArLightEstimate_getState(arcore_session_.get(), arcore_light_estimate_.get(),
                           &light_estimate_state);

  // The light estimate state is not guaranteed to be valid initially
  if (light_estimate_state != AR_LIGHT_ESTIMATE_STATE_VALID) {
    DVLOG(2) << "ArCore light estimation state invalid.";
    return nullptr;
  }

  auto light_estimation_data = mojom::XRLightEstimationData::New();
  light_estimation_data->light_probe =
      GetLightProbe(arcore_session_.get(), arcore_light_estimate_.get());
  if (!light_estimation_data->light_probe) {
    DVLOG(1) << "Failed to generate light probe.";
    return nullptr;
  }
  light_estimation_data->reflection_probe =
      GetReflectionProbe(arcore_session_.get(), arcore_light_estimate_.get());
  if (!light_estimation_data->reflection_probe) {
    DVLOG(1) << "Failed to generate reflection probe.";
    return nullptr;
  }

  return light_estimation_data;
}

void ArCoreImpl::Pause() {
  DVLOG(2) << __func__;

  DCHECK(IsOnGlThread());
  DCHECK(arcore_session_.is_valid());

  ArStatus status = ArSession_pause(arcore_session_.get());
  DLOG_IF(ERROR, status != AR_SUCCESS)
      << "ArSession_pause failed: status = " << status;
}

void ArCoreImpl::Resume() {
  DVLOG(2) << __func__;

  DCHECK(IsOnGlThread());
  DCHECK(arcore_session_.is_valid());

  ArStatus status = ArSession_resume(arcore_session_.get());
  DLOG_IF(ERROR, status != AR_SUCCESS)
      << "ArSession_resume failed: status = " << status;
}

gfx::Transform ArCoreImpl::GetProjectionMatrix(float near, float far) {
  DCHECK(IsOnGlThread());
  DCHECK(arcore_session_.is_valid());
  DCHECK(arcore_frame_.is_valid());

  internal::ScopedArCoreObject<ArCamera*> arcore_camera;
  ArFrame_acquireCamera(
      arcore_session_.get(), arcore_frame_.get(),
      internal::ScopedArCoreObject<ArCamera*>::Receiver(arcore_camera).get());
  DCHECK(arcore_camera.is_valid())
      << "ArFrame_acquireCamera failed despite documentation saying it cannot";

  // ArCore's projection matrix is 16 floats in column-major order.
  float matrix_4x4[16];
  ArCamera_getProjectionMatrix(arcore_session_.get(), arcore_camera.get(), near,
                               far, matrix_4x4);
  gfx::Transform result;
  result.matrix().setColMajorf(matrix_4x4);
  return result;
}

float ArCoreImpl::GetEstimatedFloorHeight() {
  return kDefaultFloorHeightEstimation;
}

base::Optional<uint64_t> ArCoreImpl::SubscribeToHitTest(
    mojom::XRNativeOriginInformationPtr native_origin_information,
    const std::vector<mojom::EntityTypeForHitTest>& entity_types,
    mojom::XRRayPtr ray) {
  // First, check if we recognize the type of the native origin.

  if (native_origin_information->is_reference_space_type()) {
    // Reference spaces are implicitly recognized and don't carry an ID.
  } else if (native_origin_information->is_input_source_id()) {
    // Input source IDs are verified in the higher layer as ArCoreImpl does
    // not carry input source state.
  } else if (native_origin_information->is_plane_id()) {
    // Validate that we know which plane's space the hit test is interested in
    // tracking.
    if (!plane_manager_->PlaneExists(
            PlaneId(native_origin_information->get_plane_id()))) {
      return base::nullopt;
    }
  } else if (native_origin_information->is_anchor_id()) {
    // Validate that we know which anchor's space the hit test is interested
    // in tracking.
    if (!anchor_manager_->AnchorExists(
            AnchorId(native_origin_information->get_anchor_id()))) {
      return base::nullopt;
    }
  } else {
    NOTREACHED();
    return base::nullopt;
  }

  auto subscription_id = CreateHitTestSubscriptionId();

  hit_test_subscription_id_to_data_.emplace(
      subscription_id,
      HitTestSubscriptionData{std::move(native_origin_information),
                              entity_types, std::move(ray)});

  return subscription_id.GetUnsafeValue();
}

base::Optional<uint64_t> ArCoreImpl::SubscribeToHitTestForTransientInput(
    const std::string& profile_name,
    const std::vector<mojom::EntityTypeForHitTest>& entity_types,
    mojom::XRRayPtr ray) {
  auto subscription_id = CreateHitTestSubscriptionId();

  hit_test_subscription_id_to_transient_hit_test_data_.emplace(
      subscription_id, TransientInputHitTestSubscriptionData{
                           profile_name, entity_types, std::move(ray)});

  return subscription_id.GetUnsafeValue();
}

mojom::XRHitTestSubscriptionResultsDataPtr
ArCoreImpl::GetHitTestSubscriptionResults(
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
        *subscription_id_and_data.second.ray,
        subscription_id_and_data.second.entity_types,
        *maybe_mojo_from_native_origin));
  }

  DVLOG(3)
      << __func__
      << ": calculating hit test subscription results for transient input, "
         "hit_test_subscription_id_to_transient_hit_test_data_.size()="
      << hit_test_subscription_id_to_transient_hit_test_data_.size();

  for (const auto& subscription_id_and_data :
       hit_test_subscription_id_to_transient_hit_test_data_) {
    auto input_source_ids_and_transforms =
        GetMojoFromInputSources(subscription_id_and_data.second.profile_name,
                                mojo_from_viewer, input_state);

    result->transient_input_results.push_back(
        GetTransientHitTestSubscriptionResult(
            HitTestSubscriptionId(subscription_id_and_data.first),
            *subscription_id_and_data.second.ray,
            subscription_id_and_data.second.entity_types,
            input_source_ids_and_transforms));
  }

  return result;
}

device::mojom::XRHitTestSubscriptionResultDataPtr
ArCoreImpl::GetHitTestSubscriptionResult(
    HitTestSubscriptionId id,
    const mojom::XRRay& native_origin_ray,
    const std::vector<mojom::EntityTypeForHitTest>& entity_types,
    const gfx::Transform& mojo_from_native_origin) {
  DVLOG(3) << __func__ << ": id=" << id;

  // Transform the ray according to the latest transform based on the XRSpace
  // used in hit test subscription.

  gfx::Point3F origin = native_origin_ray.origin;
  mojo_from_native_origin.TransformPoint(&origin);

  gfx::Vector3dF direction = native_origin_ray.direction;
  mojo_from_native_origin.TransformVector(&direction);

  std::vector<mojom::XRHitResultPtr> hit_results;
  if (!RequestHitTest(origin, direction, entity_types, &hit_results)) {
    hit_results.clear();  // On failure, clear partial results.
  }

  return mojom::XRHitTestSubscriptionResultData::New(id.GetUnsafeValue(),
                                                     std::move(hit_results));
}

device::mojom::XRHitTestTransientInputSubscriptionResultDataPtr
ArCoreImpl::GetTransientHitTestSubscriptionResult(
    HitTestSubscriptionId id,
    const mojom::XRRay& input_source_ray,
    const std::vector<mojom::EntityTypeForHitTest>& entity_types,
    const std::vector<std::pair<uint32_t, gfx::Transform>>&
        input_source_ids_and_mojo_from_input_sources) {
  auto result =
      device::mojom::XRHitTestTransientInputSubscriptionResultData::New();

  result->subscription_id = id.GetUnsafeValue();

  for (const auto& input_source_id_and_mojo_from_input_source :
       input_source_ids_and_mojo_from_input_sources) {
    gfx::Point3F origin = input_source_ray.origin;
    input_source_id_and_mojo_from_input_source.second.TransformPoint(&origin);

    gfx::Vector3dF direction = input_source_ray.direction;
    input_source_id_and_mojo_from_input_source.second.TransformVector(
        &direction);

    std::vector<mojom::XRHitResultPtr> hit_results;
    if (!RequestHitTest(origin, direction, entity_types, &hit_results)) {
      hit_results.clear();  // On failure, clear partial results.
    }

    result->input_source_id_to_hit_test_results.insert(
        {input_source_id_and_mojo_from_input_source.first,
         std::move(hit_results)});
  }

  return result;
}

std::vector<std::pair<uint32_t, gfx::Transform>>
ArCoreImpl::GetMojoFromInputSources(
    const std::string& profile_name,
    const gfx::Transform& mojo_from_viewer,
    const std::vector<mojom::XRInputSourceStatePtr>& input_state) {
  std::vector<std::pair<uint32_t, gfx::Transform>> result;

  for (const auto& input_source_state : input_state) {
    if (input_source_state && input_source_state->description) {
      if (base::Contains(input_source_state->description->profiles,
                         profile_name)) {
        // Input source represented by input_state matches the profile, find
        // the transform and grab input source id.
        base::Optional<gfx::Transform> maybe_mojo_from_input_source =
            GetMojoFromInputSource(input_source_state, mojo_from_viewer);

        if (!maybe_mojo_from_input_source)
          continue;

        result.push_back(
            {input_source_state->source_id, *maybe_mojo_from_input_source});
      }
    }
  }

  return result;
}

base::Optional<gfx::Transform> ArCoreImpl::GetMojoFromReferenceSpace(
    device::mojom::XRReferenceSpaceType type,
    const gfx::Transform& mojo_from_viewer) {
  switch (type) {
    case device::mojom::XRReferenceSpaceType::kLocal:
      return gfx::Transform{};
    case device::mojom::XRReferenceSpaceType::kLocalFloor: {
      auto result = gfx::Transform{};
      result.Translate3d(0, -GetEstimatedFloorHeight(), 0);
      return result;
    }
    case device::mojom::XRReferenceSpaceType::kViewer:
      return mojo_from_viewer;
    case device::mojom::XRReferenceSpaceType::kBoundedFloor:
      return base::nullopt;
    case device::mojom::XRReferenceSpaceType::kUnbounded:
      return base::nullopt;
  }
}

bool ArCoreImpl::NativeOriginExists(
    const mojom::XRNativeOriginInformation& native_origin_information,
    const std::vector<mojom::XRInputSourceStatePtr>& input_state) {
  switch (native_origin_information.which()) {
    case mojom::XRNativeOriginInformation::Tag::INPUT_SOURCE_ID:

      // Linear search should be fine for ARCore device as it only has one input
      // source (for now).
      for (auto& input_source_state : input_state) {
        if (input_source_state->source_id ==
            native_origin_information.get_input_source_id()) {
          return true;
        }
      }

      return false;
    case mojom::XRNativeOriginInformation::Tag::REFERENCE_SPACE_TYPE:
      // All reference spaces are known to ARCore.
      return true;

    case mojom::XRNativeOriginInformation::Tag::PLANE_ID:
      return plane_manager_->PlaneExists(
          PlaneId(native_origin_information.get_plane_id()));
    case mojom::XRNativeOriginInformation::Tag::ANCHOR_ID:

      return anchor_manager_->AnchorExists(
          AnchorId(native_origin_information.get_anchor_id()));
  }
}

base::Optional<gfx::Transform> ArCoreImpl::GetMojoFromNativeOrigin(
    const mojom::XRNativeOriginInformation& native_origin_information,
    const gfx::Transform& mojo_from_viewer,
    const std::vector<mojom::XRInputSourceStatePtr>& input_state) {
  switch (native_origin_information.which()) {
    case mojom::XRNativeOriginInformation::Tag::INPUT_SOURCE_ID:

      // Linear search should be fine for ARCore device as it only has one input
      // source (for now).
      for (auto& input_source_state : input_state) {
        if (input_source_state->source_id ==
            native_origin_information.get_input_source_id()) {
          return GetMojoFromInputSource(input_source_state, mojo_from_viewer);
        }
      }

      return base::nullopt;
    case mojom::XRNativeOriginInformation::Tag::REFERENCE_SPACE_TYPE:
      return GetMojoFromReferenceSpace(
          native_origin_information.get_reference_space_type(),
          mojo_from_viewer);
    case mojom::XRNativeOriginInformation::Tag::PLANE_ID:
      return plane_manager_->GetMojoFromPlane(
          PlaneId(native_origin_information.get_plane_id()));
    case mojom::XRNativeOriginInformation::Tag::ANCHOR_ID:
      return anchor_manager_->GetMojoFromAnchor(
          AnchorId(native_origin_information.get_anchor_id()));
  }
}

void ArCoreImpl::UnsubscribeFromHitTest(uint64_t subscription_id) {
  DVLOG(2) << __func__ << ": subscription_id=" << subscription_id;

  // Hit test subscription ID space is the same for transient and non-transient
  // hit test sources, so we can attempt to remove it from both collections (it
  // will succeed only for one of them anyway).

  hit_test_subscription_id_to_data_.erase(
      HitTestSubscriptionId(subscription_id));
  hit_test_subscription_id_to_transient_hit_test_data_.erase(
      HitTestSubscriptionId(subscription_id));
}

HitTestSubscriptionId ArCoreImpl::CreateHitTestSubscriptionId() {
  CHECK(next_id_ != std::numeric_limits<uint64_t>::max())
      << "preventing ID overflow";

  uint64_t current_id = next_id_++;

  return HitTestSubscriptionId(current_id);
}

bool ArCoreImpl::RequestHitTest(
    const mojom::XRRayPtr& ray,
    std::vector<mojom::XRHitResultPtr>* hit_results) {
  DCHECK(ray);
  return RequestHitTest(ray->origin, ray->direction,
                        {mojom::EntityTypeForHitTest::PLANE},
                        hit_results);  // "Plane" to maintain current behavior
                                       // of async hit test.
}

bool ArCoreImpl::RequestHitTest(
    const gfx::Point3F& origin,
    const gfx::Vector3dF& direction,
    const std::vector<mojom::EntityTypeForHitTest>& entity_types,
    std::vector<mojom::XRHitResultPtr>* hit_results) {
  DVLOG(2) << __func__ << ": origin=" << origin.ToString()
           << ", direction=" << direction.ToString();

  DCHECK(hit_results);
  DCHECK(IsOnGlThread());
  DCHECK(arcore_session_.is_valid());
  DCHECK(arcore_frame_.is_valid());

  auto arcore_entity_types = GetArCoreEntityTypes(entity_types);

  // ArCore returns hit-results in sorted order, thus providing the guarantee
  // of sorted results promised by the WebXR spec for requestHitTest().
  std::array<float, 3> origin_array = {origin.x(), origin.y(), origin.z()};
  std::array<float, 3> direction_array = {direction.x(), direction.y(),
                                          direction.z()};

  internal::ScopedArCoreObject<ArHitResultList*> arcore_hit_result_list;
  ArHitResultList_create(
      arcore_session_.get(),
      internal::ScopedArCoreObject<ArHitResultList*>::Receiver(
          arcore_hit_result_list)
          .get());
  if (!arcore_hit_result_list.is_valid()) {
    DLOG(ERROR) << "ArHitResultList_create failed!";
    return false;
  }

  ArFrame_hitTestRay(arcore_session_.get(), arcore_frame_.get(),
                     origin_array.data(), direction_array.data(),
                     arcore_hit_result_list.get());

  int arcore_hit_result_list_size = 0;
  ArHitResultList_getSize(arcore_session_.get(), arcore_hit_result_list.get(),
                          &arcore_hit_result_list_size);
  DVLOG(2) << __func__
           << ": arcore_hit_result_list_size=" << arcore_hit_result_list_size;

  // Go through the list in reverse order so the first hit we encounter is the
  // furthest.
  // We will accept the furthest hit and then for the rest require that the hit
  // be within the actual polygon detected by ArCore. This heuristic allows us
  // to get better results on floors w/o overestimating the size of tables etc.
  // See https://crbug.com/872855.
  for (int i = arcore_hit_result_list_size - 1; i >= 0; i--) {
    internal::ScopedArCoreObject<ArHitResult*> arcore_hit;

    ArHitResult_create(
        arcore_session_.get(),
        internal::ScopedArCoreObject<ArHitResult*>::Receiver(arcore_hit).get());

    if (!arcore_hit.is_valid()) {
      DLOG(ERROR) << "ArHitResult_create failed!";
      return false;
    }

    ArHitResultList_getItem(arcore_session_.get(), arcore_hit_result_list.get(),
                            i, arcore_hit.get());

    internal::ScopedArCoreObject<ArTrackable*> ar_trackable;

    ArHitResult_acquireTrackable(
        arcore_session_.get(), arcore_hit.get(),
        internal::ScopedArCoreObject<ArTrackable*>::Receiver(ar_trackable)
            .get());
    ArTrackableType ar_trackable_type = AR_TRACKABLE_NOT_VALID;
    ArTrackable_getType(arcore_session_.get(), ar_trackable.get(),
                        &ar_trackable_type);

    // Only consider trackables listed in arcore_entity_types.
    if (!base::Contains(arcore_entity_types, ar_trackable_type)) {
      DVLOG(2) << __func__
               << ": hit a trackable that is not in entity types set, ignoring "
                  "it. ar_trackable_type="
               << ar_trackable_type;
      continue;
    }

    internal::ScopedArCoreObject<ArPose*> arcore_pose;
    ArPose_create(
        arcore_session_.get(), nullptr,
        internal::ScopedArCoreObject<ArPose*>::Receiver(arcore_pose).get());
    if (!arcore_pose.is_valid()) {
      DLOG(ERROR) << "ArPose_create failed!";
      return false;
    }

    ArHitResult_getHitPose(arcore_session_.get(), arcore_hit.get(),
                           arcore_pose.get());

    // After the first (furthest) hit, for planes, only return hits that are
    // within the actual detected polygon and not just within than the larger
    // plane.
    uint64_t plane_id = 0;
    if (ar_trackable_type == AR_TRACKABLE_PLANE) {
      ArPlane* ar_plane = ArAsPlane(ar_trackable.get());

      if (!hit_results->empty()) {
        int32_t in_polygon = 0;
        ArPlane_isPoseInPolygon(arcore_session_.get(), ar_plane,
                                arcore_pose.get(), &in_polygon);
        if (!in_polygon) {
          DVLOG(2) << __func__
                   << ": hit a trackable that is not within detected polygon, "
                      "ignoring it";
          continue;
        }
      }

      base::Optional<PlaneId> maybe_plane_id =
          plane_manager_->GetPlaneId(ar_plane);
      if (maybe_plane_id) {
        plane_id = maybe_plane_id->GetUnsafeValue();
      }
    }

    mojom::XRHitResultPtr mojo_hit = mojom::XRHitResult::New();

    mojo_hit->plane_id = plane_id;

    {
      std::array<float, 7> raw_pose;
      ArPose_getPoseRaw(arcore_session_.get(), arcore_pose.get(),
                        raw_pose.data());

      gfx::Quaternion orientation(raw_pose[0], raw_pose[1], raw_pose[2],
                                  raw_pose[3]);
      gfx::Point3F position(raw_pose[4], raw_pose[5], raw_pose[6]);

      mojo_hit->mojo_from_result = device::Pose(position, orientation);

      DVLOG(3) << __func__
               << ": adding hit test result, position=" << position.ToString()
               << ", orientation=" << orientation.ToString()
               << ", plane_id=" << plane_id << " (0 means no plane)";
    }

    // Insert new results at head to preserver order from ArCore
    hit_results->insert(hit_results->begin(), std::move(mojo_hit));
  }

  DVLOG(2) << __func__ << ": hit_results->size()=" << hit_results->size();
  return true;
}

void ArCoreImpl::CreateAnchor(
    const mojom::XRNativeOriginInformation& native_origin_information,
    const device::Pose& native_origin_from_anchor,
    CreateAnchorCallback callback) {
  DVLOG(2) << __func__ << ": native_origin_information.which()="
           << static_cast<uint32_t>(native_origin_information.which())
           << ", native_origin_from_anchor.position()="
           << native_origin_from_anchor.position().ToString()
           << ", native_origin_from_anchor.orientation()="
           << native_origin_from_anchor.orientation().ToString();

  create_anchor_requests_.emplace_back(native_origin_information,
                                       native_origin_from_anchor.ToTransform(),
                                       std::move(callback));
}

void ArCoreImpl::CreatePlaneAttachedAnchor(
    const mojom::XRNativeOriginInformation& native_origin_information,
    const device::Pose& native_origin_from_anchor,
    uint64_t plane_id,
    CreateAnchorCallback callback) {
  DVLOG(2) << __func__ << ": native_origin_information.which()="
           << static_cast<uint32_t>(native_origin_information.which())
           << ", plane_id=" << plane_id
           << ", native_origin_from_anchor.position()="
           << native_origin_from_anchor.position().ToString()
           << ", native_origin_from_anchor.orientation()="
           << native_origin_from_anchor.orientation().ToString();

  create_plane_attached_anchor_requests_.emplace_back(
      native_origin_information, native_origin_from_anchor.ToTransform(),
      plane_id, std::move(callback));
}

void ArCoreImpl::ProcessAnchorCreationRequests(
    const gfx::Transform& mojo_from_viewer,
    const std::vector<mojom::XRInputSourceStatePtr>& input_state,
    const base::TimeTicks& frame_time) {
  DVLOG(2) << __func__ << ": Processing free-floating anchor creation requests";
  ProcessAnchorCreationRequestsHelper(
      mojo_from_viewer, input_state, &create_anchor_requests_, frame_time,
      [this](const CreateAnchorRequest& create_anchor_request,
             const gfx::Point3F& position, const gfx::Quaternion& orientation) {
        return anchor_manager_->CreateAnchor(
            device::mojom::Pose(orientation, position));
      });

  DVLOG(2) << __func__
           << ": Processing plane-attached anchor creation requests";
  ProcessAnchorCreationRequestsHelper(
      mojo_from_viewer, input_state, &create_plane_attached_anchor_requests_,
      frame_time,
      [this](const CreatePlaneAttachedAnchorRequest& create_anchor_request,
             const gfx::Point3F& position, const gfx::Quaternion& orientation) {
        PlaneId plane_id = PlaneId(create_anchor_request.GetPlaneId());
        return anchor_manager_->CreateAnchor(
            plane_manager_.get(), device::mojom::Pose(orientation, position),
            plane_id);
      });
}

template <typename T, typename FunctionType>
void ArCoreImpl::ProcessAnchorCreationRequestsHelper(
    const gfx::Transform& mojo_from_viewer,
    const std::vector<mojom::XRInputSourceStatePtr>& input_state,
    std::vector<T>* anchor_creation_requests,
    const base::TimeTicks& frame_time,
    FunctionType&& create_anchor_function) {
  DCHECK(anchor_creation_requests);

  DVLOG(3) << __func__ << ": pre-call anchor_creation_requests->size()="
           << anchor_creation_requests->size();

  // If we are unable to create an anchor because position of the native origin
  // is unknown, keep deferring it. On the other hand, if the anchor creation
  // failed in ARCore SDK, notify blink - we are ensuring that anchor creation
  // requests are processed when ARCore is in correct state so any failures
  // coming from ARCore SDK are real failures we won't be able to recover from.
  std::vector<T> postponed_requests;
  for (auto& create_anchor : *anchor_creation_requests) {
    auto anchor_creation_age = frame_time - create_anchor.GetRequestStartTime();

    if (anchor_creation_age > kOutdatedAnchorCreationRequestThreshold) {
      DVLOG(3)
          << __func__
          << ": failing outdated anchor creation request, anchor_creation_age="
          << anchor_creation_age;
      create_anchor.TakeCallback().Run(
          device::mojom::CreateAnchorResult::FAILURE, 0);
      continue;
    }

    mojom::XRNativeOriginInformation native_origin_information =
        create_anchor.GetNativeOriginInformation();

    if (!NativeOriginExists(native_origin_information, input_state)) {
      DVLOG(3) << __func__
               << ": failing anchor creation request, native origin does not "
                  "exist";
      create_anchor.TakeCallback().Run(
          device::mojom::CreateAnchorResult::FAILURE, 0);
      continue;
    }

    base::Optional<gfx::Transform> maybe_mojo_from_native_origin =
        GetMojoFromNativeOrigin(native_origin_information, mojo_from_viewer,
                                input_state);

    if (!maybe_mojo_from_native_origin) {
      // We don't know where the native origin currently is (but we know it is
      // still tracked), so let's postpone the request & try again later.
      DVLOG(3) << __func__
               << ": postponing anchor creation request, native origin is not "
                  "currently localizable";
      postponed_requests.emplace_back(std::move(create_anchor));
      continue;
    }

    base::Optional<device::Pose> mojo_from_anchor =
        device::Pose::Create(*maybe_mojo_from_native_origin *
                             create_anchor.GetNativeOriginFromAnchor());

    if (!mojo_from_anchor) {
      // Fail the call now, failure to decompose is unlikely to resolve itself.
      DVLOG(3)
          << __func__
          << ": failing anchor creation request, unable to decompose a matrix";
      create_anchor.TakeCallback().Run(
          device::mojom::CreateAnchorResult::FAILURE, 0);
      continue;
    }

    base::Optional<AnchorId> maybe_anchor_id = std::forward<FunctionType>(
        create_anchor_function)(create_anchor, mojo_from_anchor->position(),
                                mojo_from_anchor->orientation());

    if (!maybe_anchor_id) {
      // Fail the call now, failure to create anchor in ARCore SDK is unlikely
      // to resolve itself.
      DVLOG(3) << __func__
               << ": failing anchor creation request, anchor creation "
                  "function did not return an anchor id";
      create_anchor.TakeCallback().Run(
          device::mojom::CreateAnchorResult::FAILURE, 0);
      continue;
    }

    DVLOG(3) << __func__ << ": anchor creation request succeeded, time taken: "
             << anchor_creation_age;
    create_anchor.TakeCallback().Run(device::mojom::CreateAnchorResult::SUCCESS,
                                     maybe_anchor_id->GetUnsafeValue());
  }

  // Return the postponed requests - all other requests should have their
  // status already reported to blink at this point:
  anchor_creation_requests->swap(postponed_requests);
  DVLOG(3) << __func__ << ": post-call anchor_creation_requests->size()="
           << anchor_creation_requests->size();
}

void ArCoreImpl::DetachAnchor(uint64_t anchor_id) {
  anchor_manager_->DetachAnchor(AnchorId(anchor_id));
}

mojom::XRDepthDataPtr ArCoreImpl::GetDepthData() {
  DVLOG(3) << __func__;

  internal::ScopedArCoreObject<ArImage*> ar_image;
  ArStatus status = ArFrame_acquireDepthImage(
      arcore_session_.get(), arcore_frame_.get(),
      internal::ScopedArCoreObject<ArImage*>::Receiver(ar_image).get());

  if (status != AR_SUCCESS) {
    DVLOG(2) << __func__
             << ": ArFrame_acquireDepthImage failed, status=" << status;
    return nullptr;
  }

  int64_t timestamp_ns;
  ArImage_getTimestamp(arcore_session_.get(), ar_image.get(), &timestamp_ns);
  base::TimeDelta time_delta = base::TimeDelta::FromNanoseconds(timestamp_ns);
  DVLOG(3) << __func__ << ": depth image time_delta=" << time_delta;

  // The image returned from ArFrame_acquireDepthImage() is documented to have
  // a single 16-bit plane at index 0. The ArImage format is documented to be
  // AR_IMAGE_FORMAT_DEPTH16 (equivalent to ImageFormat.DEPTH16). There should
  // be no need to validate this in non-debug builds.
  // https://developers.google.com/ar/reference/c/group/ar-frame#arframe_acquiredepthimage
  // https://developer.android.com/reference/android/graphics/ImageFormat#DEPTH16

  ArImageFormat image_format;
  ArImage_getFormat(arcore_session_.get(), ar_image.get(), &image_format);

  CHECK_EQ(image_format, AR_IMAGE_FORMAT_DEPTH16)
      << "Depth image format must be AR_IMAGE_FORMAT_DEPTH16, found: "
      << image_format;

  int32_t num_planes;
  ArImage_getNumberOfPlanes(arcore_session_.get(), ar_image.get(), &num_planes);

  CHECK_EQ(num_planes, 1) << "Depth image must have 1 plane, found: "
                          << num_planes;

  if (time_delta > previous_depth_data_time_) {
    mojom::XRDepthDataUpdatedPtr result = mojom::XRDepthDataUpdated::New();

    result->time_delta = time_delta;

    int32_t width = 0, height = 0;
    ArImage_getWidth(arcore_session_.get(), ar_image.get(), &width);
    ArImage_getHeight(arcore_session_.get(), ar_image.get(), &height);

    DVLOG(3) << __func__ << ": depth image dimensions=" << width << "x"
             << height;

    // Depth image is defined as a width by height array of 2-byte elements:
    auto checked_buffer_size = base::CheckMul<size_t>(2, width, height);

    size_t buffer_size;
    if (!checked_buffer_size.AssignIfValid(&buffer_size)) {
      DVLOG(2) << __func__
               << ": overflow in 2 * width * height expression, returning null "
                  "depth data";
      return nullptr;
    }

    mojo_base::BigBuffer pixels(buffer_size);

    // Interpret BigBuffer's data as a width by height array of uint16_t's and
    // copy image data into it:
    CopyArCoreImage(
        arcore_session_.get(), ar_image.get(), 0,
        base::span<uint16_t>(reinterpret_cast<uint16_t*>(pixels.data()),
                             pixels.size() / 2),
        width, height);

    result->pixel_data = std::move(pixels);
    // Transform needed to consume the data:
    result->norm_texture_from_norm_view = GetCameraUvFromScreenUvTransform();
    result->size = gfx::Size(width, height);

    DVLOG(3) << __func__ << ": norm_texture_from_norm_view=\n"
             << result->norm_texture_from_norm_view.ToString();

    previous_depth_data_time_ = time_delta;

    return mojom::XRDepthData::NewUpdatedDepthData(std::move(result));
  }

  return mojom::XRDepthData::NewDataStillValid(
      mojom::XRDepthDataStillValid::New());
}

bool ArCoreImpl::IsOnGlThread() const {
  return gl_thread_task_runner_->BelongsToCurrentThread();
}

std::unique_ptr<ArCore> ArCoreImplFactory::Create() {
  return std::make_unique<ArCoreImpl>();
}

CreateAnchorRequest::CreateAnchorRequest(
    const mojom::XRNativeOriginInformation& native_origin_information,
    const gfx::Transform& native_origin_from_anchor,
    ArCore::CreateAnchorCallback callback)
    : native_origin_information_(native_origin_information),
      native_origin_from_anchor_(native_origin_from_anchor),
      request_start_time_(base::TimeTicks::Now()),
      callback_(std::move(callback)) {}
CreateAnchorRequest::CreateAnchorRequest(CreateAnchorRequest&& other) = default;
CreateAnchorRequest::~CreateAnchorRequest() = default;

mojom::XRNativeOriginInformation
CreateAnchorRequest::GetNativeOriginInformation() const {
  return native_origin_information_;
}

gfx::Transform CreateAnchorRequest::GetNativeOriginFromAnchor() const {
  return native_origin_from_anchor_;
}

base::TimeTicks CreateAnchorRequest::GetRequestStartTime() const {
  return request_start_time_;
}

ArCore::CreateAnchorCallback CreateAnchorRequest::TakeCallback() {
  return std::move(callback_);
}

CreatePlaneAttachedAnchorRequest::CreatePlaneAttachedAnchorRequest(
    const mojom::XRNativeOriginInformation& native_origin_information,
    const gfx::Transform& native_origin_from_anchor,
    uint64_t plane_id,
    ArCore::CreateAnchorCallback callback)
    : native_origin_information_(native_origin_information),
      native_origin_from_anchor_(native_origin_from_anchor),
      plane_id_(plane_id),
      request_start_time_(base::TimeTicks::Now()),
      callback_(std::move(callback)) {}
CreatePlaneAttachedAnchorRequest::CreatePlaneAttachedAnchorRequest(
    CreatePlaneAttachedAnchorRequest&& other) = default;
CreatePlaneAttachedAnchorRequest::~CreatePlaneAttachedAnchorRequest() = default;

mojom::XRNativeOriginInformation
CreatePlaneAttachedAnchorRequest::GetNativeOriginInformation() const {
  return native_origin_information_;
}

uint64_t CreatePlaneAttachedAnchorRequest::GetPlaneId() const {
  return plane_id_;
}

gfx::Transform CreatePlaneAttachedAnchorRequest::GetNativeOriginFromAnchor()
    const {
  return native_origin_from_anchor_;
}

base::TimeTicks CreatePlaneAttachedAnchorRequest::GetRequestStartTime() const {
  return request_start_time_;
}

ArCore::CreateAnchorCallback CreatePlaneAttachedAnchorRequest::TakeCallback() {
  return std::move(callback_);
}

}  // namespace device
