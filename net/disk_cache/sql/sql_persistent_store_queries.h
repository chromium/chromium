// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SQL_SQL_PERSISTENT_STORE_QUERIES_H_
#define NET_DISK_CACHE_SQL_SQL_PERSISTENT_STORE_QUERIES_H_

#include "base/notreached.h"
#include "base/strings/cstring_view.h"

namespace disk_cache_sql_queries {
namespace internal {

// The query strings are defined in this namespace to hide them from the public
// API. Callers should use `GetQuery()` instead.
//
// The query strings are defined as `inline constexpr` variables in this header
// file. This allows for compile-time optimization.

// The `resources` table stores the main metadata for each cache entry.
inline constexpr const char kInitSchema_CreateTableResources[] =
    // clang-format off
    "CREATE TABLE IF NOT EXISTS resources("
        // Unique ID for the resource
        "res_id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,"
        // High part of an unguessable token
        "token_high INTEGER NOT NULL,"
        // Low part of an unguessable token
        "token_low INTEGER NOT NULL,"
        // Timestamp for LRU
        "last_used INTEGER NOT NULL,"
        // End offset of the body
        "body_end INTEGER NOT NULL,"
        // Total bytes consumed by the entry
        "bytes_usage INTEGER NOT NULL,"
        // Flag for entries pending deletion
        "doomed INTEGER NOT NULL,"
        // The cache key created by HttpCache::GenerateCacheKeyForRequest()
        "cache_key TEXT NOT NULL,"
        // Serialized response headers
        "head BLOB)";
// clang-format on

// The `blobs` table stores the data chunks of the cached body.
inline constexpr const char kInitSchema_CreateTableBlobs[] =
    // clang-format off
    "CREATE TABLE IF NOT EXISTS blobs("
        // Unique ID for the blob
        "blob_id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,"
        // Foreign key to resources.token_high
        "token_high INTEGER NOT NULL,"
        // Foreign key to resources.token_low
        "token_low INTEGER NOT NULL,"
        // Start offset of this blob chunk
        "start INTEGER NOT NULL,"
        // End offset of this blob chunk
        "end INTEGER NOT NULL,"
        // The actual data chunk
        "blob BLOB NOT NULL)";
// clang-format on

// A unique index on the entry's token. This is used to quickly look up or
// update a resource entry using its unique token, which is essential for
// operations like `DoomEntry`, `UpdateEntryHeaderAndLastUsed`, and
// `WriteEntryData`. The `UNIQUE` constraint ensures data integrity by
// preventing duplicate tokens.
inline constexpr const char kIndex_ResourcesToken[] =
    "CREATE UNIQUE INDEX IF NOT EXISTS index_resources_token ON "
    "resources(token_high, token_low)";

// An index on `(cache_key, doomed)` to speed up lookups for live entries. This
// is frequently used in operations like `OpenEntry` to quickly find a
// non-doomed entry for a given cache key.
inline constexpr const char kIndex_ResourcesCacheKeyDoomed[] =
    "CREATE INDEX IF NOT EXISTS index_resources_cache_key_doomed ON "
    "resources(cache_key, doomed)";

// An index on `(doomed, last_used)` to optimize eviction logic. Eviction
// typically targets the least recently used (`last_used`) live (`doomed=false`)
// entries. This index significantly speeds up queries that select entries for
// eviction.
inline constexpr const char kIndex_ResourcesDoomedLastUsed[] =
    "CREATE INDEX IF NOT EXISTS index_resources_doomed_last_used ON "
    "resources(doomed, last_used)";

// An index on `(doomed, res_id)` to optimize iterating through entries while
// filtering for live (`doomed=false`) entries. `res_id` is a monotonically
// increasing primary key, making this index ideal for ordered traversals like
// in `OpenLatestEntryBeforeResId`.
inline constexpr const char kIndex_ResourcesDoomedResId[] =
    "CREATE INDEX IF NOT EXISTS index_resources_doomed_res_id ON "
    "resources(doomed, res_id)";

// A unique index on `(token_high, token_low, start)` in the `blobs` table. This
// is critical for quickly finding the correct data blobs for a given entry when
// reading or writing data at a specific offset. The `UNIQUE` constraint
// ensures that there are no overlapping blobs starting at the same offset for
// the same entry, which is important for data integrity.
inline constexpr const char kIndex_BlobsTokenStart[] =
    "CREATE UNIQUE INDEX IF NOT EXISTS index_blobs_token_start ON "
    "blobs(token_high, token_low, start)";

inline constexpr const char kOpenEntry_SelectLiveResources[] =
    // clang-format off
    "SELECT "
        "token_high,"  // 0
        "token_low,"   // 1
        "last_used,"   // 2
        "body_end,"    // 3
        "head "        // 4
    "FROM resources "
    "WHERE "
        "cache_key=? AND "  // 0
        "doomed=0 "
    "ORDER BY res_id DESC";
// clang-format on

inline constexpr const char kCreateEntry_InsertIntoResources[] =
    // clang-format off
    "INSERT INTO resources("
        "token_high,"   // 0
        "token_low,"    // 1
        "last_used,"    // 2
        "body_end,"     // 3
        "bytes_usage,"  // 4
        "doomed,"
        "cache_key) "   // 5
    "VALUES(?,?,?,?,?,0,?)";
// clang-format on

inline constexpr const char kDoomEntry_MarkDoomedResources[] =
    // clang-format off
    "UPDATE resources "
    "SET "
        "doomed=1 "
    "WHERE "
        "cache_key=? AND "   // 0
        "token_high=? AND "  // 1
        "token_low=? AND "   // 2
        "doomed=0 "
    "RETURNING "
        "bytes_usage";       // 0
// clang-format on

inline constexpr const char kDeleteDoomedEntry_DeleteFromResources[] =
    // clang-format off
    "DELETE FROM resources "
    "WHERE "
        "cache_key=? AND "   // 0
        "token_high=? AND "  // 1
        "token_low=? AND "   // 2
        "doomed=1";
// clang-format on

inline constexpr const char kDeleteDoomedEntries_SelectDoomedResources[] =
    // clang-format off
    "SELECT "
        "res_id,"       // 0
        "token_high,"   // 1
        "token_low "    // 2
    "FROM resources "
    "WHERE doomed=1";
// clang-format on

inline constexpr const char kDeleteLiveEntry_DeleteFromResources[] =
    // clang-format off
    "DELETE FROM resources "
    "WHERE "
        "cache_key=? AND "  // 0
        "doomed=0 "
    "RETURNING "
        "token_high,"       // 0
        "token_low,"        // 1
        "bytes_usage";      // 2
// clang-format on

inline constexpr const char kDeleteAllEntries_DeleteFromResources[] =
    "DELETE FROM resources";

inline constexpr const char kDeleteAllEntries_DeleteFromBlobs[] =
    "DELETE FROM blobs";

inline constexpr const char kDeleteLiveEntriesBetween_SelectLiveResources[] =
    // clang-format off
    "SELECT "
        "res_id,"       // 0
        "token_high,"   // 1
        "token_low,"    // 2
        "bytes_usage,"  // 3
        "cache_key "    // 4
    "FROM resources "
    "WHERE "
        "last_used>=? AND "  // 0
        "last_used<? AND "   // 1
        "doomed=0";
// clang-format on

inline constexpr const char kDeleteResourcesByResIds_DeleteFromResources[] =
    "DELETE FROM resources WHERE res_id=?";

inline constexpr const char kUpdateEntryLastUsed_UpdateResourceLastUsed[] =
    // clang-format off
    "UPDATE resources "
    "SET "
        "last_used=? "      // 0
    "WHERE "
        "cache_key=? AND "  // 1
        "doomed=0";
// clang-format on

inline constexpr const char kUpdateEntryHeaderAndLastUsed_UpdateResource[] =
    // clang-format off
    "UPDATE resources "
    "SET "
        "last_used=?, "                // 0
        "bytes_usage=bytes_usage+?, "  // 1
        "head=? "                      // 2
    "WHERE "
        "cache_key=? AND "             // 3
        "token_high=? AND "            // 4
        "token_low=? AND "             // 5
        "doomed=0 "
    "RETURNING "
        "bytes_usage";                 // 0
// clang-format on

inline constexpr const char kWriteEntryData_UpdateResource[] =
    // clang-format off
    "UPDATE resources "
    "SET "
        "body_end=body_end+?, "       // 0
        "bytes_usage=bytes_usage+? "  // 1
    "WHERE "
        "cache_key=? AND "            // 2
        "token_high=? AND "           // 3
        "token_low=? "                // 4
    "RETURNING "
        "body_end,"                   // 0
        "doomed";                     // 1
// clang-format on

inline constexpr const char kTrimOverlappingBlobs_DeleteContained[] =
    // clang-format off
    "DELETE FROM blobs "
    "WHERE "
        "token_high=? AND "  // 0
        "token_low=? AND "   // 1
        "start>=? AND "      // 2
        "end<=? "            // 3
    "RETURNING "
        "start,"             // 0
        "end";               // 1
// clang-format on

inline constexpr const char kTrimOverlappingBlobs_SelectOverlapping[] =
    // clang-format off
  "SELECT "
      "blob_id,"           // 0
      "start,"             // 1
      "end,"               // 2
      "blob "              // 3
  "FROM blobs "
  "WHERE "
      "token_high=? AND "  // 0
      "token_low=? AND "   // 1
      "start<? AND "       // 2
      "end>?";             // 3
// clang-format on

inline constexpr const char kTruncateBlobsAfter_DeleteAfter[] =
    // clang-format off
    "DELETE FROM blobs "
    "WHERE "
        "token_high=? AND "  // 0
        "token_low=? AND "   // 1
        "start>=? "          // 2
    "RETURNING "
        "start,"             // 0
        "end";               // 1
// clang-format on

inline constexpr const char kInsertNewBlob_InsertIntoBlobs[] =
    // clang-format off
    "INSERT INTO blobs("
        "token_high,"  // 0
        "token_low,"   // 1
        "start,"       // 2
        "end,"         // 3
        "blob) "       // 4
    "VALUES(?,?,?,?,?)";
// clang-format on

inline constexpr const char kDeleteBlobById_DeleteFromBlobs[] =
    // clang-format off
    "DELETE FROM blobs "
    "WHERE "
        "blob_id=? "  // 0
    "RETURNING "
        "start,"      // 0
        "end";        // 1
// clang-format on

inline constexpr const char kDeleteBlobsByToken_DeleteFromBlobs[] =
    // clang-format off
    "DELETE FROM blobs "
    "WHERE "
        "token_high=? AND "  // 0
        "token_low=?";       // 1
// clang-format on

inline constexpr const char kReadEntryData_SelectOverlapping[] =
    // clang-format off
    "SELECT "
        "start,"             // 0
        "end,"               // 1
        "blob "              // 2
    "FROM blobs "
    "WHERE "
        "token_high=? AND "  // 0
        "token_low=? AND "   // 1
        "start<? AND "       // 2
        "end>? "             // 3
    "ORDER BY start";
// clang-format on

inline constexpr const char kGetEntryAvailableRange_SelectOverlapping[] =
    // clang-format off
    "SELECT "
        "start,"  // 0
        "end "    // 1
    "FROM blobs "
    "WHERE "
        "token_high=? AND "  // 0
        "token_low=? AND "   // 1
        "start<? AND "       // 2
        "end>? "             // 3
    "ORDER BY start";
// clang-format on

inline constexpr const char
    kCalculateSizeOfEntriesBetween_SelectLiveResources[] =
        // clang-format off
    "SELECT "
        "bytes_usage "  // 0
    "FROM resources "
    "WHERE "
        "last_used>=? AND "  // 0
        "last_used<? AND "   // 1
        "doomed=0";
// clang-format on

inline constexpr const char kOpenLatestEntryBeforeResId_SelectLiveResources[] =
    // clang-format off
    "SELECT "
        "res_id,"      // 0
        "token_high,"  // 1
        "token_low,"   // 2
        "last_used,"   // 3
        "body_end,"    // 4
        "cache_key,"   // 5
        "head "        // 6
    "FROM resources "
    "WHERE "
        "res_id<? AND "  // 0
        "doomed=0 "
    "ORDER BY res_id DESC";
// clang-format on

inline constexpr const char kRunEviction_SelectLiveResources[] =
    // clang-format off
    "SELECT "
        "token_high,"   // 0
        "token_low,"    // 1
        "cache_key,"    // 2
        "bytes_usage "  // 3
    "FROM resources "
    "WHERE "
        "doomed=0 "
    "ORDER BY last_used";
// clang-format on

inline constexpr const char kRunEviction_DeleteFromResources[] =
    // clang-format off
    "DELETE FROM resources "
    "WHERE "
        "token_high=? AND "  // 0
        "token_low=?";       // 1
// clang-format on

inline constexpr const char
    kCalculateResourceEntryCount_SelectCountFromLiveResources[] =
        "SELECT COUNT(*) FROM resources WHERE doomed=0";

inline constexpr const char
    kCalculateTotalSize_SelectTotalSizeFromLiveResources[] =
        "SELECT SUM(bytes_usage) FROM resources WHERE doomed=0";

}  // namespace internal

// An enum for all SQL queries. This helps ensure that all queries are tested.
enum class Query {
  kInitSchema_CreateTableResources,
  kInitSchema_CreateTableBlobs,
  kIndex_ResourcesToken,
  kIndex_ResourcesCacheKeyDoomed,
  kIndex_ResourcesDoomedLastUsed,
  kIndex_ResourcesDoomedResId,
  kIndex_BlobsTokenStart,
  kOpenEntry_SelectLiveResources,
  kCreateEntry_InsertIntoResources,
  kDoomEntry_MarkDoomedResources,
  kDeleteDoomedEntry_DeleteFromResources,
  kDeleteDoomedEntries_SelectDoomedResources,
  kDeleteLiveEntry_DeleteFromResources,
  kDeleteAllEntries_DeleteFromResources,
  kDeleteAllEntries_DeleteFromBlobs,
  kDeleteLiveEntriesBetween_SelectLiveResources,
  kDeleteResourcesByResIds_DeleteFromResources,
  kUpdateEntryLastUsed_UpdateResourceLastUsed,
  kUpdateEntryHeaderAndLastUsed_UpdateResource,
  kWriteEntryData_UpdateResource,
  kTrimOverlappingBlobs_DeleteContained,
  kTrimOverlappingBlobs_SelectOverlapping,
  kTruncateBlobsAfter_DeleteAfter,
  kInsertNewBlob_InsertIntoBlobs,
  kDeleteBlobById_DeleteFromBlobs,
  kDeleteBlobsByToken_DeleteFromBlobs,
  kReadEntryData_SelectOverlapping,
  kGetEntryAvailableRange_SelectOverlapping,
  kCalculateSizeOfEntriesBetween_SelectLiveResources,
  kOpenLatestEntryBeforeResId_SelectLiveResources,
  kRunEviction_SelectLiveResources,
  kRunEviction_DeleteFromResources,
  kCalculateResourceEntryCount_SelectCountFromLiveResources,
  kCalculateTotalSize_SelectTotalSizeFromLiveResources,

