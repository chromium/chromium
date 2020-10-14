// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_EXTENSION_HELPER_H_
#define DEVICE_VR_OPENXR_OPENXR_EXTENSION_HELPER_H_

#include <d3d11.h>
#include <vector>

#include "base/logging.h"
#include "third_party/openxr/src/include/openxr/openxr.h"
#include "third_party/openxr/src/include/openxr/openxr_platform.h"

namespace device {
struct OpenXrExtensionMethods {
  PFN_xrGetD3D11GraphicsRequirementsKHR xrGetD3D11GraphicsRequirementsKHR{
      nullptr};
};

class OpenXrExtensionEnumeration {
 public:
  OpenXrExtensionEnumeration();
  ~OpenXrExtensionEnumeration();

  bool ExtensionSupported(const char* extension_name) const;

 private:
  std::vector<XrExtensionProperties> extension_properties_;
};

class OpenXrExtensionHelper {
 public:
  OpenXrExtensionHelper(
      XrInstance instance,
      const OpenXrExtensionEnumeration* const extension_enumeration);
  ~OpenXrExtensionHelper();

  const OpenXrExtensionEnumeration* ExtensionEnumeration() const {
    return extension_enumeration_;
  }

  const OpenXrExtensionMethods& ExtensionMethods() const {
    return extension_methods_;
  }

 private:
  const OpenXrExtensionMethods extension_methods_;
  const OpenXrExtensionEnumeration* const extension_enumeration_;
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_EXTENSION_HELPER_H_
