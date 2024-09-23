// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/arcore/arcore_shim.h"

#include <dlfcn.h>

#include "base/android/android_hardware_buffer_compat.h"
#include "base/android/build_info.h"
#include "base/logging.h"
#include "device/vr/android/arcore/arcore_sdk.h"

namespace {

// Run DO macro for every function defined in the API.
#define FOR_EACH_API_FN(DO)                                        \
  DO(ArAnchor_detach)                                              \
  DO(ArAnchor_getPose)                                             \
  DO(ArAnchor_getTrackingState)                                    \
  DO(ArAnchor_release)                                             \
  DO(ArAnchorList_acquireItem)                                     \
  DO(ArAnchorList_create)                                          \
  DO(ArAnchorList_destroy)                                         \
  DO(ArAnchorList_getSize)                                         \
  DO(ArAugmentedImage_getCenterPose)                               \
  DO(ArAugmentedImage_getExtentX)                                  \
  DO(ArAugmentedImage_getIndex)                                    \
  DO(ArAugmentedImage_getTrackingMethod)                           \
  DO(ArAugmentedImageDatabase_addImageWithPhysicalSize)            \
  DO(ArAugmentedImageDatabase_create)                              \
  DO(ArAugmentedImageDatabase_destroy)                             \
  DO(ArAugmentedImageDatabase_getNumImages)                        \
  DO(ArCamera_getDisplayOrientedPose)                              \
  DO(ArCamera_getProjectionMatrix)                                 \
  DO(ArCamera_getTextureIntrinsics)                                \
  DO(ArCamera_getTrackingState)                                    \
  DO(ArCamera_getViewMatrix)                                       \
  DO(ArCameraConfig_create)                                        \
  DO(ArCameraConfig_destroy)                                       \
  DO(ArCameraConfig_getDepthSensorUsage)                           \
  DO(ArCameraConfig_getFacingDirection)                            \
  DO(ArCameraConfig_getFpsRange)                                   \
  DO(ArCameraConfig_getImageDimensions)                            \
  DO(ArCameraConfig_getTextureDimensions)                          \
  DO(ArCameraConfigFilter_create)                                  \
  DO(ArCameraConfigFilter_destroy)                                 \
  DO(ArCameraConfigFilter_setDepthSensorUsage)                     \
  DO(ArCameraConfigFilter_setFacingDirection)                      \
  DO(ArCameraConfigFilter_setTargetFps)                            \
  DO(ArCameraConfigList_create)                                    \
  DO(ArCameraConfigList_destroy)                                   \
  DO(ArCameraConfigList_getItem)                                   \
  DO(ArCameraConfigList_getSize)                                   \
  DO(ArCameraIntrinsics_create)                                    \
  DO(ArCameraIntrinsics_destroy)                                   \
  DO(ArCameraIntrinsics_getImageDimensions)                        \
  DO(ArConfig_create)                                              \
  DO(ArConfig_destroy)                                             \
  DO(ArConfig_getDepthMode)                                        \
  DO(ArConfig_getLightEstimationMode)                              \
  DO(ArConfig_setAugmentedImageDatabase)                           \
  DO(ArConfig_setDepthMode)                                        \
  DO(ArConfig_setFocusMode)                                        \
  DO(ArConfig_setLightEstimationMode)                              \
  DO(ArFrame_acquireCamera)                                        \
  DO(ArFrame_acquireDepthImage16Bits)                              \
  DO(ArFrame_create)                                               \
  DO(ArFrame_destroy)                                              \
  DO(ArFrame_getLightEstimate)                                     \
  DO(ArFrame_getTimestamp)                                         \
  DO(ArFrame_getUpdatedAnchors)                                    \
  DO(ArFrame_getUpdatedTrackables)                                 \
  DO(ArFrame_hitTestRay)                                           \
  DO(ArFrame_transformCoordinates2d)                               \
  DO(ArHitResult_acquireTrackable)                                 \
  DO(ArHitResult_create)                                           \
  DO(ArHitResult_destroy)                                          \
  DO(ArHitResult_getHitPose)                                       \
  DO(ArHitResultList_create)                                       \
  DO(ArHitResultList_destroy)                                      \
  DO(ArHitResultList_getItem)                                      \
  DO(ArHitResultList_getSize)                                      \
  DO(ArImage_getFormat)                                            \
  DO(ArImage_getHeight)                                            \
  DO(ArImage_getNumberOfPlanes)                                    \
  DO(ArImage_getPlaneData)                                         \
  DO(ArImage_getPlanePixelStride)                                  \
  DO(ArImage_getPlaneRowStride)                                    \
  DO(ArImage_getTimestamp)                                         \
  DO(ArImage_getWidth)                                             \
  DO(ArImage_release)                                              \
  DO(ArLightEstimate_acquireEnvironmentalHdrCubemap)               \
  DO(ArLightEstimate_create)                                       \
  DO(ArLightEstimate_destroy)                                      \
  DO(ArLightEstimate_getEnvironmentalHdrAmbientSphericalHarmonics) \
  DO(ArLightEstimate_getEnvironmentalHdrMainLightDirection)        \
  DO(ArLightEstimate_getEnvironmentalHdrMainLightIntensity)        \
  DO(ArLightEstimate_getState)                                     \
  DO(ArLightEstimate_getTimestamp)                                 \
  DO(ArPlane_acquireSubsumedBy)                                    \
  DO(ArPlane_getCenterPose)                                        \
  DO(ArPlane_getPolygon)                                           \
  DO(ArPlane_getPolygonSize)                                       \
  DO(ArPlane_getType)                                              \
  DO(ArPlane_isPoseInPolygon)                                      \
  DO(ArPose_create)                                                \
  DO(ArPose_destroy)                                               \
  DO(ArPose_getMatrix)                                             \
  DO(ArPose_getPoseRaw)                                            \
  DO(ArSession_acquireNewAnchor)                                   \
  DO(ArSession_configure)                                          \
  DO(ArSession_create)                                             \
  DO(ArSession_destroy)                                            \
  DO(ArSession_enableIncognitoMode_private)                        \
  DO(ArSession_getAllAnchors)                                      \
  DO(ArSession_getAllTrackables)                                   \
  DO(ArSession_getSupportedCameraConfigsWithFilter)                \
  DO(ArSession_pause)                                              \
  DO(ArSession_resume)                                             \
  DO(ArSession_setCameraConfig)                                    \
  DO(ArSession_setCameraTextureName)                               \
  DO(ArSession_setDisplayGeometry)                                 \
  DO(ArSession_update)                                             \
  DO(ArTrackable_acquireNewAnchor)                                 \
  DO(ArTrackable_getTrackingState)                                 \
  DO(ArTrackable_getType)                                          \
  DO(ArTrackable_release)                                          \
  DO(ArTrackableList_acquireItem)                                  \
  DO(ArTrackableList_create)                                       \
  DO(ArTrackableList_destroy)                                      \
  DO(ArTrackableList_getSize)

struct ArCoreApi {
  // Generate a function pointer field for every ArCore API function.
#define GEN_FN_PTR(fn) decltype(&fn) impl_##fn = nullptr;
  FOR_EACH_API_FN(GEN_FN_PTR)
#undef GEN_FN_PTR
};

template <typename Fn>
void LoadFunction(void* handle, const char* function_name, Fn* fn_out) {
  void* fn = dlsym(handle, function_name);
  if (!fn) {
    return;
  }

  *fn_out = reinterpret_cast<Fn>(fn);
}

void LoadArCoreApi(void* handle, ArCoreApi* api) {
  // Initialize each ArCoreApi function pointer field from the DLL
#define LOAD_FN_PTR(fn) LoadFunction(handle, #fn, &api->impl_##fn);
  FOR_EACH_API_FN(LOAD_FN_PTR)
#undef LOAD_FN_PTR
}

#undef FOR_EACH_API_FN

void* g_sdk_handle = nullptr;
ArCoreApi* g_arcore_api = nullptr;

}  // namespace

