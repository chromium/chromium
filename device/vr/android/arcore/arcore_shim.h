// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_ARCORE_ARCORE_SHIM_H_
#define DEVICE_VR_ANDROID_ARCORE_ARCORE_SHIM_H_

#include <string>

#include "base/component_export.h"

namespace device {

// TODO(crbug.com/41433109): add support for unloading the SDK.
COMPONENT_EXPORT(VR_ARCORE) bool LoadArCoreSdk(const std::string& libraryPath);

// Determines whether AR Core features are supported.
// TODO(crbug.com/41436902): Currently, this is very simplistic. It should
// consider whether the device can support ARCore.
// Calling this method won't load AR Core SDK and does not depend on AR Core SDK
// to be loaded.
// Returns true if the AR Core usage is supported, false otherwise.
COMPONENT_EXPORT(VR_ARCORE) bool IsArCoreSupported();

}  // namespace device

#endif  // DEVICE_VR_ANDROID_ARCORE_ARCORE_SHIM_H_
