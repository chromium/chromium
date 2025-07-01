// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/numerics/safe_conversions.h"
#include "ppapi/proxy/camera_capabilities_resource.h"

namespace ppapi {
namespace proxy {

CameraCapabilitiesResource::CameraCapabilitiesResource(
    PP_Instance instance,
    const std::vector<PP_VideoCaptureFormat>& formats)
    : Resource(OBJECT_IS_PROXY, instance),
      num_video_capture_formats_(formats.size()),
      video_capture_formats_(
          new PP_VideoCaptureFormat[num_video_capture_formats_]) {
  std::copy(formats.begin(), formats.end(), video_capture_formats_.get());
}

CameraCapabilitiesResource::~CameraCapabilitiesResource() {
}

thunk::PPB_CameraCapabilities_API*
CameraCapabilitiesResource::AsPPB_CameraCapabilities_API() {
  return this;
}

void CameraCapabilitiesResource::GetSupportedVideoCaptureFormats(
    uint32_t* array_size,
    PP_VideoCaptureFormat** formats) {
  *array_size = base::checked_cast<uint32_t>(num_video_capture_formats_);
  *formats = video_capture_formats_.get();
}

}  // namespace proxy
}  // namespace ppapi