namespace device {

bool LoadArCoreSdk(const std::string& libraryPath) {
  if (g_arcore_api)
    return true;

  auto* sdk_handle = dlopen(libraryPath.c_str(), RTLD_GLOBAL | RTLD_NOW);
  if (!sdk_handle) {
    char* error_string = dlerror();
    LOG(ERROR) << "Could not open libarcore_sdk_c.so: " << error_string;
    return false;
  } else {
    VLOG(2) << "Opened shim shared library.";
  }

  // TODO(crbug.com/41431724): check SDK version.
  auto* arcore_api = new ArCoreApi();
  LoadArCoreApi(sdk_handle, arcore_api);

  g_sdk_handle = sdk_handle;
  g_arcore_api = arcore_api;
  return true;
}

bool IsArCoreSupported() {
  return base::android::BuildInfo::GetInstance()->sdk_int() >=
         base::android::SDK_VERSION_NOUGAT;
}

}  // namespace device

void ArAnchor_detach(ArSession* session, ArAnchor* anchor) {
  return g_arcore_api->impl_ArAnchor_detach(session, anchor);
}

void ArAnchor_getPose(const ArSession* session,
                      const ArAnchor* anchor,
                      ArPose* out_pose) {
  return g_arcore_api->impl_ArAnchor_getPose(session, anchor, out_pose);
}

