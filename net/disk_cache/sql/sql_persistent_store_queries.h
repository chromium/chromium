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
    "CREATE TABLE resources("
        // Unique ID for the resource
        "res_id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,"
        // Timestamp for LRU
        "last_used INTEGER NOT NULL,"
        // In memory hints (MemoryEntryDataHints).
        "hints INTEGER NOT NULL,"
        // End offset of the body
        "body_end INTEGER NOT NULL,"
        // Total bytes consumed by the entry
        "bytes_usage INTEGER NOT NULL,"
        // Flag for entries pending deletion
        "doomed INTEGER NOT NULL,"
        // The checksum `crc32(head + cache_key_hash)`.
        "check_sum INTEGER NOT NULL,"
        // The hash of `cache_key` created by simple_util::GetEntryHashKey()
        "cache_key_hash INTEGER NOT NULL,"
        // The cache key created by HttpCache::GenerateCacheKeyForRequest()
        "cache_key TEXT NOT NULL,"
        // Serialized response headers
        "head BLOB)";
// clang-format on

// The `blobs` table stores the data chunks of the cached body.
inline constexpr const char kInitSchema_CreateTableBlobs[] =
    // clang-format off
    "CREATE TABLE blobs("
        // Unique ID for the blob
        "blob_id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,"
        // Foreign key to resources.res_id
        "res_id INTEGER NOT NULL,"
        // Start offset of this blob chunk
        "start INTEGER NOT NULL,"
        // End offset of this blob chunk
        "end INTEGER NOT NULL,"
        // The checksum `crc32(blob + cache_key_hash)`.
        "check_sum INTEGER NOT NULL,"
        // The actual data chunk
        "blob BLOB NOT NULL)";
// clang-format on

// An index on `(cache_key_hash, doomed)` to speed up lookups for live entries.
// This is frequently used in operations like `OpenEntry` to quickly find a
// non-doomed entry for a given cache key.
inline constexpr const char kIndex_ResourcesCacheKeyHashDoomed[] =
    "CREATE INDEX index_resources_cache_key_hash_doomed ON "
    "resources(cache_key_hash, doomed)";

// An index on `last_used` and `bytes_usage` for live entries (`doomed=0`). This
// is crucial for eviction logic, which targets the least recently used entries.
// To avoid looking at the actual resources table during eviction, this creates
// a covering index.
inline constexpr const char kIndex_LiveResourcesLastUsed[] =
    "CREATE INDEX index_live_resources_last_used_bytes_usage ON "
    "resources(last_used, bytes_usage) WHERE doomed=0";

// Index for quickly loading entries with non-zero hints into the in-memory
// index.
inline constexpr const char kIndex_LiveResourcesHints[] =
    "CREATE INDEX index_live_resources_hints ON "
    "resources(hints) WHERE hints!=0 AND doomed=0";

// A unique index on `(res_id, start)` in the `blobs` table. This is critical
// for quickly finding the correct data blobs for a given entry when reading or
// writing data at a specific offset. The `UNIQUE` constraint ensures that
// there are no overlapping blobs starting at the same offset for the same
// entry, which is important for data integrity.
inline constexpr const char kIndex_BlobsResIdStart[] =
    "CREATE UNIQUE INDEX index_blobs_res_id_start ON "
    "blobs(res_id, start)";

inline constexpr const char kOpenEntry_SelectLiveResources[] =
    // clang-format off
    "SELECT "
        "res_id,"      // 0
        "last_used,"   // 1
        "body_end,"    // 2
        "check_sum,"   // 3
        "head "        // 4
    "FROM resources "
    "WHERE "
        "cache_key_hash=? AND " // 0
        "cache_key=? AND "      // 1
        "doomed=0 "
    "ORDER BY res_id DESC";
// clang-format on

inline constexpr const char kCreateEntry_InsertIntoResources[] =
    // clang-format off
    "INSERT INTO resources("
        "last_used,"      // 0
        "hints,"
        "body_end,"       // 1
        "bytes_usage,"    // 2
        "doomed,"
        "check_sum,"      // 3
        "cache_key_hash," // 4
        "cache_key) "     // 5
    "VALUES(?,0,?,?,0,?,?,?) "
    "RETURNING res_id";
// clang-format on

inline constexpr const char kDoomEntry_MarkDoomedResources[] =
    // clang-format off
    "UPDATE resources "
    "SET "
        "doomed=1 "
    "WHERE "
        "res_id=? AND "  // 0
        "doomed=0 "
    "RETURNING "
        "bytes_usage";       // 0