  kMaxValue = kCalculateTotalSize_SelectTotalSizeFromLiveResources,
};

inline base::cstring_view GetQuery(Query query) {
  switch (query) {
    case Query::kInitSchema_CreateTableResources:
      return internal::kInitSchema_CreateTableResources;
    case Query::kInitSchema_CreateTableBlobs:
      return internal::kInitSchema_CreateTableBlobs;
    case Query::kIndex_ResourcesToken:
      return internal::kIndex_ResourcesToken;
    case Query::kIndex_ResourcesCacheKeyDoomed:
      return internal::kIndex_ResourcesCacheKeyDoomed;
    case Query::kIndex_ResourcesDoomedLastUsed:
      return internal::kIndex_ResourcesDoomedLastUsed;
    case Query::kIndex_ResourcesDoomedResId:
      return internal::kIndex_ResourcesDoomedResId;
    case Query::kIndex_BlobsTokenStart:
      return internal::kIndex_BlobsTokenStart;
    case Query::kOpenEntry_SelectLiveResources:
      return internal::kOpenEntry_SelectLiveResources;
    case Query::kCreateEntry_InsertIntoResources:
      return internal::kCreateEntry_InsertIntoResources;
    case Query::kDoomEntry_MarkDoomedResources:
      return internal::kDoomEntry_MarkDoomedResources;
    case Query::kDeleteDoomedEntry_DeleteFromResources:
      return internal::kDeleteDoomedEntry_DeleteFromResources;
    case Query::kDeleteDoomedEntries_SelectDoomedResources:
      return internal::kDeleteDoomedEntries_SelectDoomedResources;
    case Query::kDeleteLiveEntry_DeleteFromResources:
      return internal::kDeleteLiveEntry_DeleteFromResources;
    case Query::kDeleteAllEntries_DeleteFromResources:
      return internal::kDeleteAllEntries_DeleteFromResources;
    case Query::kDeleteAllEntries_DeleteFromBlobs:
      return internal::kDeleteAllEntries_DeleteFromBlobs;
    case Query::kDeleteLiveEntriesBetween_SelectLiveResources:
      return internal::kDeleteLiveEntriesBetween_SelectLiveResources;
    case Query::kDeleteResourcesByResIds_DeleteFromResources:
      return internal::kDeleteResourcesByResIds_DeleteFromResources;
    case Query::kUpdateEntryLastUsed_UpdateResourceLastUsed:
      return internal::kUpdateEntryLastUsed_UpdateResourceLastUsed;
    case Query::kUpdateEntryHeaderAndLastUsed_UpdateResource:
      return internal::kUpdateEntryHeaderAndLastUsed_UpdateResource;
    case Query::kWriteEntryData_UpdateResource:
      return internal::kWriteEntryData_UpdateResource;
    case Query::kTrimOverlappingBlobs_DeleteContained:
      return internal::kTrimOverlappingBlobs_DeleteContained;
    case Query::kTrimOverlappingBlobs_SelectOverlapping:
      return internal::kTrimOverlappingBlobs_SelectOverlapping;
    case Query::kTruncateBlobsAfter_DeleteAfter:
      return internal::kTruncateBlobsAfter_DeleteAfter;
    case Query::kInsertNewBlob_InsertIntoBlobs:
      return internal::kInsertNewBlob_InsertIntoBlobs;
    case Query::kDeleteBlobById_DeleteFromBlobs:
      return internal::kDeleteBlobById_DeleteFromBlobs;
    case Query::kDeleteBlobsByToken_DeleteFromBlobs:
      return internal::kDeleteBlobsByToken_DeleteFromBlobs;
    case Query::kReadEntryData_SelectOverlapping:
      return internal::kReadEntryData_SelectOverlapping;
    case Query::kGetEntryAvailableRange_SelectOverlapping:
      return internal::kGetEntryAvailableRange_SelectOverlapping;
    case Query::kCalculateSizeOfEntriesBetween_SelectLiveResources:
      return internal::kCalculateSizeOfEntriesBetween_SelectLiveResources;
    case Query::kOpenLatestEntryBeforeResId_SelectLiveResources:
      return internal::kOpenLatestEntryBeforeResId_SelectLiveResources;
    case Query::kRunEviction_SelectLiveResources:
      return internal::kRunEviction_SelectLiveResources;
    case Query::kRunEviction_DeleteFromResources:
      return internal::kRunEviction_DeleteFromResources;
    case Query::kCalculateResourceEntryCount_SelectCountFromLiveResources:
      return internal::
          kCalculateResourceEntryCount_SelectCountFromLiveResources;
    case Query::kCalculateTotalSize_SelectTotalSizeFromLiveResources:
      return internal::kCalculateTotalSize_SelectTotalSizeFromLiveResources;
  }
  NOTREACHED();
}

}  // namespace disk_cache_sql_queries

#endif  // NET_DISK_CACHE_SQL_SQL_PERSISTENT_STORE_QUERIES_H_