void ArAnchor_getTrackingState(const ArSession* session,
                               const ArAnchor* anchor,
                               ArTrackingState* out_tracking_state) {
  return g_arcore_api->impl_ArAnchor_getTrackingState(session, anchor,
                                                      out_tracking_state);
}

void ArAnchor_release(ArAnchor* anchor) {
  g_arcore_api->impl_ArAnchor_release(anchor);
}

void ArAnchorList_acquireItem(const ArSession* session,
                              const ArAnchorList* anchor_list,
                              int32_t index,
                              ArAnchor** out_anchor) {
  return g_arcore_api->impl_ArAnchorList_acquireItem(session, anchor_list,
                                                     index, out_anchor);
}

void ArAnchorList_create(const ArSession* session,
                         ArAnchorList** out_anchor_list) {
  return g_arcore_api->impl_ArAnchorList_create(session, out_anchor_list);
}

void ArAnchorList_destroy(ArAnchorList* anchor_list) {
  g_arcore_api->impl_ArAnchorList_destroy(anchor_list);
}

void ArAnchorList_getSize(const ArSession* session,
                          const ArAnchorList* anchor_list,
                          int32_t* out_size) {
  return g_arcore_api->impl_ArAnchorList_getSize(session, anchor_list,
                                                 out_size);
}

void ArAugmentedImage_getCenterPose(const ArSession* session,
                                    const ArAugmentedImage* augmented_image,
                                    ArPose* out_pose) {
  g_arcore_api->impl_ArAugmentedImage_getCenterPose(session, augmented_image,
                                                    out_pose);
}

void ArAugmentedImage_getExtentX(const ArSession* session,
                                 const ArAugmentedImage* augmented_image,
                                 float* out_extent_x) {
  g_arcore_api->impl_ArAugmentedImage_getExtentX(session, augmented_image,
                                                 out_extent_x);
}

void ArAugmentedImage_getIndex(const ArSession* session,
                               const ArAugmentedImage* augmented_image,
                               int32_t* out_index) {
  g_arcore_api->impl_ArAugmentedImage_getIndex(session, augmented_image,
                                               out_index);
}

void ArAugmentedImage_getTrackingMethod(
    const ArSession* session,
    const ArAugmentedImage* image,
    ArAugmentedImageTrackingMethod* out_tracking_method) {
  return g_arcore_api->impl_ArAugmentedImage_getTrackingMethod(
      session, image, out_tracking_method);
}

ArStatus ArAugmentedImageDatabase_addImageWithPhysicalSize(
    const ArSession* session,
    ArAugmentedImageDatabase* augmented_image_database,
    const char* image_name,
    const uint8_t* image_grayscale_pixels,
    int32_t image_width_in_pixels,
    int32_t image_height_in_pixels,
    int32_t image_stride_in_pixels,
    float image_width_in_meters,
    int32_t* out_index) {
  return g_arcore_api->impl_ArAugmentedImageDatabase_addImageWithPhysicalSize(
      session, augmented_image_database, image_name, image_grayscale_pixels,
      image_width_in_pixels, image_height_in_pixels, image_stride_in_pixels,
      image_width_in_meters, out_index);
}

void ArAugmentedImageDatabase_create(
    const ArSession* session,
    ArAugmentedImageDatabase** out_augmented_image_database) {
  g_arcore_api->impl_ArAugmentedImageDatabase_create(
      session, out_augmented_image_database);
}

void ArAugmentedImageDatabase_destroy(
    ArAugmentedImageDatabase* augmented_image_database) {
  g_arcore_api->impl_ArAugmentedImageDatabase_destroy(augmented_image_database);
}

void ArAugmentedImageDatabase_getNumImages(
    const ArSession* session,
    const ArAugmentedImageDatabase* augmented_image_database,
    int32_t* out_number_of_images) {
  g_arcore_api->impl_ArAugmentedImageDatabase_getNumImages(
      session, augmented_image_database, out_number_of_images);
}

void ArCamera_getDisplayOrientedPose(const ArSession* session,
                                     const ArCamera* camera,
                                     ArPose* out_pose) {
  return g_arcore_api->impl_ArCamera_getDisplayOrientedPose(session, camera,
                                                            out_pose);
}

void ArCamera_getProjectionMatrix(const ArSession* session,
                                  const ArCamera* camera,
                                  float near,
                                  float far,
                                  float* dest_col_major_4x4) {
  return g_arcore_api->impl_ArCamera_getProjectionMatrix(
      session, camera, near, far, dest_col_major_4x4);
}

