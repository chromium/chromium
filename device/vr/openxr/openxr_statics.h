// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_STATICS_H_
#define DEVICE_VR_OPENXR_OPENXR_STATICS_H_

#include <d3d11.h>

#include "base/memory/singleton.h"
#include "build/build_config.h"
#include "device/vr/openxr/openxr_platform.h"
#include "device/vr/openxr/openxr_util.h"
#include "device/vr/vr_export.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_types.h"
#endif

namespace device {

// OpenXRStatics manages the lifetime of the OpenXR instance, and hence, the
// entire OpenXR runtime. Unfortunately, unloading OpenXR runtime DLLs exposes
// shutdown bugs in runtimes that fail to stop running code when the instance is
// destroyed.
//
// To work around these bugs, we create the OpenXR statics as a leaky singleton.
// This prevents unnecessary loading/unloading of the DLLs after every
// navigation since the lifetime of the OpenXR statics is now tied to the
// lifetime of the utility process, rather than the lifetime of the WebContents
// that use WebXR. The singleton needs to be leaky as the default behavior would
// destruct the singleton during process teardown, which is under a loader lock.
// All leaks from the OpenXR instance should be inproc leaks though, which means
// that we aren't actually leaking anything. The utility process is recycled
// after 5 seconds of no WebXR activity, so we aren't keeping the object around
// for too much longer than it is actually needed.
class DEVICE_VR_EXPORT OpenXrStatics {
 public:
  static OpenXrStatics* GetInstance();

  const OpenXrExtensionEnumeration* GetExtensionEnumeration() const {
    return &extension_enumeration_;
  }

  XrInstance GetXrInstance();

  bool IsHardwareAvailable();
  bool IsApiAvailable();

#if BUILDFLAG(IS_WIN)
  CHROME_LUID GetLuid(const OpenXrExtensionHelper& extension_helper);
#endif

 private:
  OpenXrStatics();

  OpenXrStatics(const OpenXrStatics&) = delete;
  OpenXrStatics& operator=(const OpenXrStatics&) = delete;

  ~OpenXrStatics() = default;
  friend struct base::DefaultSingletonTraits<OpenXrStatics>;

  XrInstance instance_;
  OpenXrExtensionEnumeration extension_enumeration_;
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_STATICS_H_
