// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "device/vr/android/arcore/arcore_impl.h"

#include <optional>

#include "base/android/jni_android.h"
#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/checked_math.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "base/types/pass_key.h"
#include "device/vr/android/arcore/arcore_math_utils.h"
#include "device/vr/android/arcore/arcore_plane_manager.h"
#include "device/vr/android/arcore/vr_service_type_converters.h"
#include "device/vr/create_anchor_request.h"
#include "device/vr/public/mojom/pose.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/public/mojom/xr_session.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/transform_util.h"

using base::android::JavaRef;

namespace {

// Anchor creation requests that are older than 3 seconds are considered
// outdated and should be failed.
constexpr base::TimeDelta kOutdatedAnchorCreationRequestThreshold =
    base::Seconds(3);

// Helper, returns new VRPosePtr with position and orientation set to match the
// position and orientation of passed in |pose|.
device::mojom::VRPosePtr GetMojomVRPoseFromArPose(const ArSession* session,
                                                  const ArPose* pose) {
  device::mojom::VRPosePtr result = device::mojom::VRPose::New();
  std::tie(result->orientation, result->position) =
      device::GetPositionAndOrientationFromArPose(session, pose);

  return result;
}

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

  base::ranges::transform(entity_types, std::inserter(result, result.end()),
                          GetArCoreEntityType);

  return result;
}

