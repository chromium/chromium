// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_SCOPED_OPENXR_OBJECT_H_
#define DEVICE_VR_OPENXR_SCOPED_OPENXR_OBJECT_H_

#include "base/memory/raw_ref.h"
#include "device/vr/openxr/openxr_extension_helper.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

// ScopedOpenXrObject is a wrapper for OpenXR handles that ensures proper
// cleanup. It is similar in spirit to base::ScopedGeneric, but is necessary
// because the OpenXR object destruction functions are not simple free
// functions. Instead, they are function pointers retrieved from the OpenXR
// runtime at initialization and must be accessed via the OpenXrExtensionHelper.
template <typename T>
class ScopedOpenXrObject {
 public:
  explicit ScopedOpenXrObject(const OpenXrExtensionHelper& extension_helper,
                              T object = XR_NULL_HANDLE)
      : extension_helper_(extension_helper), object_(object) {}

  ~ScopedOpenXrObject() { Free(); }

  ScopedOpenXrObject(const ScopedOpenXrObject&) = delete;
  ScopedOpenXrObject& operator=(const ScopedOpenXrObject&) = delete;

  const T& get() const { return object_; }

  void Reset(T object = XR_NULL_HANDLE) {
    Free();
    object_ = object;
  }

  // Releases the currently stored value and allows overwriting it with a new
  // one.
  T* receive() {
    Reset();
    return &object_;
  }

 private:
  // This must be specialized for each handle type, so a general implementation
  // is not provided.
  void Free();

  const raw_ref<const OpenXrExtensionHelper> extension_helper_;
  T object_;
};

// Template specializations for Free() must be provided for each type.
template <>
inline void ScopedOpenXrObject<XrSpatialSnapshotEXT>::Free() {
  if (object_ != XR_NULL_HANDLE) {
    extension_helper_->ExtensionMethods().xrDestroySpatialSnapshotEXT(object_);
  }
}

}  // namespace device

#endif  // DEVICE_VR_OPENXR_SCOPED_OPENXR_OBJECT_H_
