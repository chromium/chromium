// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_EXTENSION_HANDLER_FACTORIES_H_
#define DEVICE_VR_OPENXR_OPENXR_EXTENSION_HANDLER_FACTORIES_H_

#include <memory>
#include <vector>

namespace device {
class OpenXrExtensionHandlerFactory;

// Returns an ordered list of OpenXrExtensionHandlerFactory objects that can
// be used to support various features that are enabled via extensions. See
// `OpenXrExtensionHandlerFactory` for full details about factory usage.
const std::vector<OpenXrExtensionHandlerFactory*>&
GetExtensionHandlerFactories();
}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_EXTENSION_HANDLER_FACTORIES_H_
