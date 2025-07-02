// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_COMMON_DATABASE_LEVELDB_STATUS_HELPER_H_
#define STORAGE_COMMON_DATABASE_LEVELDB_STATUS_HELPER_H_

#include "base/component_export.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"

namespace storage {

class DbStatus;
// A set of helpers to convert between `storage::DbStatus` and
// `leveldb::Status`, and to access some leveldb::Status utilities with a
// `storage::DbStatus`.
COMPONENT_EXPORT(STORAGE_LEVELDB_STATUS_HELPER)
DbStatus FromLevelDBStatus(const leveldb::Status& status);

COMPONENT_EXPORT(STORAGE_LEVELDB_STATUS_HELPER)
leveldb::Status ToLevelDBStatus(const DbStatus& status);

COMPONENT_EXPORT(STORAGE_LEVELDB_STATUS_HELPER)
void LogLevelDBStatusHistogram(std::string_view histogram_name,
                               const DbStatus& status);

COMPONENT_EXPORT(STORAGE_LEVELDB_STATUS_HELPER)
bool IndicatesDiskFull(const DbStatus& status);

}  // namespace storage

#endif  // STORAGE_COMMON_DATABASE_LEVELDB_STATUS_HELPER_H_
