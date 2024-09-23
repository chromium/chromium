// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "device/vr/openxr/android/openxr_depth_sensor_android.h"

#include <array>
#include <memory>
#include <set>

#include "base/containers/contains.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/flat_set.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "device/vr/openxr/openxr_extension_helper.h"
#include "device/vr/openxr/openxr_util.h"
#include "device/vr/openxr/openxr_view_configuration.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/public/mojom/xr_session.mojom.h"
#include "third_party/openxr/dev/xr_android.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

namespace {
// The spec essentially requires that the depth views line up with the
// XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO type, off of which we define these
// constants. This provides an extra layer of security in case we migrate types
// or anything else that this class is kept up-to-date.
static_assert(kNumPrimaryViews == 2);
static_assert(kLeftView == 0);
static_assert(kRightView == 1);

// Returns the index in the |XrDepthAcquireResultANDROID| |views| member for the
// requested eye per the specification.
size_t GetDepthViewIndex(const mojom::XREye& eye) {
  CHECK(eye == mojom::XREye::kLeft || eye == mojom::XREye::kRight);
  if (eye == mojom::XREye::kLeft) {
    return kLeftView;
  }

  return kRightView;
}

// The depthImage from OpenXR consists of two sets of pixels one after the
// other. The first num_pixels floats in depthImage are the left eye, and the
// pixels after that are the right eye.
size_t GetDepthImageOffset(const mojom::XREye& eye, size_t pixels_per_image) {
  CHECK(eye == mojom::XREye::kLeft || eye == mojom::XREye::kRight);
  if (eye == mojom::XREye::kRight) {
    return pixels_per_image;
  }

  return 0;
}

// A map of the resolutions that we support to a gfx::Size, since we ultimately
// need to send that across mojom, and allows us to properly handle whichever
// size we get back from the API.
constexpr auto kResolutionSizeMap =
    base::MakeFixedFlatMap<XrDepthCameraResolutionANDROID, gfx::Size>(
        {{XR_DEPTH_CAMERA_RESOLUTION_80x80_ANDROID, {80, 80}},
         {XR_DEPTH_CAMERA_RESOLUTION_160x160_ANDROID, {160, 160}},
         {XR_DEPTH_CAMERA_RESOLUTION_320x320_ANDROID, {320, 320}}});

constexpr std::array<XrDepthCameraResolutionANDROID, 3> kResolutionPreferences{
    XR_DEPTH_CAMERA_RESOLUTION_320x320_ANDROID,
    XR_DEPTH_CAMERA_RESOLUTION_160x160_ANDROID,
    XR_DEPTH_CAMERA_RESOLUTION_80x80_ANDROID};
static_assert(kResolutionSizeMap.size() == kResolutionPreferences.size(),
              "Need to have a corresponding resolution size for every "
              "preferred resolution that we can support");

constexpr size_t GetByteSize(const mojom::XRDepthDataFormat& format) {
  switch (format) {
    case mojom::XRDepthDataFormat::kLuminanceAlpha:
    case mojom::XRDepthDataFormat::kUnsignedShort:
      return sizeof(uint16_t);
    case mojom::XRDepthDataFormat::kFloat32:
      return sizeof(float);
  }
}
static_assert(sizeof(uint16_t) ==
              GetByteSize(mojom::XRDepthDataFormat::kLuminanceAlpha));
static_assert(sizeof(uint16_t) ==
              GetByteSize(mojom::XRDepthDataFormat::kUnsignedShort));

// Essentially this returns the projection matrix for a given camera. Screen
// coordinates appear to need to be in clip space, e.g. [-1,1]. "Camera Space",
// conforms to space expectations compatible with other transforms used
// throughout the runtime and references a space with the camera location as the
// origin.
gfx::Transform GetScreenFromCamera(const mojom::VRFieldOfViewPtr& fov) {
  constexpr float near_depth = 0.0001;
  constexpr float far_depth = 10000;
  constexpr double kDegToRad = M_PI / 180.0;

  float up_rad = fov->up_degrees * kDegToRad;
  float down_rad = fov->down_degrees * kDegToRad;
  float left_rad = fov->left_degrees * kDegToRad;
  float right_rad = fov->right_degrees * kDegToRad;

  float up_tan = tanf(up_rad);
  float down_tan = tanf(down_rad);
  float left_tan = tanf(left_rad);
  float right_tan = tanf(right_rad);
  float x_scale = 2.0f / (left_tan + right_tan);
  float y_scale = 2.0f / (up_tan + down_tan);
  float inv_nf = 1.0f / (near_depth - far_depth);

  return gfx::Transform::ColMajor(
      x_scale, 0.0f, 0.0f, 0.0f, 0.0f, y_scale, 0.0f, 0.0f,
      -((left_tan - right_tan) * x_scale * 0.5),
      ((up_tan - down_tan) * y_scale * 0.5), (near_depth + far_depth) * inv_nf,
      -1.0f, 0.0f, 0.0f, (2.0f * far_depth * near_depth) * inv_nf, 0.0f);
}

// Converts an array coordinate value [0,size) to a texture coordinate [0, 1].
inline float ToTexCoord(float val, float size) {
  return (val + 0.5f) / size;
}

// Converts a texture coordinate [0,1] to "clip space" [-1, 1]. This is a
// necessary conversion when transforming a point through a projection matrix
// (screen_from_foo or foo_from_screen) in our normal terminology.
inline float ToClipSpace(float val) {
  return 2.0f * val - 1.0f;
}

// Converts from "clip space" [-1, 1] to texture coordinate space [0,1]. This is
// a necessary conversion to map a point transformed through a projection matrix
// back to something that can be used to sample a texture.
inline float FromClipSpace(float val) {
  return (val + 1.0f) / 2.0f;
}

inline size_t buffer_location(size_t col, size_t row, size_t row_size) {
  return row * row_size + col;
}

template <typename T>
inline void WriteToSpanStart(base::span<uint8_t> output, T val) {
  output.first<sizeof(T)>().copy_from(base::byte_span_from_ref(val));
}

// Helper function to copy depth data on the CPU. This expects to receive the
// raw array of data received from the OpenXr API and will convert it to an
// array of the same size. This function is responsible for mapping a point from
// the "pixel" it would occupy in the output buffer to sample the corresponding
// point in the depth buffer by applying all required transforms. After the
// float value is sampled, it will apply |conversion_fn| to map from float to
// |T| to assign it to the output array.
template <typename T, typename FunctionType>
void CopyDepthData(base::span<const float> input,
                   base::span<uint8_t> output,
                   gfx::Size image_size,
                   XrDepthViewANDROID depth_view,
                   const mojom::XRViewPtr& view,
                   FunctionType&& conversion_fn) {
  // We should've handled an invalid image_size before getting to this point.
  size_t num_pixels;
  CHECK(image_size.GetCheckedArea().AssignIfValid(&num_pixels));
  CHECK_EQ(input.size(), num_pixels);
  CHECK_EQ(output.size_bytes(), num_pixels * sizeof(T));

  // Extract width/height for readability (and to use size_t).
  const size_t width = image_size.width();
  const size_t height = image_size.height();
  const gfx::Transform view_from_eye_screen =
      GetScreenFromCamera(view->field_of_view).GetCheckedInverse();
  const gfx::Transform depth_screen_from_depth =
      GetScreenFromCamera(XrFovToMojomFov(depth_view.fov));

  // Depth pose is initially local_from_depth (based on passing local space
  // into the object upon creation).
  // TOOD(crbug.com/40684534): Create local_from_mojom transformations.
  const gfx::Transform local_from_mojom;
  const auto depth_from_mojom =
      XrPoseToGfxTransform(depth_view.pose).GetCheckedInverse() *
      local_from_mojom;
  const auto& mojom_from_view = view->mojo_from_view;
  const gfx::Transform depth_screen_from_eye_screen =
      depth_screen_from_depth * depth_from_mojom * mojom_from_view *
      view_from_eye_screen;
  for (size_t y = 0; y < height; y++) {
    for (size_t x = 0; x < width; x++) {
      // Assign a z value of 1 to convert from cartesian (screen) coordinates to
      // a homogeneous Euclidean (2D) coordinate space.
      const gfx::Point3F eye_screen_clip_coord{
          ToClipSpace(ToTexCoord(x, width)), ToClipSpace(ToTexCoord(y, height)),
          1};
      const gfx::Point3F depth_screen_clip_coord =
          depth_screen_from_eye_screen.MapPoint(eye_screen_clip_coord);

      const gfx::PointF depth_screen_texture_coord(
          FromClipSpace(depth_screen_clip_coord.x()),
          FromClipSpace(depth_screen_clip_coord.y()));

      // If x or y is less than 0 it's out of bounds and we should ignore it.
      // We'll convert back to whole buffer coordinates before checking the
      // width and height.
      if (depth_screen_texture_coord.x() < 0 ||
          depth_screen_texture_coord.y() < 0) {
        // We need to ensure that the whole span gets initialized.
        WriteToSpanStart(output, T());
        // Advance the span so that the start is the next uninitialized spot.
        output = output.subspan(sizeof(T));
        continue;
      }

      const gfx::PointF depth_screen_buffer_coord =
          gfx::ScalePoint(depth_screen_texture_coord, width, height);

      // We've already verified that these values can't be negative, so we can
      // safely convert to size_t now.
      // Anything from N.0 to N.999... should be treated as belonging to the
      // pixel originating at N. The previous addition of 0.5 helped to ensure
      // accuracy by forcing us to sample the value that the middle of the pixel
      // should be, as such it would be inappropriate to subtract the 0.5 again
      // as that might force us to sample a different pixel than where our
      // centerpoint should be. This static_cast from float to size_t
      // essentially is equivalent to truncation to leave us with N.
      const size_t depth_y = static_cast<size_t>(depth_screen_buffer_coord.y());
      const size_t depth_x = static_cast<size_t>(depth_screen_buffer_coord.x());

      // If the new point is out of bounds, ignore it.
      // Note that we do this part of the bounds check after the conversion from
      // float to size_t to ensure accuracy of the conversion.
      if (depth_x >= width || depth_y >= height) {
        // We need to ensure that the whole span gets initialized.
        WriteToSpanStart(output, T());
        // Advance the span so that the start is the next uninitialized spot.
        output = output.subspan(sizeof(T));
        continue;
      }

      float depth_value = input[buffer_location(depth_x, depth_y, width)];

      // The continuous `subspan` calls will essentially keep advancing output
      // through the underlying data structure for the span so that the first
      // sizeof(T) bytes are also the next unwritten bytes and correspond to
      // our current x/y "spot".
      WriteToSpanStart(output, conversion_fn(depth_value));

      // Advance the span so that the start is the next uninitialized spot.
      output = output.subspan(sizeof(T));
    }
  }

  // Since we've been advancing the span the whole time and already verified
  // that the originally passed in output span is the same size as the input, we
  // should now be at the end of the span we received, which means that output
  // should be empty.
  CHECK(output.empty());
}
}  // namespace

