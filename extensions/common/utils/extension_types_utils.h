// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_UTILS_EXTENSION_TYPES_UTILS_H_
#define EXTENSIONS_COMMON_UTILS_EXTENSION_TYPES_UTILS_H_

#include "extensions/common/api/extension_types.h"
#include "extensions/common/mojom/execution_world.mojom-shared.h"
#include "extensions/common/mojom/run_location.mojom-shared.h"

// Contains helper methods for converting from extension_types

namespace extensions {

// Converts api::extension_types::RunAt to mojom::RunLocation.
mojom::RunLocation ConvertRunLocation(api::extension_types::RunAt run_at);

// Converts mojom::RunLocation to api::extension_types::RunAt.
api::extension_types::RunAt ConvertRunLocationForAPI(mojom::RunLocation run_at);

mojom::ExecutionWorld ConvertExecutionWorld(
    api::extension_types::ExecutionWorld world);

api::extension_types::ExecutionWorld ConvertExecutionWorldForAPI(
    mojom::ExecutionWorld world);

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_UTILS_EXTENSION_TYPES_UTILS_H_