// clang-format on

inline constexpr const char kDeleteDoomedEntry_DeleteFromResources[] =
    // clang-format off
    "DELETE FROM resources "
    "WHERE "
        "res_id=? AND "  // 0
        "doomed=1";
// clang-format on

inline constexpr const char kDeleteLiveEntry_DeleteFromResources[] =
    // clang-format off
    "DELETE FROM resources "
    "WHERE "
        "cache_key_hash=? AND " // 0
        "cache_key=? AND "      // 1
        "doomed=0 "
    "RETURNING "
        "res_id,"           // 0
        "bytes_usage";      // 1
// clang-format on

inline constexpr const char kDeleteAllEntries_DeleteFromResources[] =
    "DELETE FROM resources";

inline constexpr const char kDeleteAllEntries_DeleteFromBlobs[] =
    "DELETE FROM blobs";

inline constexpr const char kDeleteLiveEntriesBetween_SelectLiveResources[] =
    // clang-format off
    "SELECT "
        "res_id,"       // 0
        "bytes_usage "  // 1
    "FROM resources "
    "WHERE "
        "last_used>=? AND "  // 0
        "last_used<? AND "   // 1
        "doomed=0";
// clang-format on

inline constexpr const char kDeleteResourceByResIds_DeleteFromResources[] =
    "DELETE FROM resources WHERE res_id=?";

inline constexpr const char kUpdateEntryLastUsedByKey_UpdateResourceLastUsed[] =
    // clang-format off
    "UPDATE resources "
    "SET "
        "last_used=? "          // 0
    "WHERE "
        "cache_key_hash=? AND " // 1
        "cache_key=? AND "      // 2
        "doomed=0";
// clang-format on

inline constexpr const char
    kUpdateEntryLastUsedByResId_UpdateResourceLastUsed[] =
        // clang-format off
    "UPDATE resources "
    "SET "
        "last_used=? "      // 0
    "WHERE "
        "res_id=? AND "     // 1
        "doomed=0";
// clang-format on

inline constexpr const char kUpdateEntryHeaderAndLastUsed_UpdateResource[] =
    // clang-format off
    "UPDATE resources "
    "SET "
        "last_used=?, "                // 0
        "bytes_usage=bytes_usage+?, "  // 1
        "check_sum=?, "                // 2
        "head=? "                      // 3
    "WHERE "
        "res_id=? AND "                // 4
        "doomed=0 "
    "RETURNING "
        "bytes_usage";                 // 0
// clang-format on

inline constexpr const char
    kUpdateEntryHeaderAndLastUsed_UpdateResourceAndHints[] =
        // clang-format off
    "UPDATE resources "
    "SET "
        "last_used=?, "                // 0
        "hints=?, "                    // 1
        "bytes_usage=bytes_usage+?, "  // 2
        "check_sum=?, "                // 3
        "head=? "                      // 4
    "WHERE "
        "res_id=? AND "                // 5
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
        "res_id=? "                   // 2
    "RETURNING "
        "body_end,"                   // 0
        "doomed";                     // 1
// clang-format on

inline constexpr const char kTrimOverlappingBlobs_DeleteContained[] =
    // clang-format off
    "DELETE FROM blobs "
    "WHERE "
        "res_id=? AND "      // 0
        "start>=? AND "      // 1
        "end<=? "            // 2
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
      "check_sum,"         // 3
      "blob "              // 4
  "FROM blobs "
  "WHERE "
      "res_id=? AND "      // 0
      "start<? AND "       // 1
      "end>?";             // 2
// clang-format on

inline constexpr const char kTruncateBlobsAfter_DeleteAfter[] =
    // clang-format off
    "DELETE FROM blobs "
    "WHERE "
        "res_id=? AND "      // 0
        "start>=? "          // 1
    "RETURNING "
        "start,"             // 0
        "end";               // 1
// clang-format on

inline constexpr const char kInsertNewBlob_InsertIntoBlobs[] =
    // clang-format off
    "INSERT INTO blobs("
        "res_id,"      // 0
        "start,"       // 1
        "end,"         // 2
        "check_sum,"   // 3
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

inline constexpr const char kDeleteBlobsByResId_DeleteFromBlobs[] =
    // clang-format off
    "DELETE FROM blobs "
    "WHERE "
        "res_id=?";       // 0
// clang-format on