void ArCamera_getTextureIntrinsics(const ArSession* session,
                                   const ArCamera* camera,
                                   ArCameraIntrinsics* camera_intrinsics) {
  return g_arcore_api->impl_ArCamera_getTextureIntrinsics(session, camera,
                                                          camera_intrinsics);
}

void ArCamera_getTrackingState(const ArSession* session,
                               const ArCamera* camera,
                               ArTrackingState* out_tracking_state) {
  return g_arcore_api->impl_ArCamera_getTrackingState(session, camera,
                                                      out_tracking_state);
}

void ArCamera_getViewMatrix(const ArSession* session,
                            const ArCamera* camera,
                            float* out_matrix) {
  return g_arcore_api->impl_ArCamera_getViewMatrix(session, camera, out_matrix);
}

void ArCameraConfig_create(const ArSession* session,
                           ArCameraConfig** out_camera_config) {
  return g_arcore_api->impl_ArCameraConfig_create(session, out_camera_config);
}

void ArCameraConfig_destroy(ArCameraConfig* camera_config) {
  return g_arcore_api->impl_ArCameraConfig_destroy(camera_config);
}

void ArCameraConfig_getDepthSensorUsage(const ArSession* session,
                                        const ArCameraConfig* camera_config,
                                        uint32_t* out_depth_sensor_usage) {
  return g_arcore_api->impl_ArCameraConfig_getDepthSensorUsage(
      session, camera_config, out_depth_sensor_usage);
}

void ArCameraConfig_getFacingDirection(
    const ArSession* session,
    const ArCameraConfig* camera_config,
    ArCameraConfigFacingDirection* out_facing) {
  return g_arcore_api->impl_ArCameraConfig_getFacingDirection(
      session, camera_config, out_facing);
}

void ArCameraConfig_getFpsRange(const ArSession* session,
                                const ArCameraConfig* camera_config,
                                int32_t* out_min_fps,
                                int32_t* out_max_fps) {
  return g_arcore_api->impl_ArCameraConfig_getFpsRange(
      session, camera_config, out_min_fps, out_max_fps);
}

void ArCameraConfig_getImageDimensions(const ArSession* session,
                                       const ArCameraConfig* camera_config,
                                       int32_t* out_width,
                                       int32_t* out_height) {
  return g_arcore_api->impl_ArCameraConfig_getImageDimensions(
      session, camera_config, out_width, out_height);
}

void ArCameraConfig_getTextureDimensions(const ArSession* session,
                                         const ArCameraConfig* camera_config,
                                         int32_t* out_width,
                                         int32_t* out_height) {
  return g_arcore_api->impl_ArCameraConfig_getTextureDimensions(
      session, camera_config, out_width, out_height);
}

void ArCameraConfigFilter_create(const ArSession* session,
                                 ArCameraConfigFilter** out_filter) {
  return g_arcore_api->impl_ArCameraConfigFilter_create(session, out_filter);
}

void ArCameraConfigFilter_destroy(ArCameraConfigFilter* filter) {
  return g_arcore_api->impl_ArCameraConfigFilter_destroy(filter);
}

void ArCameraConfigFilter_setDepthSensorUsage(
    const ArSession* session,
    ArCameraConfigFilter* filter,
    uint32_t depth_sensor_usage_filters) {
  return g_arcore_api->impl_ArCameraConfigFilter_setDepthSensorUsage(
      session, filter, depth_sensor_usage_filters);
}

void ArCameraConfigFilter_setFacingDirection(
    const ArSession* session,
    ArCameraConfigFilter* filter,
    const ArCameraConfigFacingDirection direction) {
  return g_arcore_api->impl_ArCameraConfigFilter_setFacingDirection(
      session, filter, direction);
}

void ArCameraConfigFilter_setTargetFps(const ArSession* session,
                                       ArCameraConfigFilter* filter,
                                       const uint32_t fps_filters) {
  return g_arcore_api->impl_ArCameraConfigFilter_setTargetFps(session, filter,
                                                              fps_filters);
}

void ArCameraConfigList_create(const ArSession* session,
                               ArCameraConfigList** out_list) {
  return g_arcore_api->impl_ArCameraConfigList_create(session, out_list);
}

void ArCameraConfigList_destroy(ArCameraConfigList* list) {
  return g_arcore_api->impl_ArCameraConfigList_destroy(list);
}

void ArCameraConfigList_getItem(const ArSession* session,
                                const ArCameraConfigList* list,
                                int32_t index,
                                ArCameraConfig* out_camera_config) {
  return g_arcore_api->impl_ArCameraConfigList_getItem(session, list, index,
                                                       out_camera_config);
}

