// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_HOST_EXECUTION_PROVIDER_INITIALIZER_H_
#define SERVICES_WEBNN_HOST_EXECUTION_PROVIDER_INITIALIZER_H_

#include <string>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "services/webnn/public/mojom/ep_package_info.mojom.h"

namespace base {
class CommandLine;
}  // namespace base

namespace webnn {

// Adds the EPs specified by `switches::kWebNNOrtEpLibraryPathForTesting` in
// `command_line` to `ep_package_info_map`, overriding any existing entries with
// the same EP names. Nothing is added if any entry of the switch is invalid.
COMPONENT_EXPORT(WEBNN_HOST)
void AddTestingEpsFromCommandLine(
    const base::CommandLine& command_line,
    base::flat_map<std::string, mojom::EpPackageInfoPtr>& ep_package_info_map);

// Initializes the execution providers (EPs) used by the WebNN ONNX Runtime
// backend and retrieves their package info after they get ready. This call will
// trigger the installation of the EPs that are supported on the platform if
// they are not present.
COMPONENT_EXPORT(WEBNN_HOST)
void EnsureExecutionProvidersReady(
    base::OnceCallback<
        void(base::flat_map<std::string, mojom::EpPackageInfoPtr>)> callback);

}  // namespace webnn

#endif  // SERVICES_WEBNN_HOST_EXECUTION_PROVIDER_INITIALIZER_H_