inline constexpr const char kReadEntryData_SelectOverlapping[] =
    // clang-format off
    "SELECT "
        "start,"             // 0
        "end,"               // 1
        "check_sum,"         // 2
        "blob "              // 3
    "FROM blobs "
    "WHERE "
        "res_id=? AND "      // 0
        "start<? AND "       // 1
        "end>? "             // 2
    "ORDER BY start";
// clang-format on

inline constexpr const char kGetEntryAvailableRange_SelectOverlapping[] =
    // clang-format off
    "SELECT "
        "start,"  // 0
        "end "    // 1
    "FROM blobs "
    "WHERE "
        "res_id=? AND "      // 0
        "start<? AND "       // 1
        "end>? "             // 2
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

inline constexpr const char kOpenNextEntry_SelectLiveResources[] =
    // clang-format off
    "SELECT "
        "res_id,"      // 0
        "last_used,"   // 1
        "body_end,"    // 2
        "check_sum,"   // 3
        "cache_key,"   // 4
        "head "        // 5
    "FROM resources "
    "WHERE "
        "res_id<? AND "  // 0
        "doomed=0 "
    "ORDER BY res_id DESC";
// clang-format on

inline constexpr const char kStartEviction_SelectLiveResources[] =
    // clang-format off
    "SELECT "
        "res_id,"        // 0
        "bytes_usage, "  // 1
        "last_used "     // 2
    "FROM resources "
    "WHERE "
        "doomed=0 "
    "ORDER BY last_used";
// clang-format on

inline constexpr const char
    kCalculateResourceEntryCount_SelectCountFromLiveResources[] =
        "SELECT COUNT(*) FROM resources WHERE doomed=0";

inline constexpr const char
    kCalculateTotalSize_SelectTotalSizeFromLiveResources[] =
        "SELECT SUM(bytes_usage) FROM resources WHERE doomed=0";

inline constexpr const char
    kLoadInMemoryIndex_SelectCacheKeyHashFromLiveResources[] =
        // clang-format off
    "SELECT "
        "res_id, "          // 0
        "cache_key_hash, "  // 1
        "doomed "           // 2
    "FROM resources "
    "ORDER BY cache_key_hash";
// clang-format on

inline constexpr const char kLoadInMemoryIndex_SelectHintsFromLiveResources[] =
    // clang-format off
    "SELECT "
        "res_id, "          // 0
        "hints "            // 1
    "FROM resources "
    "WHERE hints!=0 AND doomed=0";
// clang-format on

}  // namespace internal

// An enum for all SQL queries. This helps ensure that all queries are tested.
enum class Query {
  kInitSchema_CreateTableResources,
  kInitSchema_CreateTableBlobs,

  kIndex_ResourcesCacheKeyHashDoomed,
  kIndex_LiveResourcesLastUsed,
  kIndex_LiveResourcesHints,
  kIndex_BlobsResIdStart,
  kOpenEntry_SelectLiveResources,
  kCreateEntry_InsertIntoResources,
  kDoomEntry_MarkDoomedResources,
  kDeleteDoomedEntry_DeleteFromResources,
  kDeleteLiveEntry_DeleteFromResources,
  kDeleteAllEntries_DeleteFromResources,
  kDeleteAllEntries_DeleteFromBlobs,
  kDeleteLiveEntriesBetween_SelectLiveResources,
  kDeleteResourceByResIds_DeleteFromResources,
  kUpdateEntryLastUsedByKey_UpdateResourceLastUsed,
  kUpdateEntryLastUsedByResId_UpdateResourceLastUsed,
  kUpdateEntryHeaderAndLastUsed_UpdateResource,
  kUpdateEntryHeaderAndLastUsed_UpdateResourceAndHints,
  kWriteEntryData_UpdateResource,
  kTrimOverlappingBlobs_DeleteContained,
  kTrimOverlappingBlobs_SelectOverlapping,
  kTruncateBlobsAfter_DeleteAfter,
  kInsertNewBlob_InsertIntoBlobs,
  kDeleteBlobById_DeleteFromBlobs,
  kDeleteBlobsByResId_DeleteFromBlobs,
  kReadEntryData_SelectOverlapping,
  kGetEntryAvailableRange_SelectOverlapping,
  kCalculateSizeOfEntriesBetween_SelectLiveResources,
  kOpenNextEntry_SelectLiveResources,
  kStartEviction_SelectLiveResources,
  kCalculateResourceEntryCount_SelectCountFromLiveResources,
  kCalculateTotalSize_SelectTotalSizeFromLiveResources,
  kLoadInMemoryIndex_SelectCacheKeyHashFromLiveResources,
  kLoadInMemoryIndex_SelectHintsFromLiveResources,