OpenXrDepthSensorAndroid::OpenXrDepthSensorAndroid(
    const OpenXrExtensionHelper& extension_helper,
    XrSession session,
    XrSpace mojo_space,
    const mojom::XRDepthOptions& depth_options)
    : extension_helper_(extension_helper),
      session_(session),
      mojo_space_(mojo_space) {
  DVLOG(1) << __func__;
  // We can only support CPU optimized depth, so we can only support depth if
  // either no preferences were specified or if cpu-optimized was specified.
  const auto& usage_preferences = depth_options.usage_preferences;
  const bool can_support_depth =
      usage_preferences.empty() ||
      base::Contains(usage_preferences, mojom::XRDepthUsage::kCPUOptimized);

  if (can_support_depth) {
    depth_config_ = mojom::XRDepthConfig::New();
    depth_config_->depth_usage = mojom::XRDepthUsage::kCPUOptimized;

    // We can support all of the current data formats, so just grab the first if
    // they were specified, and if none were, use float32 (our native type).
    static_assert(static_cast<int>(mojom::XRDepthDataFormat::kMaxValue) == 3);
    if (!depth_options.data_format_preferences.empty()) {
      depth_config_->depth_data_format =
          depth_options.data_format_preferences[0];
    } else {
      depth_config_->depth_data_format = mojom::XRDepthDataFormat::kFloat32;
    }
  } else {
    DVLOG(1) << __func__ << " Cannot support depth";
  }
}

