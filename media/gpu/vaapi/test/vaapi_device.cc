// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <va/va.h>
#include <va/va_drm.h>

#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "media/gpu/vaapi/test/macros.h"
#include "media/gpu/vaapi/test/vaapi_device.h"

namespace media {
namespace vaapi_test {

VaapiDevice::VaapiDevice() : display_(nullptr) {
  constexpr char kDriRenderNode0Path[] = "/dev/dri/renderD128";
  display_file_ = base::File(
      base::FilePath::FromUTF8Unsafe(kDriRenderNode0Path),
      base::File::FLAG_OPEN | base::File::FLAG_READ | base::File::FLAG_WRITE);
  LOG_ASSERT(display_file_.IsValid())
      << "Failed to open " << kDriRenderNode0Path;

  display_ = vaGetDisplayDRM(display_file_.GetPlatformFile());
  LOG_ASSERT(display_ != nullptr) << "vaGetDisplayDRM failed";

  int major, minor;
  const VAStatus res = vaInitialize(display_, &major, &minor);
  VA_LOG_ASSERT(res, "vaInitialize");
  VLOG(1) << "VA major version: " << major << ", minor version: " << minor;
}

VaapiDevice::~VaapiDevice() {
  VLOG(1) << "Tearing down...";
  const VAStatus res = vaTerminate(display_);
  VA_LOG_ASSERT(res, "vaTerminate");

  VLOG(1) << "Teardown done.";
}

}  // namespace vaapi_test
}  // namespace media