void ArCameraConfigList_getSize(const ArSession* session,
                                const ArCameraConfigList* list,
                                int32_t* out_size) {
  return g_arcore_api->impl_ArCameraConfigList_getSize(session, list, out_size);
}

void ArCameraIntrinsics_create(const ArSession* session,
                               ArCameraIntrinsics** out_camera_intrinsics) {
  return g_arcore_api->impl_ArCameraIntrinsics_create(session,
                                                      out_camera_intrinsics);
}

void ArCameraIntrinsics_destroy(ArCameraIntrinsics* camera_intrinsics) {
  return g_arcore_api->impl_ArCameraIntrinsics_destroy(camera_intrinsics);
}

void ArCameraIntrinsics_getImageDimensions(
    const ArSession* session,
    const ArCameraIntrinsics* camera_intrinsics,
    int32_t* out_width,
    int32_t* out_height) {
  return g_arcore_api->impl_ArCameraIntrinsics_getImageDimensions(
      session, camera_intrinsics, out_width, out_height);
}

void ArConfig_create(const ArSession* session, ArConfig** out_config) {
  return g_arcore_api->impl_ArConfig_create(session, out_config);
}

void ArConfig_destroy(ArConfig* config) {
  return g_arcore_api->impl_ArConfig_destroy(config);
}

void ArConfig_getDepthMode(const ArSession* session,
                           const ArConfig* config,
                           ArDepthMode* out_depth_mode) {
  return g_arcore_api->impl_ArConfig_getDepthMode(session, config,
                                                  out_depth_mode);
}

void ArConfig_getLightEstimationMode(
    const ArSession* session,
    const ArConfig* config,
    ArLightEstimationMode* light_estimation_mode) {
  return g_arcore_api->impl_ArConfig_getLightEstimationMode(
      session, config, light_estimation_mode);
}

void ArConfig_setAugmentedImageDatabase(
    const ArSession* session,
    ArConfig* config,
    const ArAugmentedImageDatabase* augmented_image_database) {
  g_arcore_api->impl_ArConfig_setAugmentedImageDatabase(
      session, config, augmented_image_database);
}

void ArConfig_setDepthMode(const ArSession* session,
                           ArConfig* config,
                           ArDepthMode mode) {
  return g_arcore_api->impl_ArConfig_setDepthMode(session, config, mode);
}

void ArConfig_setFocusMode(const ArSession* session,
                           ArConfig* config,
                           ArFocusMode focus_mode) {
  g_arcore_api->impl_ArConfig_setFocusMode(session, config, focus_mode);
}

void ArConfig_setLightEstimationMode(
    const ArSession* session,
    ArConfig* config,
    ArLightEstimationMode light_estimation_mode) {
  return g_arcore_api->impl_ArConfig_setLightEstimationMode(
      session, config, light_estimation_mode);
}

void ArFrame_acquireCamera(const ArSession* session,
                           const ArFrame* frame,
                           ArCamera** out_camera) {
  return g_arcore_api->impl_ArFrame_acquireCamera(session, frame, out_camera);
}

ArStatus ArFrame_acquireDepthImage16Bits(const ArSession* session,
                                         const ArFrame* frame,
                                         ArImage** out_depth_image) {
  return g_arcore_api->impl_ArFrame_acquireDepthImage16Bits(session, frame,
                                                            out_depth_image);
}

void ArFrame_create(const ArSession* session, ArFrame** out_frame) {
  return g_arcore_api->impl_ArFrame_create(session, out_frame);
}

void ArFrame_destroy(ArFrame* frame) {
  return g_arcore_api->impl_ArFrame_destroy(frame);
}

void ArFrame_getLightEstimate(const ArSession* session,
                              const ArFrame* frame,
                              ArLightEstimate* out_light_estimate) {
  return g_arcore_api->impl_ArFrame_getLightEstimate(session, frame,
                                                     out_light_estimate);
}

void ArFrame_getTimestamp(const ArSession* session,
                          const ArFrame* frame,
                          int64_t* out_timestamp_ns) {
  return g_arcore_api->impl_ArFrame_getTimestamp(session, frame,
                                                 out_timestamp_ns);
}

void ArFrame_getUpdatedAnchors(const ArSession* session,
                               const ArFrame* frame,
                               ArAnchorList* out_anchor_list) {
  return g_arcore_api->impl_ArFrame_getUpdatedAnchors(session, frame,
                                                      out_anchor_list);
}

void ArFrame_getUpdatedTrackables(const ArSession* session,
                                  const ArFrame* frame,
                                  ArTrackableType filter_type,
                                  ArTrackableList* out_trackable_list) {
  return g_arcore_api->impl_ArFrame_getUpdatedTrackables(
      session, frame, filter_type, out_trackable_list);
}

