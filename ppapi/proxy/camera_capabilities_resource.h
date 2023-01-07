// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_CAMERA_CAPABILITIES_RESOURCE_H_
#define PPAPI_PROXY_CAMERA_CAPABILITIES_RESOURCE_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/ppb_camera_capabilities_api.h"

namespace ppapi {
namespace proxy {

class PPAPI_PROXY_EXPORT CameraCapabilitiesResource
    : public Resource,
      public thunk::PPB_CameraCapabilities_API {
 public:
  CameraCapabilitiesResource(PP_Instance instance,
                             const std::vector<PP_VideoCaptureFormat>& formats);

  CameraCapabilitiesResource(const CameraCapabilitiesResource&) = delete;
  CameraCapabilitiesResource& operator=(const CameraCapabilitiesResource&) =
      delete;

  ~CameraCapabilitiesResource() override;

  // Resource overrides.
  thunk::PPB_CameraCapabilities_API* AsPPB_CameraCapabilities_API() override;

  // PPB_CameraCapabilities_API implementation.
  void GetSupportedVideoCaptureFormats(
      uint32_t* array_size,
      PP_VideoCaptureFormat** formats) override;

 private:
  size_t num_video_capture_formats_;
  std::unique_ptr<PP_VideoCaptureFormat[]> video_capture_formats_;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_CAMERA_CAPABILITIES_RESOURCE_H_
