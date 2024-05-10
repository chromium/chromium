// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_LOCK_REQUEST_DATA_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_LOCK_REQUEST_DATA_H_

#include "base/supports_user_data.h"

namespace content {

// This struct holds data relevant to opening a new connection/database while
// IndexedDBConnectionCoordinator manages queued operations.
struct IndexedDBLockRequestData : public base::SupportsUserData::Data {
  static const void* const kKey;

  explicit IndexedDBLockRequestData(uint64_t client_id);
  ~IndexedDBLockRequestData() override;

  uint64_t client_id;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_LOCK_REQUEST_DATA_H_