  kMaxValue = kLoadInMemoryIndex_SelectHintsFromLiveResources,
};

inline base::cstring_view GetQuery(Query query) {
  switch (query) {
    case Query::kInitSchema_CreateTableResources:
      return internal::kInitSchema_CreateTableResources;
    case Query::kInitSchema_CreateTableBlobs:
      return internal::kInitSchema_CreateTableBlobs;

    case Query::kIndex_ResourcesCacheKeyHashDoomed:
      return internal::kIndex_ResourcesCacheKeyHashDoomed;
    case Query::kIndex_LiveResourcesLastUsed:
      return internal::kIndex_LiveResourcesLastUsed;
    case Query::kIndex_LiveResourcesHints:
      return internal::kIndex_LiveResourcesHints;
    case Query::kIndex_BlobsResIdStart:
      return internal::kIndex_BlobsResIdStart;
    case Query::kOpenEntry_SelectLiveResources:
      return internal::kOpenEntry_SelectLiveResources;
    case Query::kCreateEntry_InsertIntoResources:
      return internal::kCreateEntry_InsertIntoResources;
    case Query::kDoomEntry_MarkDoomedResources:
      return internal::kDoomEntry_MarkDoomedResources;
    case Query::kDeleteDoomedEntry_DeleteFromResources:
      return internal::kDeleteDoomedEntry_DeleteFromResources;
    case Query::kDeleteLiveEntry_DeleteFromResources:
      return internal::kDeleteLiveEntry_DeleteFromResources;
    case Query::kDeleteAllEntries_DeleteFromResources:
      return internal::kDeleteAllEntries_DeleteFromResources;
    case Query::kDeleteAllEntries_DeleteFromBlobs:
      return internal::kDeleteAllEntries_DeleteFromBlobs;
    case Query::kDeleteLiveEntriesBetween_SelectLiveResources:
      return internal::kDeleteLiveEntriesBetween_SelectLiveResources;
    case Query::kDeleteResourceByResIds_DeleteFromResources:
      return internal::kDeleteResourceByResIds_DeleteFromResources;
    case Query::kUpdateEntryLastUsedByKey_UpdateResourceLastUsed:
      return internal::kUpdateEntryLastUsedByKey_UpdateResourceLastUsed;
    case Query::kUpdateEntryLastUsedByResId_UpdateResourceLastUsed:
      return internal::kUpdateEntryLastUsedByResId_UpdateResourceLastUsed;
    case Query::kUpdateEntryHeaderAndLastUsed_UpdateResource:
      return internal::kUpdateEntryHeaderAndLastUsed_UpdateResource;
    case Query::kUpdateEntryHeaderAndLastUsed_UpdateResourceAndHints:
      return internal::kUpdateEntryHeaderAndLastUsed_UpdateResourceAndHints;
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
    case Query::kDeleteBlobsByResId_DeleteFromBlobs:
      return internal::kDeleteBlobsByResId_DeleteFromBlobs;
    case Query::kReadEntryData_SelectOverlapping:
      return internal::kReadEntryData_SelectOverlapping;
    case Query::kGetEntryAvailableRange_SelectOverlapping:
      return internal::kGetEntryAvailableRange_SelectOverlapping;
    case Query::kCalculateSizeOfEntriesBetween_SelectLiveResources:
      return internal::kCalculateSizeOfEntriesBetween_SelectLiveResources;
    case Query::kOpenNextEntry_SelectLiveResources:
      return internal::kOpenNextEntry_SelectLiveResources;
    case Query::kStartEviction_SelectLiveResources:
      return internal::kStartEviction_SelectLiveResources;
    case Query::kCalculateResourceEntryCount_SelectCountFromLiveResources:
      return internal::
          kCalculateResourceEntryCount_SelectCountFromLiveResources;
    case Query::kCalculateTotalSize_SelectTotalSizeFromLiveResources:
      return internal::kCalculateTotalSize_SelectTotalSizeFromLiveResources;
    case Query::kLoadInMemoryIndex_SelectCacheKeyHashFromLiveResources:
      return internal::kLoadInMemoryIndex_SelectCacheKeyHashFromLiveResources;
    case Query::kLoadInMemoryIndex_SelectHintsFromLiveResources:
      return internal::kLoadInMemoryIndex_SelectHintsFromLiveResources;
  }
  NOTREACHED();
}

}  // namespace disk_cache_sql_queries

#endif  // NET_DISK_CACHE_SQL_SQL_PERSISTENT_STORE_QUERIES_H_
