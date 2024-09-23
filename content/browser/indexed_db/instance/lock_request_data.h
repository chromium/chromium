// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INSTANCE_LOCK_REQUEST_DATA_H_
#define CONTENT_BROWSER_INDEXED_DB_INSTANCE_LOCK_REQUEST_DATA_H_

#include "base/supports_user_data.h"
#include "base/unguessable_token.h"

namespace content::indexed_db {

// This struct holds data about the client that has requested a new connection
// or transaction. It's a way to inject extra metadata into
// `PartitionedLockHolder`.
// TODO(estade): `PartitionedLockHolder` and related classes should live in
// `//content/browser/indexed_db/instance`, in which case this extra struct can
// be folded into `PartitionedLockHolder`.
struct LockRequestData : public base::SupportsUserData::Data {
  static const void* const kKey;

  LockRequestData(const base::UnguessableToken& client_token,
                  int scheduling_priority);
  ~LockRequestData() override;

  base::UnguessableToken client_token;
  int scheduling_priority;
};

}  // namespace content::indexed_db

#endif  // CONTENT_BROWSER_INDEXED_DB_INSTANCE_LOCK_REQUEST_DATA_H_
