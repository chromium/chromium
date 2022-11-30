// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/extension_types_utils.h"

namespace extensions {

mojom::RunLocation ConvertRunLocation(api::extension_types::RunAt run_at) {
  switch (run_at) {
    case api::extension_types::RUN_AT_DOCUMENT_END:
      return mojom::RunLocation::kDocumentEnd;
    case api::extension_types::RUN_AT_NONE:
    case api::extension_types::RUN_AT_DOCUMENT_IDLE:
      return mojom::RunLocation::kDocumentIdle;
    case api::extension_types::RUN_AT_DOCUMENT_START:
      return mojom::RunLocation::kDocumentStart;
  }

  NOTREACHED();
  return mojom::RunLocation::kDocumentIdle;
}

api::extension_types::RunAt ConvertRunLocationForAPI(
    mojom::RunLocation run_at) {
  // api::extension_types does not have analogues for kUndefined, kRunDeferred
  // or kBrowserDriven. We don't expect to encounter them here.
  switch (run_at) {
    case mojom::RunLocation::kDocumentEnd:
      return api::extension_types::RUN_AT_DOCUMENT_END;
    case mojom::RunLocation::kDocumentStart:
      return api::extension_types::RUN_AT_DOCUMENT_START;
    case mojom::RunLocation::kDocumentIdle:
      return api::extension_types::RUN_AT_DOCUMENT_IDLE;
    case mojom::RunLocation::kUndefined:
    case mojom::RunLocation::kRunDeferred:
    case mojom::RunLocation::kBrowserDriven:
      break;
  }

  NOTREACHED();
  return api::extension_types::RUN_AT_DOCUMENT_IDLE;
}

mojom::ExecutionWorld ConvertExecutionWorld(
    api::extension_types::ExecutionWorld world) {
  mojom::ExecutionWorld execution_world = mojom::ExecutionWorld::kIsolated;
  switch (world) {
    case api::extension_types::EXECUTION_WORLD_NONE:
    case api::extension_types::EXECUTION_WORLD_ISOLATED:
      break;  // Default to mojom::ExecutionWorld::kIsolated.
    case api::extension_types::EXECUTION_WORLD_MAIN:
      execution_world = mojom::ExecutionWorld::kMain;
  }

  return execution_world;
}

api::extension_types::ExecutionWorld ConvertExecutionWorldForAPI(
    mojom::ExecutionWorld world) {
  switch (world) {
    case mojom::ExecutionWorld::kIsolated:
      return api::extension_types::EXECUTION_WORLD_ISOLATED;
    case mojom::ExecutionWorld::kMain:
      return api::extension_types::EXECUTION_WORLD_MAIN;
  }

  NOTREACHED();
  return api::extension_types::EXECUTION_WORLD_ISOLATED;
}

}  // namespace extensions