OpenXrDepthSensorAndroid::~OpenXrDepthSensorAndroid() {
  DVLOG(1) << __func__;
  if (swapchain_ != XR_NULL_HANDLE) {
    // In the (likely) event that the session has been destroyed before us, this
    // will fail. So just ignore the result returned here.
    extension_helper_->ExtensionMethods().xrDestroyDepthSwapchainANDROID(
        swapchain_);

    swapchain_ = XR_NULL_HANDLE;
  }

  depth_images_.clear();
}

XrResult OpenXrDepthSensorAndroid::Initialize() {
  DVLOG(1) << __func__;
  if (initialized_) {
    return XR_SUCCESS;
  }

  if (!depth_config_) {
    return XR_ERROR_FEATURE_UNSUPPORTED;
  }

  uint32_t supported_resolutions_count;
  RETURN_IF_XR_FAILED(
      extension_helper_->ExtensionMethods().xrEnumerateDepthResolutionsANDROID(
          session_, 0, &supported_resolutions_count, nullptr));

  std::vector<XrDepthCameraResolutionANDROID> supported_resolutions(
      supported_resolutions_count, XR_DEPTH_CAMERA_RESOLUTION_MAX_ENUM_ANDROID);
  RETURN_IF_XR_FAILED(
      extension_helper_->ExtensionMethods().xrEnumerateDepthResolutionsANDROID(
          session_, supported_resolutions_count, &supported_resolutions_count,
          supported_resolutions.data()));

  // Realistically this should never happen, but since it theoretically can,
  // it shouldn't be a CHECK.
  if (supported_resolutions_count != supported_resolutions.size()) {
    LOG(ERROR) << __func__
               << " Supported resolution size changed during creation";
    return XR_ERROR_INITIALIZATION_FAILED;
  }

  auto it = base::ranges::find_if(
      kResolutionPreferences.begin(), kResolutionPreferences.end(),
      [&supported_resolutions](
          const XrDepthCameraResolutionANDROID& resolution) {
        return base::Contains(supported_resolutions, resolution);
      });

  if (it == kResolutionPreferences.end()) {
    DLOG(ERROR) << __func__ << " No Supported Depth Resolution";
    return XR_ERROR_INITIALIZATION_FAILED;
  }

  depth_camera_resolution_ = *it;

  XrDepthSwapchainCreateInfoANDROID swapchain_create_info{
      XR_TYPE_DEPTH_SWAPCHAIN_CREATE_INFO_ANDROID};
  swapchain_create_info.resolution = depth_camera_resolution_;
  swapchain_create_info.createFlags =
      XR_DEPTH_SWAPCHAIN_CREATE_RAW_DEPTH_IMAGE_BIT_ANDROID;
  RETURN_IF_XR_FAILED(
      extension_helper_->ExtensionMethods().xrCreateDepthSwapchainANDROID(
          session_, &swapchain_create_info, &swapchain_));

  uint32_t image_count_output = 0;
  RETURN_IF_XR_FAILED(extension_helper_->ExtensionMethods()
                          .xrEnumerateDepthSwapchainImagesANDROID(
                              swapchain_, 0, &image_count_output, nullptr));

  depth_images_.resize(image_count_output);
  for (auto& image : depth_images_) {
    image.type = XR_TYPE_DEPTH_SWAPCHAIN_IMAGE_ANDROID;
  }

  RETURN_IF_XR_FAILED(extension_helper_->ExtensionMethods()
                          .xrEnumerateDepthSwapchainImagesANDROID(
                              swapchain_, depth_images_.size(),
                              &image_count_output, depth_images_.data()));

  // Realistically this should never happen, but since it theoretically can,
  // it shouldn't be a CHECK.
  if (image_count_output != depth_images_.size()) {
    LOG(ERROR) << __func__ << " Swapchain size changed during creation";
    return XR_ERROR_INITIALIZATION_FAILED;
  }

  initialized_ = true;
  return XR_SUCCESS;
}

