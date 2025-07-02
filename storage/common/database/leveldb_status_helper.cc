// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/common/database/leveldb_status_helper.h"

#include "base/metrics/histogram_functions.h"
#include "storage/common/database/db_status.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"

namespace storage {

namespace {

int GetTypeForLegacyLogging(DbStatus status) {
  if (status.ok()) {
    return leveldb_env::LEVELDB_STATUS_OK;
  } else if (status.IsNotFound()) {
    return leveldb_env::LEVELDB_STATUS_NOT_FOUND;
  } else if (status.IsCorruption()) {
    return leveldb_env::LEVELDB_STATUS_CORRUPTION;
  } else if (status.IsNotSupported()) {
    return leveldb_env::LEVELDB_STATUS_NOT_SUPPORTED;
  } else if (status.IsIOError()) {
    return leveldb_env::LEVELDB_STATUS_IO_ERROR;
  } else {
    return leveldb_env::LEVELDB_STATUS_INVALID_ARGUMENT;
  }
}

}  // namespace

DbStatus FromLevelDBStatus(const leveldb::Status& status) {
  if (status.ok()) {
    return DbStatus::OK();
  } else if (status.IsNotFound()) {
    return DbStatus::NotFound(status.ToString());
  } else if (status.IsCorruption()) {
    return DbStatus::Corruption(status.ToString());
  } else if (status.IsNotSupportedError()) {
    return DbStatus::NotSupported(status.ToString());
  } else if (status.IsIOError()) {
    return DbStatus::IOError(status.ToString());
  } else {
    return DbStatus::InvalidArgument(status.ToString());
  }
}

leveldb::Status ToLevelDBStatus(const DbStatus& status) {
  if (status.ok()) {
    return leveldb::Status::OK();
  } else if (status.IsNotFound()) {
    return leveldb::Status::NotFound(status.ToString());
  } else if (status.IsCorruption()) {
    return leveldb::Status::Corruption(status.ToString());
  } else if (status.IsNotSupported()) {
    return leveldb::Status::NotSupported(status.ToString());
  } else if (status.IsIOError()) {
    return leveldb::Status::IOError(status.ToString());
  } else {
    return leveldb::Status::InvalidArgument(status.ToString());
  }
}

void LogLevelDBStatusHistogram(std::string_view histogram_name,
                               const DbStatus& status) {
  base::UmaHistogramEnumeration(histogram_name,
                                static_cast<leveldb_env::LevelDBStatusValue>(
                                    GetTypeForLegacyLogging(status)),
                                leveldb_env::LEVELDB_STATUS_MAX);
}

bool IndicatesDiskFull(const DbStatus& status) {
  return leveldb_env::IndicatesDiskFull(ToLevelDBStatus(status));
}

}  // namespace storage
