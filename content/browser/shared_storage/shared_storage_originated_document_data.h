// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_ORIGINATED_DOCUMENT_DATA_H_
#define CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_ORIGINATED_DOCUMENT_DATA_H_

#include "content/browser/fenced_frame/fenced_frame_url_mapping.h"
#include "content/common/content_export.h"
#include "content/public/browser/document_user_data.h"

namespace content {

// Contains the originated-from shared storage data for a document (i.e. fenced
// frame where its URL was from a URN from
// `sharedStorage.runURLSelectionOperation()`).
class CONTENT_EXPORT SharedStorageOriginatedDocumentData
    : public DocumentUserData<SharedStorageOriginatedDocumentData> {
 public:
  ~SharedStorageOriginatedDocumentData() override;

  FencedFrameURLMapping::SharedStorageBudgetMetadata& budget_metadata() const {
    return budget_metadata_;
  }

 private:
  // No public constructors to force going through static methods of
  // `DocumentUserData` (e.g. `CreateForCurrentDocument()`).
  explicit SharedStorageOriginatedDocumentData(
      RenderFrameHost* rfh,
      FencedFrameURLMapping::SharedStorageBudgetMetadata* budget_metadata);

  friend DocumentUserData;
  DOCUMENT_USER_DATA_KEY_DECL();

  // The budgeting `url::Origin`, and the amount of budget to charge whenever
  // this document is navigating a top frame. `budget_metadata_` lives in
  // `FencedFrameURLMapping`. The amount of budget to charge can be 0 if no
  // budget charging is needed from the beginning, or if the budget was already
  // charged (e.g. another document referencing the same metadata has triggered
  // a top navigation).
  FencedFrameURLMapping::SharedStorageBudgetMetadata& budget_metadata_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_ORIGINATED_DOCUMENT_DATA_H_