// Helper, computes mojo_from_input_source transform based on mojo_from_viever
// pose and input source state (containing input_from_pointer transform, which
// in case of input sources is equivalent to viewer_from_pointer).
// TODO(crbug.com/40669002): this currently assumes that the input source
// ray mode is "tapping", which is OK for input sources available for AR on
// Android, but is not true in the general case. This method should duplicate
// the logic found in XRTargetRaySpace::MojoFromNative().
std::optional<gfx::Transform> GetMojoFromInputSource(
    const device::mojom::XRInputSourceStatePtr& input_source_state,
    const gfx::Transform& mojo_from_viewer) {
  if (!input_source_state->description ||
      !input_source_state->description->input_from_pointer) {
    return std::nullopt;
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
void CopyArCoreImage(const ArSession* session,
                     const ArImage* image,
                     int32_t plane_index,
                     base::span<uint8_t> out_pixels,
                     size_t out_pixel_size,
                     uint32_t width,
                     uint32_t height) {
  DVLOG(3) << __func__ << ": width=" << width << ", height=" << height
           << ", out_pixels.size()=" << out_pixels.size();

  CHECK_GE(out_pixels.size(), out_pixel_size * width * height);

  int32_t src_row_stride = 0, src_pixel_stride = 0;
  ArImage_getPlaneRowStride(session, image, plane_index, &src_row_stride);
  ArImage_getPlanePixelStride(session, image, plane_index, &src_pixel_stride);

  // Naked pointer since ArImage_getPlaneData does not transfer ownership to us.
  uint8_t const* src_buffer = nullptr;
  int32_t src_buffer_length = 0;
  ArImage_getPlaneData(session, image, plane_index, &src_buffer,
                       &src_buffer_length);
  // size_t can hold more positive numbers than int32_t so as long as the length
  // is greater than 0 (which it should be) the static_cast is safe.
  CHECK_GE(src_buffer_length, 0);
  base::span<const uint8_t> src_span(src_buffer,
                                     static_cast<size_t>(src_buffer_length));

  // Fast path: Source and destination have the same layout
  bool const fast_path =
      static_cast<size_t>(src_row_stride) == width * out_pixel_size;
  TRACE_EVENT1("xr", "CopyArCoreImage: memcpy", "fastPath", fast_path);
  UMA_HISTOGRAM_BOOLEAN("XR.ARCore.ImageCopyFastPath", fast_path);

  DVLOG(3) << __func__ << ": plane_index=" << plane_index
           << ", src_buffer_length=" << src_buffer_length
           << ", src_row_stride=" << src_row_stride
           << ", src_pixel_stride=" << src_pixel_stride
           << ", fast_path=" << fast_path
           << ", out_pixel_size=" << out_pixel_size;

  // If they have the same layout, we can copy the entire buffer at once
  if (fast_path) {
    out_pixels.copy_from(src_span);
    return;
  }

  CHECK_EQ(out_pixel_size, static_cast<size_t>(src_pixel_stride));

  // Slow path: copy row by row
  // If we're taking this path, it means that our row stride is longer than it
  // would otherwise be for a given row. First copy the relevant bytes worth of
  // data, then advance |out_pixels| by the amount of bytes copied, and src_span
  // by the row stride to advance each of them to the next row.
  const size_t data_bytes_per_row = width * src_pixel_stride;
  for (uint32_t row = 0; row < height; ++row) {
    out_pixels.copy_prefix_from(src_span.first(data_bytes_per_row));
    out_pixels = out_pixels.subspan(data_bytes_per_row);
    src_span = src_span.subspan(src_row_stride);
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

  CopyArCoreImage(session, image, plane_index,
                  base::as_writable_byte_span(*out_pixels), sizeof(T), width,
                  height);
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
      std::size(cube_map_faces) == std::size(arcore_cube_map),
      "`ArImageCubemap` and `device::mojom::XRCubeMap` are expected to "
      "have the same number of faces (6).");

  static_assert(device::mojom::XRCubeMap::kNumComponentsPerPixel == 4,
                "`device::mojom::XRCubeMap::kNumComponentsPerPixel` is "
                "expected to be 4 (RGBA)`, as that's the format ArCore uses.");

  for (size_t i = 0; i < std::size(arcore_cube_map); ++i) {
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

constexpr std::array<device::mojom::XRDepthDataFormat, 2>
    kSupportedDepthFormats = {
        device::mojom::XRDepthDataFormat::kLuminanceAlpha,
        device::mojom::XRDepthDataFormat::kUnsignedShort,
};
}  // namespace

namespace device {

namespace {

// Helper, returns the best available camera config that is using
// `facing_direction`.
internal::ScopedArCoreObject<ArCameraConfig*> GetBestConfig(
    ArSession* ar_session,
    ArCameraConfigFacingDirection facing_direction) {
  DVLOG(3) << __func__ << ": facing_direction=" << facing_direction;
  internal::ScopedArCoreObject<ArCameraConfigFilter*> camera_config_filter;
  ArCameraConfigFilter_create(
      ar_session, internal::ScopedArCoreObject<ArCameraConfigFilter*>::Receiver(
                      camera_config_filter)
                      .get());
  if (!camera_config_filter.is_valid()) {
    DLOG(ERROR) << "ArCameraConfigFilter_create failed";
    return {};
  }

  ArCameraConfigFilter_setFacingDirection(
      ar_session, camera_config_filter.get(), facing_direction);

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
    return {};
  }

  ArSession_getSupportedCameraConfigsWithFilter(
      ar_session, camera_config_filter.get(), camera_config_list.get());
  if (!camera_config_list.is_valid()) {
    DLOG(ERROR) << "ArSession_getSupportedCameraConfigsWithFilter failed";
    return {};
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

    if constexpr (DCHECK_IS_ON()) {
      ArCameraConfigFacingDirection camera_facing_direction;
      ArCameraConfig_getFacingDirection(ar_session, camera_config.get(),
                                        &camera_facing_direction);
      DCHECK_EQ(camera_facing_direction, facing_direction);

      int32_t tex_width, tex_height;
      ArCameraConfig_getTextureDimensions(ar_session, camera_config.get(),
                                          &tex_width, &tex_height);

      int32_t img_width, img_height;
      ArCameraConfig_getImageDimensions(ar_session, camera_config.get(),
                                        &img_width, &img_height);

      uint32_t depth_sensor_usage;
      ArCameraConfig_getDepthSensorUsage(ar_session, camera_config.get(),
                                         &depth_sensor_usage);

      int32_t min_fps, max_fps;
      ArCameraConfig_getFpsRange(ar_session, camera_config.get(), &min_fps,
                                 &max_fps);

      DVLOG(3) << __func__
               << ": matching camera config found, texture dimensions="
               << tex_width << "x" << tex_height
               << ", image dimensions= " << img_width << "x" << img_height
               << ", depth sensor usage=" << depth_sensor_usage
               << ", min_fps=" << min_fps << ", max_fps=" << max_fps
               << ", camera_facing_direction=" << camera_facing_direction;
    }

    available_configs.push_back(std::move(camera_config));
  }

  if (available_configs.empty()) {
    DLOG(ERROR) << "No matching configs found";
    return {};
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

  if constexpr (DCHECK_IS_ON()) {
    int32_t tex_width, tex_height;
    ArCameraConfig_getTextureDimensions(ar_session, best_config->get(),
                                        &tex_width, &tex_height);

    int32_t img_width, img_height;
    ArCameraConfig_getImageDimensions(ar_session, best_config->get(),
                                      &img_width, &img_height);

    uint32_t depth_sensor_usage;
    ArCameraConfig_getDepthSensorUsage(ar_session, best_config->get(),
                                       &depth_sensor_usage);

    int32_t fps_min, fps_max;
    ArCameraConfig_getFpsRange(ar_session, best_config->get(), &fps_min,
                               &fps_max);

    DVLOG(3) << __func__
             << ": selected camera config with texture dimensions=" << tex_width
             << "x" << tex_height << ", image dimensions=" << img_width << "x"
             << img_height << ", depth sensor usage=" << depth_sensor_usage
             << ", fps_min=" << fps_min << ", fps_max=" << fps_max;
  }

  return std::move(*best_config);
}

}  // namespace

ArCoreImpl::ArCoreImpl()
    : gl_thread_task_runner_(
          base::SingleThreadTaskRunner::GetCurrentDefault()) {}

ArCoreImpl::~ArCoreImpl() {
  for (auto& create_anchor : create_anchor_requests_) {
    create_anchor.TakeCallback().Run(mojom::CreateAnchorResult::FAILURE, 0);
  }

  for (auto& create_anchor : create_plane_attached_anchor_requests_) {
    create_anchor.TakeCallback().Run(mojom::CreateAnchorResult::FAILURE, 0);
  }
}

std::optional<ArCore::InitializeResult> ArCoreImpl::Initialize(
    base::android::ScopedJavaLocalRef<jobject> context,
    const std::unordered_set<device::mojom::XRSessionFeature>&
        required_features,
    const std::unordered_set<device::mojom::XRSessionFeature>&
        optional_features,
    const std::vector<device::mojom::XRTrackedImagePtr>& tracked_images,
    std::optional<ArCore::DepthSensingConfiguration> depth_sensing_config) {
  DCHECK(IsOnGlThread());
  DCHECK(!arcore_session_.is_valid());

  // TODO(crbug.com/41386064): Notify error earlier if this will fail.

  JNIEnv* env = base::android::AttachCurrentThread();
  if (!env) {
    DLOG(ERROR) << "Unable to get JNIEnv for ArCore";
    return std::nullopt;
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
    return std::nullopt;
  }

  // Set incognito mode for ARCore session - this is done unconditionally as we
  // always want to limit the amount of logging done by ARCore.
  ArSession_enableIncognitoMode_private(session.get());
  DVLOG(1) << __func__ << ": ARCore incognito mode enabled";

  // Let's assume we will be able to configure a session with all features -
  // this will be adjusted if it turns out we can only create a session w/o some
  // optional features. Currently, only depth sensing is not supported across
  // all the ARCore-capable devices. Additionally, front-facing camera may
  // affect availability of other features.
  std::unordered_set<device::mojom::XRSessionFeature> enabled_features;
  enabled_features.insert(required_features.begin(), required_features.end());
  enabled_features.insert(optional_features.begin(), optional_features.end());

  if (!ConfigureCamera(session.get(), required_features, optional_features,
                       enabled_features)) {
    DLOG(ERROR) << "Failed to configure session camera";
    return std::nullopt;
  }

  if (!ConfigureFeatures(session.get(), required_features, optional_features,
                         tracked_images, depth_sensing_config,
                         enabled_features)) {
    DLOG(ERROR) << "Failed to configure session features";
    return std::nullopt;
  }

  internal::ScopedArCoreObject<ArFrame*> frame;
  ArFrame_create(session.get(),
                 internal::ScopedArCoreObject<ArFrame*>::Receiver(frame).get());
  if (!frame.is_valid()) {
    DLOG(ERROR) << "ArFrame_create failed";
    return std::nullopt;
  }

  if (base::Contains(enabled_features,
                     device::mojom::XRSessionFeature::LIGHT_ESTIMATION)) {
    internal::ScopedArCoreObject<ArLightEstimate*> light_estimate;
    ArLightEstimate_create(
        session.get(),
        internal::ScopedArCoreObject<ArLightEstimate*>::Receiver(light_estimate)
            .get());
    if (!light_estimate.is_valid()) {
      DVLOG(1) << "ArLightEstimate_create failed";
      return std::nullopt;
    }
    arcore_light_estimate_ = std::move(light_estimate);
  }

  // Success, we now have a valid session and a valid frame.
  arcore_frame_ = std::move(frame);
  arcore_session_ = std::move(session);

  if (base::Contains(enabled_features,
                     device::mojom::XRSessionFeature::ANCHORS)) {
    anchor_manager_ = std::make_unique<ArCoreAnchorManager>(
        base::PassKey<ArCoreImpl>(), arcore_session_.get());
  }
  if (base::Contains(enabled_features,
                     device::mojom::XRSessionFeature::PLANE_DETECTION)) {
    plane_manager_ = std::make_unique<ArCorePlaneManager>(
        base::PassKey<ArCoreImpl>(), arcore_session_.get());
  }

  return ArCore::InitializeResult(enabled_features, depth_configuration_);
}

bool ArCoreImpl::ConfigureFeatures(
    ArSession* ar_session,
    const std::unordered_set<device::mojom::XRSessionFeature>&
        required_features,
    const std::unordered_set<device::mojom::XRSessionFeature>&
        optional_features,
    const std::vector<device::mojom::XRTrackedImagePtr>& tracked_images,
    const std::optional<ArCore::DepthSensingConfiguration>&
        depth_sensing_config,
    std::unordered_set<device::mojom::XRSessionFeature>& enabled_features) {
  internal::ScopedArCoreObject<ArConfig*> arcore_config;
  ArConfig_create(
      ar_session,
      internal::ScopedArCoreObject<ArConfig*>::Receiver(arcore_config).get());
  if (!arcore_config.is_valid()) {
    DLOG(ERROR) << __func__ << ": ArConfig_create failed";
    return false;
  }

  const bool light_estimation_requested =
      base::Contains(required_features,
                     device::mojom::XRSessionFeature::LIGHT_ESTIMATION) ||
      base::Contains(optional_features,
                     device::mojom::XRSessionFeature::LIGHT_ESTIMATION);

  if (light_estimation_requested) {
    // Enable lighting estimation with spherical harmonics
    ArConfig_setLightEstimationMode(ar_session, arcore_config.get(),
                                    AR_LIGHT_ESTIMATION_MODE_ENVIRONMENTAL_HDR);
  }

  const bool image_tracking_requested =
      base::Contains(required_features,
                     device::mojom::XRSessionFeature::IMAGE_TRACKING) ||
      base::Contains(optional_features,
                     device::mojom::XRSessionFeature::IMAGE_TRACKING);

  if (image_tracking_requested) {
    internal::ScopedArCoreObject<ArAugmentedImageDatabase*> image_db;
    ArAugmentedImageDatabase_create(
        ar_session,
        internal::ScopedArCoreObject<ArAugmentedImageDatabase*>::Receiver(
            image_db)
            .get());
    if (!image_db.is_valid()) {
      DLOG(ERROR) << "ArAugmentedImageDatabase creation failed";
      return false;
    }

    // Populate the image tracking database and set up data structures,
    // this doesn't modify the ArConfig or session yet.
    BuildImageDatabase(ar_session, image_db.get(), tracked_images);

    if (!tracked_image_arcore_id_to_index_.empty()) {
      // Image tracking with a non-empty image DB adds a few frames of
      // synchronization delay internally in ARCore, has a high CPU cost, and
      // reconfigures its graphics pipeline. Only activate it if we got images.
      // (Apparently an empty image db is equivalent, but that seems fragile.)
      ArConfig_setAugmentedImageDatabase(ar_session, arcore_config.get(),
                                         image_db.get());
      // Switch to autofocus mode when tracking images. The default fixed focus
      // mode has trouble tracking close images since they end up blurry.
      ArConfig_setFocusMode(ar_session, arcore_config.get(),
                            AR_FOCUS_MODE_AUTO);
    }
  }

  const bool depth_api_optional =
      base::Contains(optional_features, device::mojom::XRSessionFeature::DEPTH);
  const bool depth_api_required =
      base::Contains(required_features, device::mojom::XRSessionFeature::DEPTH);
  const bool depth_api_requested = depth_api_required || depth_api_optional;

  const bool depth_api_configuration_successful =
      depth_api_requested && ConfigureDepthSensing(depth_sensing_config);

  if (depth_api_configuration_successful) {
    // Don't try to set the depth mode if we know we won't be able to support
    // the desired usage and data format.
    ArConfig_setDepthMode(ar_session, arcore_config.get(),
                          AR_DEPTH_MODE_AUTOMATIC);
  } else if (depth_api_required) {
    // If we couldn't support the desired usage/format and depth is required,
    // reject the session.
    return false;
  } else if (depth_api_optional) {
    // If we couldn't support the desired usage/format and depth is optional,
    // remove it from our list of enabled features.
    enabled_features.erase(device::mojom::XRSessionFeature::DEPTH);
  }

  ArStatus status = ArSession_configure(ar_session, arcore_config.get());
  if (status != AR_SUCCESS && depth_api_requested &&
      depth_api_configuration_successful && !depth_api_required) {
    // Configuring an ARCore session failed for some reason, and we know depth
    // API was requested but is not required to be enabled.
    // Depth API may not be available on some ARCore-capable devices - since it
    // was requested optionally, let's try to request the session w/o it.
    // Currently, Depth API is the only feature that is not supported across the
    // board, so we speculatively assume that it is the reason why the session
    // creation failed.

    DLOG(WARNING) << __func__
                  << ": Depth API was optionally requested and the session "
                     "creation failed, re-trying with depth API disabled";

    enabled_features.erase(device::mojom::XRSessionFeature::DEPTH);

    ArConfig_setDepthMode(ar_session, arcore_config.get(),
                          AR_DEPTH_MODE_DISABLED);

    status = ArSession_configure(ar_session, arcore_config.get());
  }

  if (status != AR_SUCCESS) {
    DLOG(ERROR) << __func__ << ": ArSession_configure failed: " << status;
    return false;
  }

  return true;
}

bool ArCoreImpl::ConfigureDepthSensing(
    const std::optional<ArCore::DepthSensingConfiguration>&
        depth_sensing_config) {
  if (!depth_sensing_config) {
    return false;
  }

  // We can only support cpu-optimized usage. If the preference list is empty we
  // are allowed to return any supported depth usage.
  const auto& usage_preference = depth_sensing_config->depth_usage_preference;
  if (!usage_preference.empty() &&
      !base::Contains(usage_preference,
                      device::mojom::XRDepthUsage::kCPUOptimized)) {
    return false;
  }

  std::optional<device::mojom::XRDepthDataFormat> maybe_format;
  const auto& format_preference =
      depth_sensing_config->depth_data_format_preference;
  if (format_preference.empty()) {
    // An empty preference list means we're allowed to use our preferred format.
    maybe_format = device::mojom::XRDepthDataFormat::kLuminanceAlpha;
  } else {
    // Try and find the first format that we support in the preference list.
    const auto format_it = base::ranges::find_if(
        format_preference.begin(), format_preference.end(),
        [](const device::mojom::XRDepthDataFormat& format) {
          return base::Contains(kSupportedDepthFormats, format);
        });

    if (format_it != format_preference.end()) {
      maybe_format = *format_it;
    }
  }

  // If we were unable to find a format that we support, we cannot enable depth.
  if (!maybe_format) {
    return false;
  }

  // Note that since both of our supported formats are the same size, we don't
  // currently need to store the value we return to the session since for our
  // purposes they are interchangeable.
  static_assert(kSupportedDepthFormats.size() == 2u);
  depth_configuration_ = device::mojom::XRDepthConfig(
      device::mojom::XRDepthUsage::kCPUOptimized, *maybe_format);

  return true;
}

bool ArCoreImpl::ConfigureCamera(
    ArSession* ar_session,
    const std::unordered_set<device::mojom::XRSessionFeature>&
        required_features,
    const std::unordered_set<device::mojom::XRSessionFeature>&
        optional_features,
    std::unordered_set<device::mojom::XRSessionFeature>& enabled_features) {
  const bool front_facing_camera_required = base::Contains(
      required_features, device::mojom::XRSessionFeature::FRONT_FACING);
  const bool front_facing_camera_optional = base::Contains(
      optional_features, device::mojom::XRSessionFeature::FRONT_FACING);
  const bool front_facing_camera_requested =
      front_facing_camera_required || front_facing_camera_optional;

  DVLOG(3) << __func__ << ": front_facing_camera_requested="
           << front_facing_camera_requested;

  auto best_config =
      GetBestConfig(ar_session, front_facing_camera_requested
                                    ? AR_CAMERA_CONFIG_FACING_DIRECTION_FRONT
                                    : AR_CAMERA_CONFIG_FACING_DIRECTION_BACK);

  ArStatus status = best_config.is_valid() ? ArSession_setCameraConfig(
                                                 ar_session, best_config.get())
                                           : AR_ERROR_CAMERA_NOT_AVAILABLE;
  if (status != AR_SUCCESS && front_facing_camera_requested &&
      !front_facing_camera_required) {
    DLOG(WARNING) << "ArSession_setCameraConfig failed, status=" << status
                  << ", best_config.is_valid()=" << best_config.is_valid();

    // Front-facing camera was requested but optional and camera configuration
    // failed - let's try to configure back-facing camera:
    enabled_features.erase(device::mojom::XRSessionFeature::FRONT_FACING);

    best_config =
        GetBestConfig(ar_session, AR_CAMERA_CONFIG_FACING_DIRECTION_BACK);
    status = best_config.is_valid()
                 ? ArSession_setCameraConfig(ar_session, best_config.get())
                 : AR_ERROR_CAMERA_NOT_AVAILABLE;
  }

  if (status != AR_SUCCESS) {
    DLOG(ERROR) << "ArSession_setCameraConfig failed, status=" << status
                << ", best_config.is_valid()=" << best_config.is_valid();
    return false;
  }

  int32_t fps_min, fps_max;
  ArCameraConfig_getFpsRange(ar_session, best_config.get(), &fps_min, &fps_max);
  target_framerate_range_ = {static_cast<float>(fps_min),
                             static_cast<float>(fps_max)};

  return true;
}

ArCore::MinMaxRange ArCoreImpl::GetTargetFramerateRange() {
  return target_framerate_range_;
}

void ArCoreImpl::BuildImageDatabase(
    const ArSession* session,
    ArAugmentedImageDatabase* image_db,
    const std::vector<device::mojom::XRTrackedImagePtr>& tracked_images) {
  for (std::size_t index = 0; index < tracked_images.size(); ++index) {
    const device::mojom::XRTrackedImage* image = tracked_images[index].get();
    gfx::Size size = image->size_in_pixels;

    // Use Skia to convert the image to grayscale.
    const SkBitmap& src_bitmap = image->bitmap;
    SkBitmap canvas_bitmap;
    canvas_bitmap.allocPixelsFlags(
        SkImageInfo::Make(size.width(), size.height(), kGray_8_SkColorType,
                          kOpaque_SkAlphaType),
        SkBitmap::kZeroPixels_AllocFlag);
    SkCanvas gray_canvas(canvas_bitmap);
    sk_sp<SkImage> src_image = SkImages::RasterFromBitmap(src_bitmap);
    gray_canvas.drawImage(src_image, 0, 0);
    SkPixmap gray_pixmap;
    if (!gray_canvas.peekPixels(&gray_pixmap)) {
      DLOG(WARNING) << __func__ << ": failed to access grayscale bitmap";
      image_trackable_scores_.push_back(false);
      continue;
    }

    const SkPixmap& pixmap = gray_pixmap;
    float width_in_meters = image->width_in_meters;
    DVLOG(3) << __func__ << " tracked image index=" << index
             << " size=" << pixmap.width() << "x" << pixmap.height()
             << " width_in_meters=" << width_in_meters;
    int32_t arcore_id = -1;
    std::string id_name = base::NumberToString(index);
    ArStatus status = ArAugmentedImageDatabase_addImageWithPhysicalSize(
        session, image_db, id_name.c_str(), pixmap.addr8(), pixmap.width(),
        pixmap.height(), pixmap.rowBytesAsPixels(), width_in_meters,
        &arcore_id);
    if (status != AR_SUCCESS) {
      DVLOG(2) << __func__ << ": add image failed";
      image_trackable_scores_.push_back(false);
      continue;
    }

    // ARCore uses internal IDs for images, these only include the trackable
    // images. The tracking results need to refer to the original image index
    // corresponding to its position in the input tracked_images array.
    tracked_image_arcore_id_to_index_[arcore_id] = index;
    DVLOG(2) << __func__ << ": added image, index=" << index
             << " arcore_id=" << arcore_id;
    image_trackable_scores_.push_back(true);
  }
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

gfx::Size ArCoreImpl::GetUncroppedCameraImageSize() const {
  return uncropped_camera_image_size_;
}

mojom::VRPosePtr ArCoreImpl::Update(bool* camera_updated) {
  DVLOG(3) << __func__;

  TRACE_EVENT0("gpu", "ArCoreImpl Update");

  DCHECK(IsOnGlThread());
  DCHECK(arcore_session_.is_valid());
  DCHECK(arcore_frame_.is_valid());
  DCHECK(camera_updated);

  TRACE_EVENT_BEGIN0("gpu", "ArCore Update");
  ArStatus status =
      ArSession_update(arcore_session_.get(), arcore_frame_.get());
  TRACE_EVENT_END0("gpu", "ArCore Update");

  if (status != AR_SUCCESS) {
    DLOG(ERROR) << "ArSession_update failed: " << status;
    *camera_updated = false;
    return nullptr;
  }

  if (plane_manager_) {
    TRACE_EVENT0("gpu", "ArCorePlaneManager Update");
    plane_manager_->Update(arcore_frame_.get());
  }

  if (anchor_manager_) {
    TRACE_EVENT0("gpu", "ArCoreAnchorManager Update");
    anchor_manager_->Update(arcore_frame_.get());
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

  // Get the camera image dimensions via ARCore's intrinsics methods. We don't
  // currently use the raw focal length and principal point that's exported by
  // ARCore. Instead, WebXR uses the projection matrix provided by
  // ARCore. Currently, it seems that ARCore simply uses a centered virtual
  // camera, ignoring the low-level principal point which may be offset from
  // center by a few pixels. This is unlikely to make a noticeable difference in
  // practice.
  internal::ScopedArCoreObject<ArCameraIntrinsics*> intrinsics;
  ArCameraIntrinsics_create(
      arcore_session_.get(),
      internal::ScopedArCoreObject<ArCameraIntrinsics*>::Receiver(intrinsics)
          .get());
  ArCamera_getTextureIntrinsics(arcore_session_.get(), arcore_camera.get(),
                                intrinsics.get());
  int32_t intrinsics_width, intrinsics_height;
  ArCameraIntrinsics_getImageDimensions(arcore_session_.get(), intrinsics.get(),
                                        &intrinsics_width, &intrinsics_height);
  DVLOG(3) << __func__ << ": intrinsics_width=" << intrinsics_width
           << " intrinsics_height=" << intrinsics_height;
  uncropped_camera_image_size_ = {intrinsics_width, intrinsics_height};

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

  return mojo_from_viewer;
}

mojom::XRTrackedImagesDataPtr ArCoreImpl::GetTrackedImages() {
  std::vector<mojom::XRTrackedImageDataPtr> images_data;

  internal::ScopedArCoreObject<ArTrackableList*> updated_images;
  ArTrackableList_create(
      arcore_session_.get(),
      internal::ScopedArCoreObject<ArTrackableList*>::Receiver(updated_images)
          .get());
  ArFrame_getUpdatedTrackables(arcore_session_.get(), arcore_frame_.get(),
                               AR_TRACKABLE_AUGMENTED_IMAGE,
                               updated_images.get());

  int32_t images_count = 0;
  ArTrackableList_getSize(arcore_session_.get(), updated_images.get(),
                          &images_count);
  DVLOG(2) << __func__ << ": trackable images count=" << images_count;

  for (int32_t i = 0; i < images_count; ++i) {
    internal::ScopedArCoreObject<ArTrackable*> trackable;
    ArTrackableList_acquireItem(
        arcore_session_.get(), updated_images.get(), i,
        internal::ScopedArCoreObject<ArTrackable*>::Receiver(trackable).get());
    ArTrackingState tracking_state;
    ArTrackable_getTrackingState(arcore_session_.get(), trackable.get(),
                                 &tracking_state);
    ArAugmentedImage* image = ArAsAugmentedImage(trackable.get());

    // Get the original image index from ARCore's internal ID for use in the
    // returned results.
    int32_t arcore_id;
    ArAugmentedImage_getIndex(arcore_session_.get(), image, &arcore_id);
    uint64_t index = tracked_image_arcore_id_to_index_[arcore_id];
    DVLOG(3) << __func__ << ": #" << i << " tracked image index=" << index
             << " arcore_id=" << arcore_id << " state=" << tracking_state;

    if (tracking_state == AR_TRACKING_STATE_TRACKING) {
      internal::ScopedArCoreObject<ArPose*> arcore_pose;
      ArPose_create(
          arcore_session_.get(), nullptr,
          internal::ScopedArCoreObject<ArPose*>::Receiver(arcore_pose).get());
      if (!arcore_pose.is_valid()) {
        DLOG(ERROR) << "ArPose_create failed!";
        continue;
      }
      ArAugmentedImage_getCenterPose(arcore_session_.get(), image,
                                     arcore_pose.get());
      float pose_raw[7];
      ArPose_getPoseRaw(arcore_session_.get(), arcore_pose.get(), &pose_raw[0]);
      DVLOG(3) << __func__ << ": tracked image pose_raw pos=(" << pose_raw[4]
               << ", " << pose_raw[5] << ", " << pose_raw[6] << ")";

      device::Pose device_pose =
          GetPoseFromArPose(arcore_session_.get(), arcore_pose.get());

      ArAugmentedImageTrackingMethod tracking_method;
      ArAugmentedImage_getTrackingMethod(arcore_session_.get(), image,
                                         &tracking_method);
      bool actively_tracked =
          tracking_method == AR_AUGMENTED_IMAGE_TRACKING_METHOD_FULL_TRACKING;

      float width_in_meters;
      ArAugmentedImage_getExtentX(arcore_session_.get(), image,
                                  &width_in_meters);

      images_data.push_back(mojom::XRTrackedImageData::New(
          index, device_pose, actively_tracked, width_in_meters));
    }
  }

  // Include information about each image's trackability status in the first
  // returned result list.
  if (!image_trackable_scores_sent_) {
    auto ret = mojom::XRTrackedImagesData::New(
        std::move(images_data), std::move(image_trackable_scores_));
    image_trackable_scores_sent_ = true;
    image_trackable_scores_.clear();
    return ret;
  } else {
    return mojom::XRTrackedImagesData::New(std::move(images_data),
                                           std::nullopt);
  }
}

base::TimeDelta ArCoreImpl::GetFrameTimestamp() {
  DCHECK(arcore_session_.is_valid());
  DCHECK(arcore_frame_.is_valid());
  int64_t out_timestamp_ns;
  ArFrame_getTimestamp(arcore_session_.get(), arcore_frame_.get(),
                       &out_timestamp_ns);
  return base::Nanoseconds(out_timestamp_ns);
}

mojom::XRPlaneDetectionDataPtr ArCoreImpl::GetDetectedPlanesData() {
  DVLOG(2) << __func__;

  TRACE_EVENT0("gpu", __func__);

  // ArCoreGl::ProcessFrame only calls this method if the feature is enabled.
  DCHECK(plane_manager_);

  return plane_manager_->GetDetectedPlanesData();
}

mojom::XRAnchorsDataPtr ArCoreImpl::GetAnchorsData() {
  DVLOG(2) << __func__;

  TRACE_EVENT0("gpu", __func__);

  // ArCoreGl::ProcessFrame only calls this method if the feature is enabled.
  DCHECK(anchor_manager_);

  return anchor_manager_->GetAnchorsData();
}

mojom::XRLightEstimationDataPtr ArCoreImpl::GetLightEstimationData() {
  TRACE_EVENT0("gpu", __func__);

  // ArCoreGl::ProcessFrame only calls this method if the feature is enabled.
  DCHECK(arcore_light_estimate_.get());

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
  return gfx::Transform::ColMajorF(matrix_4x4);
}

float ArCoreImpl::GetEstimatedFloorHeight() {
  return kDefaultFloorHeightEstimation;
}

std::optional<uint64_t> ArCoreImpl::SubscribeToHitTest(
    mojom::XRNativeOriginInformationPtr native_origin_information,
    const std::vector<mojom::EntityTypeForHitTest>& entity_types,
    mojom::XRRayPtr ray) {
  // First, check if we recognize the type of the native origin.
  switch (native_origin_information->which()) {
    case mojom::XRNativeOriginInformation::Tag::kInputSourceSpaceInfo:
      // Input sources are verified in the higher layer as ArCoreImpl does
      // not carry input source state.
      break;
    case mojom::XRNativeOriginInformation::Tag::kReferenceSpaceType:
      // Reference spaces are implicitly recognized and don't carry an ID.
      break;
    case mojom::XRNativeOriginInformation::Tag::kPlaneId:
      // Validate that we know which plane's space the hit test is interested in
      // tracking.
      if (!plane_manager_ || !plane_manager_->PlaneExists(PlaneId(
                                 native_origin_information->get_plane_id()))) {
        return std::nullopt;
      }
      break;
    case mojom::XRNativeOriginInformation::Tag::kHandJointSpaceInfo:
      // Unsupported by ARCore:
      return std::nullopt;
    case mojom::XRNativeOriginInformation::Tag::kImageIndex:
      // TODO(crbug.com/40728355): Add hit test support for tracked
      // images.
      return std::nullopt;
    case mojom::XRNativeOriginInformation::Tag::kAnchorId:
      // Validate that we know which anchor's space the hit test is interested
      // in tracking.
      if (!anchor_manager_ ||
          !anchor_manager_->AnchorExists(
              AnchorId(native_origin_information->get_anchor_id()))) {
        return std::nullopt;
      }
      break;
  }

  auto subscription_id = CreateHitTestSubscriptionId();

  hit_test_subscription_id_to_data_.emplace(
      subscription_id,
      HitTestSubscriptionData{std::move(native_origin_information),
                              entity_types, std::move(ray)});

  return subscription_id.GetUnsafeValue();
}

std::optional<uint64_t> ArCoreImpl::SubscribeToHitTestForTransientInput(
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

  gfx::Point3F origin =
      mojo_from_native_origin.MapPoint(native_origin_ray.origin);

  gfx::Vector3dF direction =
      mojo_from_native_origin.MapVector(native_origin_ray.direction);

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
    gfx::Point3F origin =
        input_source_id_and_mojo_from_input_source.second.MapPoint(
            input_source_ray.origin);

    gfx::Vector3dF direction =
        input_source_id_and_mojo_from_input_source.second.MapVector(
            input_source_ray.direction);

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
        std::optional<gfx::Transform> maybe_mojo_from_input_source =
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

std::optional<gfx::Transform> ArCoreImpl::GetMojoFromReferenceSpace(
    device::mojom::XRReferenceSpaceType type,
    const gfx::Transform& mojo_from_viewer) {
  DVLOG(3) << __func__ << ": type=" << type;

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
      return std::nullopt;
    case device::mojom::XRReferenceSpaceType::kUnbounded:
      return std::nullopt;
  }
}

bool ArCoreImpl::NativeOriginExists(
    const mojom::XRNativeOriginInformation& native_origin_information,
    const std::vector<mojom::XRInputSourceStatePtr>& input_state) {
  switch (native_origin_information.which()) {
    case mojom::XRNativeOriginInformation::Tag::kInputSourceSpaceInfo: {
      mojom::XRInputSourceSpaceInfo* input_source_space_info =
          native_origin_information.get_input_source_space_info().get();

      // ARCore only supports input sources that have "TAPPING" target ray mode,
      // those input sources do not have grip space so the native origin is
      // guaranteed not to exist:
      if (input_source_space_info->input_source_space_type ==
          mojom::XRInputSourceSpaceType::kGrip) {
        return false;
      }

      // Linear search should be fine for ARCore device as it only has one input
      // source (for now).
      for (auto& input_source_state : input_state) {
        if (input_source_state->source_id ==
            input_source_space_info->input_source_id) {
          return true;
        }
      }

      return false;
    }
    case mojom::XRNativeOriginInformation::Tag::kReferenceSpaceType:
      // All reference spaces are known to ARCore.
      return true;

    case mojom::XRNativeOriginInformation::Tag::kPlaneId:
      return plane_manager_ ? plane_manager_->PlaneExists(PlaneId(
                                  native_origin_information.get_plane_id()))
                            : false;
    case mojom::XRNativeOriginInformation::Tag::kAnchorId:
      return anchor_manager_ ? anchor_manager_->AnchorExists(AnchorId(
                                   native_origin_information.get_anchor_id()))
                             : false;
    case mojom::XRNativeOriginInformation::Tag::kHandJointSpaceInfo:
      return false;
    case mojom::XRNativeOriginInformation::Tag::kImageIndex:
      // TODO(crbug.com/40728355): Needed for anchor creation relaitve to
      // tracked images.
      return false;
  }
}

std::optional<gfx::Transform> ArCoreImpl::GetMojoFromNativeOrigin(
    const mojom::XRNativeOriginInformation& native_origin_information,
    const gfx::Transform& mojo_from_viewer,
    const std::vector<mojom::XRInputSourceStatePtr>& input_state) {
  switch (native_origin_information.which()) {
    case mojom::XRNativeOriginInformation::Tag::kInputSourceSpaceInfo: {
      mojom::XRInputSourceSpaceInfo* input_source_space_info =
          native_origin_information.get_input_source_space_info().get();

      // ARCore only supports input sources that have "TAPPING" target ray mode,
      // those input sources do not have grip space so the native origin is
      // guaranteed not to be localizable:
      if (input_source_space_info->input_source_space_type ==
          mojom::XRInputSourceSpaceType::kGrip) {
        return std::nullopt;
      }

      // Linear search should be fine for ARCore device as it only has one input
      // source (for now).
      for (auto& input_source_state : input_state) {
        if (input_source_state->source_id ==
            input_source_space_info->input_source_id) {
          return GetMojoFromInputSource(input_source_state, mojo_from_viewer);
        }
      }

      return std::nullopt;
    }
    case mojom::XRNativeOriginInformation::Tag::kReferenceSpaceType:
      return GetMojoFromReferenceSpace(
          native_origin_information.get_reference_space_type(),
          mojo_from_viewer);
    case mojom::XRNativeOriginInformation::Tag::kPlaneId:
      return plane_manager_ ? plane_manager_->GetMojoFromPlane(PlaneId(
                                  native_origin_information.get_plane_id()))
                            : std::nullopt;
    case mojom::XRNativeOriginInformation::Tag::kAnchorId:
      return anchor_manager_ ? anchor_manager_->GetMojoFromAnchor(AnchorId(
                                   native_origin_information.get_anchor_id()))
                             : std::nullopt;
    case mojom::XRNativeOriginInformation::Tag::kHandJointSpaceInfo:
      return std::nullopt;

    case mojom::XRNativeOriginInformation::Tag::kImageIndex:
      // TODO(crbug.com/40728355): Needed for hit test and anchors
      // support for tracked images.
      return std::nullopt;
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

      std::optional<PlaneId> maybe_plane_id =
          plane_manager_ ? plane_manager_->GetPlaneId(ar_plane) : std::nullopt;
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
  // This is only called from ArCoreGl::ProcessFrame if the feature is enabled.
  DCHECK(anchor_manager_);

  DVLOG(2) << __func__ << ": Processing free-floating anchor creation requests";
  ProcessAnchorCreationRequestsHelper(
      mojo_from_viewer, input_state, &create_anchor_requests_, frame_time,
      [this](const CreateAnchorRequest& create_anchor_request,
             const gfx::Point3F& position, const gfx::Quaternion& orientation) {
        return anchor_manager_->CreateAnchor(
            device::mojom::Pose(orientation, position));
      });

  // Plane detection and anchors are separate features, we can't assume that
  // plane detection is enabled. If not, just skip this step.
  if (!plane_manager_)
    return;

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

    const mojom::XRNativeOriginInformation& native_origin_information =
        create_anchor.GetNativeOriginInformation();

    if (!NativeOriginExists(native_origin_information, input_state)) {
      DVLOG(3) << __func__
               << ": failing anchor creation request, native origin does not "
                  "exist";
      create_anchor.TakeCallback().Run(
          device::mojom::CreateAnchorResult::FAILURE, 0);
      continue;
    }

    std::optional<gfx::Transform> maybe_mojo_from_native_origin =
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

    std::optional<device::Pose> mojo_from_anchor =
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

    std::optional<AnchorId> maybe_anchor_id = std::forward<FunctionType>(
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
  DCHECK(anchor_manager_);
  anchor_manager_->DetachAnchor(AnchorId(anchor_id));
}

mojom::XRDepthDataPtr ArCoreImpl::GetDepthData() {
  DVLOG(3) << __func__;

  internal::ScopedArCoreObject<ArImage*> ar_image;
  ArStatus status = ArFrame_acquireDepthImage16Bits(
      arcore_session_.get(), arcore_frame_.get(),
      internal::ScopedArCoreObject<ArImage*>::Receiver(ar_image).get());

  if (status != AR_SUCCESS) {
    DVLOG(2) << __func__
             << ": ArFrame_acquireDepthImage failed, status=" << status;
    return nullptr;
  }

  int64_t timestamp_ns;
  ArImage_getTimestamp(arcore_session_.get(), ar_image.get(), &timestamp_ns);
  base::TimeDelta time_delta = base::Nanoseconds(timestamp_ns);
  DVLOG(3) << __func__ << ": depth image time_delta=" << time_delta;

  // The image returned from ArFrame_acquireDepthImage() is documented to have
  // a single 16-bit plane at index 0. The ArImage format is documented to be
  // AR_IMAGE_FORMAT_D_16 (equivalent to HardwareBuffer.D_16).
  // https://developers.google.com/ar/reference/c/group/ar-frame#arframe_acquiredepthimage16bits
  // https://developer.android.com/reference/android/hardware/HardwareBuffer#D_16

  ArImageFormat image_format;
  ArImage_getFormat(arcore_session_.get(), ar_image.get(), &image_format);

  CHECK_EQ(image_format, AR_IMAGE_FORMAT_D_16)
      << "Depth image format must be AR_IMAGE_FORMAT_D_16 ("
      << AR_IMAGE_FORMAT_D_16 << "), found: " << image_format;
  // AR_IMAGE_FORMAT_D_16 means 2 bytes per pixel.
  constexpr size_t kDepthPixelSize = 2;

  int32_t num_planes;
  ArImage_getNumberOfPlanes(arcore_session_.get(), ar_image.get(), &num_planes);

  CHECK_EQ(num_planes, 1) << "Depth image must have 1 plane, found: "
                          << num_planes;

  if (time_delta > previous_depth_data_time_) {
    // The depth data is more recent than what was previously returned, we need
    // to send the latest information back:
    mojom::XRDepthDataUpdatedPtr result = mojom::XRDepthDataUpdated::New();

    int32_t width = 0, height = 0;
    ArImage_getWidth(arcore_session_.get(), ar_image.get(), &width);
    ArImage_getHeight(arcore_session_.get(), ar_image.get(), &height);

    DVLOG(3) << __func__ << ": depth image dimensions=" << width << "x"
             << height;

    // The depth image is a width by height array of |kDepthPixelSize| elements:
    auto checked_buffer_size =
        base::CheckMul<size_t>(kDepthPixelSize, width, height);

    size_t buffer_size;
    if (!checked_buffer_size.AssignIfValid(&buffer_size)) {
      DVLOG(2) << __func__
               << ": overflow in kDepthPixelSize * width * height expression, "
                  "returning null depth data";
      return nullptr;
    }

    // Log a histogram w/ the number of entries in the depth buffer to make sure
    // we have a way of measuring the impact of the decision to suppress
    // too-high-resolution depth buffers. Assuming various common aspect ratios
    // & fixing the width to 160 pixels, the total number of pixels varies from
    // ~6000 to ~20000, and w/ the threshold below set to 43200 pixels, the
    // custom count from 5000 to 55000 with bucket size of 1000 should give us
    // sufficient granularity of data.
    UMA_HISTOGRAM_CUSTOM_COUNTS("XR.ARCore.DepthBufferSizeInPixels",
                                buffer_size / kDepthPixelSize, 5000, 55000, 50);

    TRACE_COUNTER2(TRACE_DISABLED_BY_DEFAULT("xr.debug"),
                   "Depth buffer resolution (in pixels)", "width", width,
                   "height", height);

    if (buffer_size / kDepthPixelSize > 240 * 180) {
      // ARCore should report depth data buffers w/ resolution in the ballpark
      // of 160x120. If the number of data entries is higher than 240 * 180
      // (=43200), we should not return it. The threshold was picked by
      // multiplying each expected dimension (160x120) by 1.5. Note that this
      // translates to 2.25 increase in allowed number of pixels compared to
      // the currently expected resolution.
      return nullptr;
    }

    mojo_base::BigBuffer pixels(buffer_size);

    // Interpret BigBuffer's data as a width by height array of uint16_t's and
    // copy image data into it:
    CopyArCoreImage(arcore_session_.get(), ar_image.get(), 0, pixels,
                    kDepthPixelSize, width, height);

    result->pixel_data = std::move(pixels);
    // Transform needed to consume the data:
    result->norm_texture_from_norm_view = GetDepthUvFromScreenUvTransform();
    result->size = gfx::Size(width, height);
    result->raw_value_to_meters =
        1.0 / 1000.0;  // DepthInMillimeters * 1/1000 = DepthInMeters

    DVLOG(3) << __func__ << ": norm_texture_from_norm_view=\n"
             << result->norm_texture_from_norm_view.ToString();

    previous_depth_data_time_ = time_delta;

    return mojom::XRDepthData::NewUpdatedDepthData(std::move(result));
  }

  // We don't have more recent data than what was already returned, inform the
  // caller that previously returned data is still valid:
  return mojom::XRDepthData::NewDataStillValid(
      mojom::XRDepthDataStillValid::New());
}

bool ArCoreImpl::IsOnGlThread() const {
  return gl_thread_task_runner_->BelongsToCurrentThread();
}

std::unique_ptr<ArCore> ArCoreImplFactory::Create() {
  return std::make_unique<ArCoreImpl>();
}

CreatePlaneAttachedAnchorRequest::CreatePlaneAttachedAnchorRequest(
    const mojom::XRNativeOriginInformation& native_origin_information,
    const gfx::Transform& native_origin_from_anchor,
    uint64_t plane_id,
    CreateAnchorCallback callback)
    : native_origin_information_(native_origin_information.Clone()),
      native_origin_from_anchor_(native_origin_from_anchor),
      plane_id_(plane_id),
      request_start_time_(base::TimeTicks::Now()),
      callback_(std::move(callback)) {}
CreatePlaneAttachedAnchorRequest::CreatePlaneAttachedAnchorRequest(
    CreatePlaneAttachedAnchorRequest&& other) = default;
CreatePlaneAttachedAnchorRequest::~CreatePlaneAttachedAnchorRequest() = default;

const mojom::XRNativeOriginInformation&
CreatePlaneAttachedAnchorRequest::GetNativeOriginInformation() const {
  return *native_origin_information_;
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

CreateAnchorCallback CreatePlaneAttachedAnchorRequest::TakeCallback() {
  return std::move(callback_);
}

}  // namespace device