mojom::XRDepthConfigPtr OpenXrDepthSensorAndroid::GetDepthConfig() {
  return depth_config_ ? depth_config_.Clone() : nullptr;
}

void OpenXrDepthSensorAndroid::PopulateDepthData(
    XrTime frame_time,
    const std::vector<mojom::XRViewPtr>& views) {
  DVLOG(3) << __func__;
  // We could fail to be initialized if depth isn't actually supported.
  if (!initialized_) {
    DVLOG(3) << __func__ << " Not initialized";
    return;
  }

  if (views.size() < kNumPrimaryViews ||
      views[kLeftView]->eye != mojom::XREye::kLeft ||
      views[kRightView]->eye != mojom::XREye::kRight) {
    DLOG(ERROR) << __func__ << " Incorrect eye configuration";
    return;
  }

  XrDepthAcquireInfoANDROID acquire_info = {XR_TYPE_DEPTH_ACQUIRE_INFO_ANDROID};
  acquire_info.space = mojo_space_;
  acquire_info.displayTime = frame_time;

  XrDepthAcquireResultANDROID acquire_result = {
      XR_TYPE_DEPTH_ACQUIRE_RESULT_ANDROID};
  XrResult result = extension_helper_->ExtensionMethods()
                        .xrAcquireDepthSwapchainImagesANDROID(
                            swapchain_, &acquire_info, &acquire_result);
  if (XR_FAILED(result)) {
    DLOG(ERROR) << __func__
                << " Failed to acquire depth swapchain images: " << result;
    return;
  }

  if (acquire_result.acquiredIndex >= depth_images_.size()) {
    DLOG(ERROR) << __func__ << " Acquired Index was out of bounds: "
                << acquire_result.acquiredIndex << " vs "
                << depth_images_.size();
    return;
  }

  for (size_t i = 0; i < kNumPrimaryViews; i++) {
    views[i]->depth_data = GetDepthDataForEye(acquire_result, views[i]);
  }
}

