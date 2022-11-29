// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/test/vaapi_device.h"

#include <fcntl.h>
#include <va/va_drm.h>
#include <xf86drm.h>

#include "base/files/file.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "media/gpu/vaapi/test/macros.h"

namespace media {
namespace vaapi_test {

base::File FindDrmNode() {
  constexpr char kRenderNodeFilePattern[] = "/dev/dri/renderD%d";
  // This loop ends on either the first card that does not exist or the first
  // render node that is not vgem.
  for (int i = 128;; i++) {
    base::FilePath dev_path(FILE_PATH_LITERAL(
        base::StringPrintf(kRenderNodeFilePattern, i).c_str()));
    base::File drm_file =
        base::File(dev_path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                                 base::File::FLAG_WRITE);
    if (!drm_file.IsValid())
      return drm_file;
    // Skip the virtual graphics memory manager device.
    drmVersionPtr version = drmGetVersion(drm_file.GetPlatformFile());
    if (!version)
      continue;
    std::string version_name(
        version->name,
        base::checked_cast<std::string::size_type>(version->name_len));
    drmFreeVersion(version);
    if (base::EqualsCaseInsensitiveASCII(version_name, "vgem"))
      continue;
    return drm_file;
  }
}

VaapiDevice::VaapiDevice() : display_(nullptr) {
  display_file_ = FindDrmNode();
  LOG_ASSERT(display_file_.IsValid()) << "Failed to determine DRM render node";

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