void ArFrame_hitTestRay(const ArSession* session,
                        const ArFrame* frame,
                        const float* ray_origin_3,
                        const float* ray_direction_3,
                        ArHitResultList* out_hit_results) {
  return g_arcore_api->impl_ArFrame_hitTestRay(
      session, frame, ray_origin_3, ray_direction_3, out_hit_results);
}

void ArFrame_transformCoordinates2d(const ArSession* session,
                                    const ArFrame* frame,
                                    ArCoordinates2dType input_coordinates,
                                    int32_t number_of_vertices,
                                    const float* vertices_2d,
                                    ArCoordinates2dType output_coordinates,
                                    float* out_vertices_2d) {
  return g_arcore_api->impl_ArFrame_transformCoordinates2d(
      session, frame, input_coordinates, number_of_vertices, vertices_2d,
      output_coordinates, out_vertices_2d);
}

void ArHitResult_create(const ArSession* session,
                        ArHitResult** out_hit_result) {
  return g_arcore_api->impl_ArHitResult_create(session, out_hit_result);
}

void ArHitResult_destroy(ArHitResult* hit_result) {
  return g_arcore_api->impl_ArHitResult_destroy(hit_result);
}

void ArHitResult_getHitPose(const ArSession* session,
                            const ArHitResult* hit_result,
                            ArPose* out_pose) {
  return g_arcore_api->impl_ArHitResult_getHitPose(session, hit_result,
                                                   out_pose);
}

void ArHitResult_acquireTrackable(const ArSession* session,
                                  const ArHitResult* hit_result,
                                  ArTrackable** out_trackable) {
  return g_arcore_api->impl_ArHitResult_acquireTrackable(session, hit_result,
                                                         out_trackable);
}

void ArHitResultList_create(const ArSession* session,
                            ArHitResultList** out_hit_result_list) {
  return g_arcore_api->impl_ArHitResultList_create(session,
                                                   out_hit_result_list);
}

void ArHitResultList_destroy(ArHitResultList* hit_result_list) {
  return g_arcore_api->impl_ArHitResultList_destroy(hit_result_list);
}

void ArHitResultList_getItem(const ArSession* session,
                             const ArHitResultList* hit_result_list,
                             int index,
                             ArHitResult* out_hit_result) {
  return g_arcore_api->impl_ArHitResultList_getItem(session, hit_result_list,
                                                    index, out_hit_result);
}

void ArHitResultList_getSize(const ArSession* session,
                             const ArHitResultList* hit_result_list,
                             int* out_size) {
  return g_arcore_api->impl_ArHitResultList_getSize(session, hit_result_list,
                                                    out_size);
}

void ArImage_getFormat(const ArSession* session,
                       const ArImage* image,
                       ArImageFormat* out_format) {
  return g_arcore_api->impl_ArImage_getFormat(session, image, out_format);
}

void ArImage_getHeight(const ArSession* session,
                       const ArImage* image,
                       int32_t* out_height) {
  return g_arcore_api->impl_ArImage_getHeight(session, image, out_height);
}

void ArImage_getNumberOfPlanes(const ArSession* session,
                               const ArImage* image,
                               int32_t* out_num_planes) {
  return g_arcore_api->impl_ArImage_getNumberOfPlanes(session, image,
                                                      out_num_planes);
}

void ArImage_getPlaneData(const ArSession* session,
                          const ArImage* image,
                          int32_t plane_index,
                          const uint8_t** out_data,
                          int32_t* out_data_length) {
  return g_arcore_api->impl_ArImage_getPlaneData(session, image, plane_index,
                                                 out_data, out_data_length);
}

void ArImage_getPlanePixelStride(const ArSession* session,
                                 const ArImage* image,
                                 int32_t plane_index,
                                 int32_t* out_pixel_stride) {
  return g_arcore_api->impl_ArImage_getPlanePixelStride(
      session, image, plane_index, out_pixel_stride);
}

void ArImage_getPlaneRowStride(const ArSession* session,
                               const ArImage* image,
                               int32_t plane_index,
                               int32_t* out_row_stride) {
  return g_arcore_api->impl_ArImage_getPlaneRowStride(
      session, image, plane_index, out_row_stride);
}

void ArImage_getTimestamp(const ArSession* session,
                          const ArImage* image,
                          int64_t* out_timestamp_ns) {
  return g_arcore_api->impl_ArImage_getTimestamp(session, image,
                                                 out_timestamp_ns);
}

void ArImage_getWidth(const ArSession* session,
                      const ArImage* image,
                      int32_t* out_width) {
  return g_arcore_api->impl_ArImage_getWidth(session, image, out_width);
}

