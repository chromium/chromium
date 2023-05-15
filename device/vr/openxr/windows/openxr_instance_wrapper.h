// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_WINDOWS_OPENXR_INSTANCE_WRAPPER_H_
#define DEVICE_VR_OPENXR_WINDOWS_OPENXR_INSTANCE_WRAPPER_H_

#include "base/memory/singleton.h"
#include "device/vr/vr_export.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

// Unfortunately on Windows, unloading the OpenXR runtime DLLs exposes shutdown
// bugs in runtimes that fail to stop running code when the instance is
// destroyed.
//
// To work around these bugs, we treat the OpenXr Instance as a leaky singleton,
// which is managed/accessed via this wrapper class.
// This prevents unnecessary loading/unloading of the DLLs after every
// navigation since the lifetime of the OpenXR statics is now tied to the
// lifetime of the utility process, rather than the lifetime of the WebContents
// that use WebXR. The singleton needs to be leaky as the default behavior would
// destruct the singleton during process teardown, which is under a loader lock.
// All leaks from the OpenXR instance should be inproc leaks though, which means
// that we aren't actually leaking anything. The utility process is recycled
// after 5 seconds of no WebXR activity, so we aren't keeping the object around
// for too much longer than it is actually needed.
class DEVICE_VR_EXPORT OpenXrInstanceWrapper {
 public:
  static OpenXrInstanceWrapper* GetWrapper();

  OpenXrInstanceWrapper(const OpenXrInstanceWrapper&) = delete;
  OpenXrInstanceWrapper& operator=(const OpenXrInstanceWrapper&) = delete;

  bool HasXrInstance();
  void SetXrInstance(XrInstance instance);
  XrInstance GetXrInstance();

 private:
  OpenXrInstanceWrapper() = default;
  ~OpenXrInstanceWrapper() = default;

  friend struct base::DefaultSingletonTraits<OpenXrInstanceWrapper>;
  XrInstance instance_ = XR_NULL_HANDLE;
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_WINDOWS_OPENXR_INSTANCE_WRAPPER_H_
