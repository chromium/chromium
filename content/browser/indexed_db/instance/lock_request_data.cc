// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/lock_request_data.h"

namespace content::indexed_db {

const void* const LockRequestData::kKey = &LockRequestData::kKey;

LockRequestData::LockRequestData(const base::UnguessableToken& client_token,
                                 int scheduling_priority)
    : client_token(client_token), scheduling_priority(scheduling_priority) {}

LockRequestData::~LockRequestData() = default;

}  // namespace content::indexed_db