void ArImage_release(ArImage* image) {
  return g_arcore_api->impl_ArImage_release(image);
}

void ArLightEstimate_acquireEnvironmentalHdrCubemap(
    const ArSession* session,
    const ArLightEstimate* light_estimate,
    ArImageCubemap out_textures_6) {
  return g_arcore_api->impl_ArLightEstimate_acquireEnvironmentalHdrCubemap(
      session, light_estimate, out_textures_6);
}

void ArLightEstimate_create(const ArSession* session,
                            ArLightEstimate** out_light_estimate) {
  return g_arcore_api->impl_ArLightEstimate_create(session, out_light_estimate);
}

void ArLightEstimate_destroy(ArLightEstimate* light_estimate) {
  return g_arcore_api->impl_ArLightEstimate_destroy(light_estimate);
}

void ArLightEstimate_getEnvironmentalHdrAmbientSphericalHarmonics(
    const ArSession* session,
    const ArLightEstimate* light_estimate,
    float* out_coefficients_27) {
  return g_arcore_api
      ->impl_ArLightEstimate_getEnvironmentalHdrAmbientSphericalHarmonics(
          session, light_estimate, out_coefficients_27);
}

void ArLightEstimate_getEnvironmentalHdrMainLightDirection(
    const ArSession* session,
    const ArLightEstimate* light_estimate,
    float* out_direction_3) {
  return g_arcore_api
      ->impl_ArLightEstimate_getEnvironmentalHdrMainLightDirection(
          session, light_estimate, out_direction_3);
}

void ArLightEstimate_getEnvironmentalHdrMainLightIntensity(
    const ArSession* session,
    const ArLightEstimate* light_estimate,
    float* out_intensity_3) {
  return g_arcore_api
      ->impl_ArLightEstimate_getEnvironmentalHdrMainLightIntensity(
          session, light_estimate, out_intensity_3);
}

void ArLightEstimate_getState(const ArSession* session,
                              const ArLightEstimate* light_estimate,
                              ArLightEstimateState* out_light_estimate_state) {
  return g_arcore_api->impl_ArLightEstimate_getState(session, light_estimate,
                                                     out_light_estimate_state);
}

void ArLightEstimate_getTimestamp(const ArSession* session,
                                  const ArLightEstimate* light_estimate,
                                  int64_t* out_timestamp_ns) {
  return g_arcore_api->impl_ArLightEstimate_getTimestamp(
      session, light_estimate, out_timestamp_ns);
}

void ArPlane_acquireSubsumedBy(const ArSession* session,
                               const ArPlane* plane,
                               ArPlane** out_subsumed_by) {
  return g_arcore_api->impl_ArPlane_acquireSubsumedBy(session, plane,
                                                      out_subsumed_by);
}

void ArPlane_getCenterPose(const ArSession* session,
                           const ArPlane* plane,
                           ArPose* out_pose) {
  return g_arcore_api->impl_ArPlane_getCenterPose(session, plane, out_pose);
}

void ArPlane_getPolygon(const ArSession* session,
                        const ArPlane* plane,
                        float* out_polygon_xz) {
  return g_arcore_api->impl_ArPlane_getPolygon(session, plane, out_polygon_xz);
}

void ArPlane_getPolygonSize(const ArSession* session,
                            const ArPlane* plane,
                            int32_t* out_polygon_size) {
  return g_arcore_api->impl_ArPlane_getPolygonSize(session, plane,
                                                   out_polygon_size);
}

void ArPlane_getType(const ArSession* session,
                     const ArPlane* plane,
                     ArPlaneType* out_plane_type) {
  return g_arcore_api->impl_ArPlane_getType(session, plane, out_plane_type);
}

void ArPlane_isPoseInPolygon(const ArSession* session,
                             const ArPlane* plane,
                             const ArPose* pose,
                             int32_t* out_pose_in_polygon) {
  return g_arcore_api->impl_ArPlane_isPoseInPolygon(session, plane, pose,
                                                    out_pose_in_polygon);
}

void ArPose_create(const ArSession* session,
                   const float* pose_raw,
                   ArPose** out_pose) {
  return g_arcore_api->impl_ArPose_create(session, pose_raw, out_pose);
}

void ArPose_destroy(ArPose* pose) {
  return g_arcore_api->impl_ArPose_destroy(pose);
}

void ArPose_getMatrix(const ArSession* session,
                      const ArPose* pose,
                      float* out_matrix) {
  return g_arcore_api->impl_ArPose_getMatrix(session, pose, out_matrix);
}

void ArPose_getPoseRaw(const ArSession* session,
                       const ArPose* pose,
                       float* out_pose_raw) {
  return g_arcore_api->impl_ArPose_getPoseRaw(session, pose, out_pose_raw);
}

