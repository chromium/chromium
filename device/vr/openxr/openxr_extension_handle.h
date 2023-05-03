// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_VR_OPENXR_OPENXR_EXTENSION_HANDLE_H_
#define DEVICE_VR_OPENXR_OPENXR_EXTENSION_HANDLE_H_

#include "base/scoped_generic.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

template <typename HandleType>
struct OpenXrExtensionHandleTraits {
  using PFN_DestroyFunction = XrResult(XRAPI_PTR*)(HandleType);
  explicit OpenXrExtensionHandleTraits(
      const PFN_DestroyFunction destroy_function)
      : destroyer_(destroy_function) {}
  static HandleType InvalidValue() { return XR_NULL_HANDLE; }
  void Free(HandleType value) { destroyer_(value); }

 private:
  const PFN_DestroyFunction destroyer_;
};

template <typename HandleType>
using OpenXrExtensionHandle =
    base::ScopedGeneric<HandleType, OpenXrExtensionHandleTraits<HandleType>>;

}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_EXTENSION_HANDLE_H_
