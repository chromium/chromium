// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_lock_request_data.h"

namespace content {

const void* const IndexedDBLockRequestData::kKey =
    &IndexedDBLockRequestData::kKey;

IndexedDBLockRequestData::IndexedDBLockRequestData(
    const base::UnguessableToken& client_token)
    : client_token(client_token) {}

IndexedDBLockRequestData::~IndexedDBLockRequestData() = default;

}  // namespace content