ArStatus ArSession_acquireNewAnchor(ArSession* session,
                                    const ArPose* pose,
                                    ArAnchor** out_anchor) {
  return g_arcore_api->impl_ArSession_acquireNewAnchor(session, pose,
                                                       out_anchor);
}

ArStatus ArSession_configure(ArSession* session, const ArConfig* config) {
  return g_arcore_api->impl_ArSession_configure(session, config);
}

ArStatus ArSession_create(void* env,
                          void* application_context,
                          ArSession** out_session_pointer) {
  return g_arcore_api->impl_ArSession_create(env, application_context,
                                             out_session_pointer);
}

void ArSession_destroy(ArSession* session) {
  return g_arcore_api->impl_ArSession_destroy(session);
}

void ArSession_enableIncognitoMode_private(ArSession* session) {
  return g_arcore_api->impl_ArSession_enableIncognitoMode_private(session);
}

void ArSession_getAllAnchors(const ArSession* session,
                             ArAnchorList* out_anchor_list) {
  return g_arcore_api->impl_ArSession_getAllAnchors(session, out_anchor_list);
}

void ArSession_getAllTrackables(const ArSession* session,
                                ArTrackableType filter_type,
                                ArTrackableList* out_trackable_list) {
  return g_arcore_api->impl_ArSession_getAllTrackables(session, filter_type,
                                                       out_trackable_list);
}

void ArSession_getSupportedCameraConfigsWithFilter(
    const ArSession* session,
    const ArCameraConfigFilter* filter,
    ArCameraConfigList* list) {
  return g_arcore_api->impl_ArSession_getSupportedCameraConfigsWithFilter(
      session, filter, list);
}

ArStatus ArSession_pause(ArSession* session) {
  return g_arcore_api->impl_ArSession_pause(session);
}

ArStatus ArSession_resume(ArSession* session) {
  return g_arcore_api->impl_ArSession_resume(session);
}

ArStatus ArSession_setCameraConfig(const ArSession* session,
                                   const ArCameraConfig* camera_config) {
  return g_arcore_api->impl_ArSession_setCameraConfig(session, camera_config);
}

void ArSession_setCameraTextureName(ArSession* session, uint32_t texture_id) {
  return g_arcore_api->impl_ArSession_setCameraTextureName(session, texture_id);
}

void ArSession_setDisplayGeometry(ArSession* session,
                                  int32_t rotation,
                                  int32_t width,
                                  int32_t height) {
  return g_arcore_api->impl_ArSession_setDisplayGeometry(session, rotation,
                                                         width, height);
}

ArStatus ArSession_update(ArSession* session, ArFrame* out_frame) {
  return g_arcore_api->impl_ArSession_update(session, out_frame);
}

ArStatus ArTrackable_acquireNewAnchor(ArSession* session,
                                      ArTrackable* trackable,
                                      ArPose* pose,
                                      ArAnchor** out_anchor) {
  return g_arcore_api->impl_ArTrackable_acquireNewAnchor(session, trackable,
                                                         pose, out_anchor);
}

void ArTrackable_getTrackingState(const ArSession* session,
                                  const ArTrackable* trackable,
                                  ArTrackingState* out_tracking_state) {
  return g_arcore_api->impl_ArTrackable_getTrackingState(session, trackable,
                                                         out_tracking_state);
}

void ArTrackable_getType(const ArSession* session,
                         const ArTrackable* trackable,
                         ArTrackableType* out_trackable_type) {
  return g_arcore_api->impl_ArTrackable_getType(session, trackable,
                                                out_trackable_type);
}

void ArTrackable_release(ArTrackable* trackable) {
  return g_arcore_api->impl_ArTrackable_release(trackable);
}

void ArTrackableList_acquireItem(const ArSession* session,
                                 const ArTrackableList* trackable_list,
                                 int32_t index,
                                 ArTrackable** out_trackable) {
  return g_arcore_api->impl_ArTrackableList_acquireItem(session, trackable_list,
                                                        index, out_trackable);
}

void ArTrackableList_create(const ArSession* session,
                            ArTrackableList** out_trackable_list) {
  return g_arcore_api->impl_ArTrackableList_create(session, out_trackable_list);
}

void ArTrackableList_destroy(ArTrackableList* trackable_list) {
  return g_arcore_api->impl_ArTrackableList_destroy(trackable_list);
}

void ArTrackableList_getSize(const ArSession* session,
                             const ArTrackableList* trackable_list,
                             int32_t* out_size) {
  return g_arcore_api->impl_ArTrackableList_getSize(session, trackable_list,
                                                    out_size);
}