mojom::XRDepthDataPtr OpenXrDepthSensorAndroid::GetDepthDataForEye(
    const XrDepthAcquireResultANDROID& acquire_result,
    const mojom::XRViewPtr& view) {
  const auto& eye = view->eye;
  DVLOG(3) << __func__ << " eye: " << eye;
  CHECK(eye == mojom::XREye::kLeft || eye == mojom::XREye::kRight);
  auto& depth_image = depth_images_[acquire_result.acquiredIndex];

  const auto& image_size = kResolutionSizeMap.at(depth_camera_resolution_);
  size_t num_pixels;
  if (!image_size.GetCheckedArea().AssignIfValid(&num_pixels)) {
    DLOG(ERROR) << __func__ << " Image size overflowed";
    return nullptr;
  }

  const auto& data_format = depth_config_->depth_data_format;
  size_t buffer_size;
  if (!base::CheckMul<size_t>(GetByteSize(data_format), num_pixels)
           .AssignIfValid(&buffer_size)) {
    DLOG(ERROR) << __func__ << " Buffer size overflowed";
    return nullptr;
  }

  XrDepthViewANDROID depth_view = acquire_result.views[GetDepthViewIndex(eye)];
  size_t pixel_offset = GetDepthImageOffset(eye, num_pixels);
  base::span<const float> raw_depth_image =
      base::span(depth_image.rawDepthImage + pixel_offset, num_pixels);

  mojom::XRDepthDataUpdatedPtr result = mojom::XRDepthDataUpdated::New();
  mojo_base::BigBuffer pixels(buffer_size);
  switch (depth_config_->depth_data_format) {
    case mojom::XRDepthDataFormat::kFloat32:
      // Results are already in meters.
      CHECK(GetByteSize(data_format) == sizeof(float));
      CopyDepthData<float>(raw_depth_image, pixels, image_size, depth_view,
                           view, [](float val) { return val; });
      break;
    // Luminance alpha needs to be converted
    case mojom::XRDepthDataFormat::kLuminanceAlpha:
    case mojom::XRDepthDataFormat::kUnsignedShort:
      // We'll be converting to millimeters.
      result->raw_value_to_meters = 1 / 1000.0f;

      CHECK(GetByteSize(data_format) == sizeof(uint16_t));
      CopyDepthData<uint16_t>(
          raw_depth_image, pixels, image_size, depth_view, view, [](float val) {
            // val is in meters, so convert to mm to avoid losing precision.
            return base::saturated_cast<uint16_t>(std::nearbyint(val * 1000));
          });
      break;
  }

  result->pixel_data = std::move(pixels);
  result->size = image_size;
  return mojom::XRDepthData::NewUpdatedDepthData(std::move(result));
}

