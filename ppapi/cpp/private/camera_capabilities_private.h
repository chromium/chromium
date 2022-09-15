/* Copyright 2014 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef PPAPI_CPP_PRIVATE_CAMERA_CAPABILITIES_PRIVATE_H_
#define PPAPI_CPP_PRIVATE_CAMERA_CAPABILITIES_PRIVATE_H_

#include <vector>

#include "ppapi/c/private/ppb_camera_capabilities_private.h"
#include "ppapi/cpp/resource.h"
#include "ppapi/cpp/size.h"

/// @file
/// This file defines the CameraCapabilities_Private interface for
/// establishing an image capture configuration resource within the browser.
namespace pp {

/// The <code>CameraCapabilities_Private</code> interface contains methods for
/// getting the image capture capabilities within the browser.
class CameraCapabilities_Private : public Resource {
 public:
  /// Default constructor for creating an is_null()
  /// <code>CameraCapabilities_Private</code> object.
  CameraCapabilities_Private();

  /// The copy constructor for <code>CameraCapabilities_Private</code>.
  ///
  /// @param[in] other A reference to a <code>CameraCapabilities_Private
  /// </code>.
  CameraCapabilities_Private(const CameraCapabilities_Private& other);

  /// Constructs a <code>CameraCapabilities_Private</code> from a <code>
  /// Resource</code>.
  ///
  /// @param[in] resource A <code>PPB_CameraCapabilities_Private</code>
  /// resource.
  explicit CameraCapabilities_Private(const Resource& resource);

  /// A constructor used when you have received a <code>PP_Resource</code> as a
  /// return value that has had 1 ref added for you.
  ///
  /// @param[in] resource A <code>PPB_CameraCapabilities_Private</code>
  /// resource.
  CameraCapabilities_Private(PassRef, PP_Resource resource);

  // Destructor.
  ~CameraCapabilities_Private();

  /// GetSupportedVideoCaptureFormats() returns the supported video capture
  /// formats.
  ///
  /// @param[out] formats A vector of <code>PP_VideoCaptureFormat</code>
  /// corresponding to the supported video capture formats. This output vector
  /// must be prepared by the caller beforehand.
  void GetSupportedVideoCaptureFormats(
      std::vector<PP_VideoCaptureFormat>* formats);

  /// IsCameraCapabilities() determines if the given resource is a
  /// <code>CameraCapabilities_Private</code>.
  ///
  /// @param[in] resource A <code>Resource</code> corresponding to an image
  /// capture capabilities resource.
  ///
  /// @return true if the given resource is an <code>
  /// CameraCapabilities_Private</code> resource, otherwise false.
  static bool IsCameraCapabilities(const Resource& resource);
};

}  // namespace pp

#endif  // PPAPI_CPP_PRIVATE_CAMERA_CAPABILITIES_PRIVATE_H_
