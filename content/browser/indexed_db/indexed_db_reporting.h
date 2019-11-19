// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_REPORTING_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_REPORTING_H_

#include <string>

#include "base/logging.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"

namespace url {
class Origin;
}

namespace content {
namespace indexed_db {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum IndexedDBBackingStoreErrorSource {
  // 0 - 2 are no longer used.
  FIND_KEY_IN_INDEX = 3,
  GET_IDBDATABASE_METADATA = 4,
  GET_INDEXES = 5,
  GET_KEY_GENERATOR_CURRENT_NUMBER = 6,
  GET_OBJECT_STORES = 7,
  GET_RECORD = 8,
  KEY_EXISTS_IN_OBJECT_STORE = 9,
  LOAD_CURRENT_ROW = 10,
  SET_UP_METADATA = 11,
  GET_PRIMARY_KEY_VIA_INDEX = 12,
  KEY_EXISTS_IN_INDEX = 13,
  VERSION_EXISTS = 14,
  DELETE_OBJECT_STORE = 15,
  SET_MAX_OBJECT_STORE_ID = 16,
  SET_MAX_INDEX_ID = 17,
  GET_NEW_DATABASE_ID = 18,
  GET_NEW_VERSION_NUMBER = 19,
  CREATE_IDBDATABASE_METADATA = 20,
  DELETE_DATABASE = 21,
  TRANSACTION_COMMIT_METHOD = 22,  // TRANSACTION_COMMIT is a WinNT.h macro
  GET_DATABASE_NAMES = 23,
  DELETE_INDEX = 24,
  CLEAR_OBJECT_STORE = 25,
  READ_BLOB_JOURNAL = 26,
  DECODE_BLOB_JOURNAL = 27,
  GET_BLOB_KEY_GENERATOR_CURRENT_NUMBER = 28,
  GET_BLOB_INFO_FOR_RECORD = 29,
  UPGRADING_SCHEMA_CORRUPTED_BLOBS = 30,
  REVERT_SCHEMA_TO_V2 = 31,
  INTERNAL_ERROR_MAX,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum IndexedDBBackingStoreOpenResult {
  INDEXED_DB_BACKING_STORE_OPEN_MEMORY_SUCCESS,
  INDEXED_DB_BACKING_STORE_OPEN_SUCCESS,
  INDEXED_DB_BACKING_STORE_OPEN_FAILED_DIRECTORY,
  INDEXED_DB_BACKING_STORE_OPEN_FAILED_UNKNOWN_SCHEMA,
  INDEXED_DB_BACKING_STORE_OPEN_CLEANUP_DESTROY_FAILED,
  INDEXED_DB_BACKING_STORE_OPEN_CLEANUP_REOPEN_FAILED,
  INDEXED_DB_BACKING_STORE_OPEN_CLEANUP_REOPEN_SUCCESS,
  INDEXED_DB_BACKING_STORE_OPEN_FAILED_IO_ERROR_CHECKING_SCHEMA,
  INDEXED_DB_BACKING_STORE_OPEN_FAILED_UNKNOWN_ERR_DEPRECATED,
  INDEXED_DB_BACKING_STORE_OPEN_MEMORY_FAILED,
  INDEXED_DB_BACKING_STORE_OPEN_ATTEMPT_NON_ASCII,
  INDEXED_DB_BACKING_STORE_OPEN_DISK_FULL_DEPRECATED,
  INDEXED_DB_BACKING_STORE_OPEN_ORIGIN_TOO_LONG,
  INDEXED_DB_BACKING_STORE_OPEN_NO_RECOVERY,
  INDEXED_DB_BACKING_STORE_OPEN_FAILED_PRIOR_CORRUPTION,
  INDEXED_DB_BACKING_STORE_OPEN_FAILED_CLEANUP_JOURNAL_ERROR,
  INDEXED_DB_BACKING_STORE_OPEN_FAILED_METADATA_SETUP,
  INDEXED_DB_BACKING_STORE_OPEN_MAX,
};

void ReportOpenStatus(IndexedDBBackingStoreOpenResult result,
                      const url::Origin& origin);

void ReportInternalError(const char* type,
                         IndexedDBBackingStoreErrorSource location);

void ReportSchemaVersion(int version, const url::Origin& origin);

void ReportV2Schema(bool has_broken_blobs, const url::Origin& origin);

void ReportLevelDBError(const std::string& histogram_name,
                        const leveldb::Status& s);

// Use to signal conditions caused by data corruption.
// A macro is used instead of an inline function so that the assert and log
// report the line number.
#define REPORT_ERROR(type, location)                      \
  do {                                                    \
    LOG(ERROR) << "IndexedDB " type " Error: " #location; \
    ::content::indexed_db::ReportInternalError(           \
        type, ::content::indexed_db::location);           \
  } while (0)

#define INTERNAL_READ_ERROR(location) REPORT_ERROR("Read", location)
#define INTERNAL_CONSISTENCY_ERROR(location) \
  REPORT_ERROR("Consistency", location)
#define INTERNAL_WRITE_ERROR(location) REPORT_ERROR("Write", location)

// Use to signal conditions that usually indicate developer error, but
// could be caused by data corruption.  A macro is used instead of an
// inline function so that the assert and log report the line number.
// TODO(cmumford): Improve test coverage so that all error conditions are
// "tested" and then delete this macro.
#define REPORT_ERROR_UNTESTED(type, location)             \
  do {                                                    \
    LOG(ERROR) << "IndexedDB " type " Error: " #location; \
    NOTREACHED();                                         \
    ::content::indexed_db::ReportInternalError(           \
        type, ::content::indexed_db::location);           \
  } while (0)

#define INTERNAL_READ_ERROR_UNTESTED(location) \
  REPORT_ERROR_UNTESTED("Read", location)
#define INTERNAL_CONSISTENCY_ERROR_UNTESTED(location) \
  REPORT_ERROR_UNTESTED("Consistency", location)
#define INTERNAL_WRITE_ERROR_UNTESTED(location) \
  REPORT_ERROR_UNTESTED("Write", location)

}  // namespace indexed_db
}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_REPORTING_H_
