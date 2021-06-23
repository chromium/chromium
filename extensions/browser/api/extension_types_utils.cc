// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/extension_types_utils.h"

#include "base/macros.h"

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

}  // namespace extensions
