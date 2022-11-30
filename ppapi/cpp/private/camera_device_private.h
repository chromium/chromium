/* Copyright 2014 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef PPAPI_CPP_PRIVATE_CAMERA_DEVICE_PRIVATE_H_
#define PPAPI_CPP_PRIVATE_CAMERA_DEVICE_PRIVATE_H_

#include <stdint.h>

#include "ppapi/c/private/ppb_camera_device_private.h"
#include "ppapi/cpp/resource.h"
#include "ppapi/cpp/var.h"

/// @file
/// Defines the <code>CameraDevice_Private</code> interface. Used for
/// manipulating a camera device.
namespace pp {

class CameraCapabilities_Private;
class CompletionCallback;
class InstanceHandle;

template <typename T>
class CompletionCallbackWithOutput;

/// To query camera capabilities:
/// 1. Create a CameraDevice_Private object.
/// 2. Open() camera device with track id of MediaStream video track.
/// 3. Call GetCameraCapabilities() to get a
///    <code>CameraCapabilities_Private</code> object, which can be used to
///    query camera capabilities.
class CameraDevice_Private : public Resource {
 public:
  /// Default constructor for creating an is_null()
  /// <code>CameraDevice_Private</code> object.
  CameraDevice_Private();

  /// The copy constructor for <code>CameraDevice_Private</code>.
  ///
  /// @param[in] other A reference to a <code>CameraDevice_Private</code>.
  CameraDevice_Private(const CameraDevice_Private& other);

  /// Constructs a <code>CameraDevice_Private</code> from a
  /// <code>Resource</code>.
  ///
  /// @param[in] resource A <code>PPB_CameraDevice_Private</code> resource.
  explicit CameraDevice_Private(const Resource& resource);

  /// Constructs a CameraDevice_Private resource.
  ///
  /// @param[in] instance A <code>PP_Instance</code> identifying one instance
  /// of a module.
  explicit CameraDevice_Private(const InstanceHandle& instance);

  /// A constructor used when you have received a <code>PP_Resource</code> as a
  /// return value that has had 1 ref added for you.
  ///
  /// @param[in] resource A <code>PPB_CameraDevice_Private</code> resource.
  CameraDevice_Private(PassRef, PP_Resource resource);

  // Destructor.
  ~CameraDevice_Private();

  /// Opens a camera device.
  ///
  /// @param[in] device_id A <code>Var</code> identifying a camera
  /// device. The type is string. The ID can be obtained from
  /// navigator.mediaDevices.enumerateDevices() or MediaStreamVideoTrack.id.
  /// @param[in] callback A <code>CompletionCallback</code> to be called upon
  /// completion of <code>Open()</code>.
  ///
  /// @return An int32_t containing a result code from <code>pp_errors.h</code>.
  int32_t Open(const Var& device_id, const CompletionCallback& callback);

  /// Disconnects from the camera and cancels all pending requests.
  /// After this returns, no callbacks will be called. If <code>
  /// CameraDevice_Private</code> is destroyed and is not closed yet, this
  /// function will be automatically called. Calling this more than once has no
  /// effect.
  void Close();

  /// Gets the camera capabilities.
  ///
  /// The camera capabilities do not change for a given camera source.
  ///
  /// @param[in] callback A <code>CompletionCallbackWithOutput</code>
  /// to be called upon completion.
  ///
  /// @return An int32_t containing a result code from <code>pp_errors.h</code>.
  int32_t GetCameraCapabilities(
      const CompletionCallbackWithOutput<CameraCapabilities_Private>& callback);

  /// Determines if a resource is a camera device resource.
  ///
  /// @param[in] resource The <code>Resource</code> to test.
  ///
  /// @return true if the given resource is a camera device resource or false
  /// otherwise.
  static bool IsCameraDevice(const Resource& resource);
};

} // namespace pp

#endif  // PPAPI_CPP_PRIVATE_CAMERA_DEVICE_PRIVATE_H_
