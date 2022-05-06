// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_storage/shared_storage_originated_document_data.h"

namespace content {

SharedStorageOriginatedDocumentData::~SharedStorageOriginatedDocumentData() =
    default;

SharedStorageOriginatedDocumentData::SharedStorageOriginatedDocumentData(
    RenderFrameHost* rfh,
    FencedFrameURLMapping::SharedStorageBudgetMetadata* budget_metadata)
    : DocumentUserData(rfh), budget_metadata_(*budget_metadata) {}

DOCUMENT_USER_DATA_KEY_IMPL(SharedStorageOriginatedDocumentData);

}  // namespace content