OpenXrDepthSensorAndroidFactory::OpenXrDepthSensorAndroidFactory() = default;
OpenXrDepthSensorAndroidFactory::~OpenXrDepthSensorAndroidFactory() = default;

const base::flat_set<std::string_view>&
OpenXrDepthSensorAndroidFactory::GetRequestedExtensions() const {
  static base::NoDestructor<base::flat_set<std::string_view>> kExtensions(
      {XR_ANDROID_DEPTH_TEXTURE_EXTENSION_NAME});
  return *kExtensions;
}

std::set<device::mojom::XRSessionFeature>
OpenXrDepthSensorAndroidFactory::GetSupportedFeatures(
    const OpenXrExtensionEnumeration* extension_enum) const {
  if (!IsEnabled(extension_enum)) {
    return {};
  }

  return {device::mojom::XRSessionFeature::DEPTH};
}

void OpenXrDepthSensorAndroidFactory::ProcessSystemProperties(
    const OpenXrExtensionEnumeration* extension_enum,
    XrInstance instance,
    XrSystemId system) {
  XrSystemDepthTrackingPropertiesANDROID depth_properties{
      XR_TYPE_SYSTEM_DEPTH_TRACKING_PROPERTIES_ANDROID};

  XrSystemProperties system_properties{XR_TYPE_SYSTEM_PROPERTIES};
  system_properties.next = &depth_properties;

  bool depth_supported = false;
  XrResult result = xrGetSystemProperties(instance, system, &system_properties);
  if (XR_SUCCEEDED(result)) {
    depth_supported = depth_properties.supportsDepthTracking;
  }

  SetSystemPropertiesSupport(depth_supported);
}

std::unique_ptr<OpenXrDepthSensor>
OpenXrDepthSensorAndroidFactory::CreateDepthSensor(
    const OpenXrExtensionHelper& extension_helper,
    XrSession session,
    XrSpace mojo_space,
    const mojom::XRDepthOptions& depth_options) const {
  bool is_supported = IsEnabled(extension_helper.ExtensionEnumeration());
  DVLOG(2) << __func__ << " is_supported=" << is_supported;
  if (is_supported) {
    return std::make_unique<OpenXrDepthSensorAndroid>(
        extension_helper, session, mojo_space, depth_options);
  }

  return nullptr;
}

}  // namespace device
