// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/utils/extension_types_utils.h"
#include "extensions/common/api/extension_types.h"
#include "extensions/common/mojom/execution_world.mojom-shared.h"

namespace extensions {

mojom::RunLocation ConvertRunLocation(api::extension_types::RunAt run_at) {
  switch (run_at) {
    case api::extension_types::RunAt::kDocumentEnd:
      return mojom::RunLocation::kDocumentEnd;
    case api::extension_types::RunAt::kNone:
    case api::extension_types::RunAt::kDocumentIdle:
      return mojom::RunLocation::kDocumentIdle;
    case api::extension_types::RunAt::kDocumentStart:
      return mojom::RunLocation::kDocumentStart;
  }

  NOTREACHED_IN_MIGRATION();
  return mojom::RunLocation::kDocumentIdle;
}

api::extension_types::RunAt ConvertRunLocationForAPI(
    mojom::RunLocation run_at) {
  // api::extension_types does not have analogues for kUndefined, kRunDeferred
  // or kBrowserDriven. We don't expect to encounter them here.
  switch (run_at) {
    case mojom::RunLocation::kDocumentEnd:
      return api::extension_types::RunAt::kDocumentEnd;
    case mojom::RunLocation::kDocumentStart:
      return api::extension_types::RunAt::kDocumentStart;
    case mojom::RunLocation::kDocumentIdle:
      return api::extension_types::RunAt::kDocumentIdle;
    case mojom::RunLocation::kUndefined:
    case mojom::RunLocation::kRunDeferred:
    case mojom::RunLocation::kBrowserDriven:
      break;
  }

  NOTREACHED_IN_MIGRATION();
  return api::extension_types::RunAt::kDocumentIdle;
}

mojom::ExecutionWorld ConvertExecutionWorld(
    api::extension_types::ExecutionWorld world) {
  mojom::ExecutionWorld execution_world = mojom::ExecutionWorld::kIsolated;
  switch (world) {
    case api::extension_types::ExecutionWorld::kNone:
    case api::extension_types::ExecutionWorld::kIsolated:
      break;  // Default to mojom::ExecutionWorld::kIsolated.
    case api::extension_types::ExecutionWorld::kMain:
      execution_world = mojom::ExecutionWorld::kMain;
      break;
    case api::extension_types::ExecutionWorld::kUserScript:
      execution_world = mojom::ExecutionWorld::kUserScript;
  }

  return execution_world;
}

api::extension_types::ExecutionWorld ConvertExecutionWorldForAPI(
    mojom::ExecutionWorld world) {
  switch (world) {
    case mojom::ExecutionWorld::kIsolated:
      return api::extension_types::ExecutionWorld::kIsolated;
    case mojom::ExecutionWorld::kMain:
      return api::extension_types::ExecutionWorld::kMain;
    case mojom::ExecutionWorld::kUserScript:
      return api::extension_types::ExecutionWorld::kUserScript;
  }

  NOTREACHED_IN_MIGRATION();
  return api::extension_types::ExecutionWorld::kIsolated;
}

}  // namespace extensions
