// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_APPCACHE_APPCACHE_BACKFILLERS_H_
#define CONTENT_BROWSER_APPCACHE_APPCACHE_BACKFILLERS_H_

#include <string>

#include "base/optional.h"
#include "sql/database.h"

namespace content {

// Backfills an AppCache database after it has been migrated to version 8.
class AppCacheBackfillerVersion8 {
 public:
  // |db| must outlive this instance.
  explicit AppCacheBackfillerVersion8(sql::Database* db) : db_(db) {}

  // Populates the |padding_size| column in the Caches and Entries tables.
  //
  // The |padding_size| columns were added in version 8 of the schema.
  bool BackfillPaddingSizes();

 private:
  // Iterates over each Entry record for a cache; execute |callable| on each
  // iteration.
  //
  // ForEachCallable: (int64_t cache_id, int64_t group_id) -> bool.
  //
  // Returns whether the database queries succeeded.
  template <typename ForEachCallable>
  bool ForEachEntry(int64_t cache_id, const ForEachCallable& callable);

  // Gets the manifest URL of a group. Returns base::nullopt if the database
  // query failed.
  base::Optional<std::string> GetManifestUrlForGroup(int64_t group_id);

  // Updates the padding size of the Entry record identified by |response_id|.
  // Returns whether the database statement succeeded.
  bool UpdateEntryPaddingSize(int64_t padding_size, int64_t response_id);

  // Updates the padding size of the Cache record identified by |cache_id|.
  // Returns whether the database statement succeeded.
  bool UpdateCachePaddingSize(int64_t padding_size, int64_t cache_id);

  // The AppCacheDatabase instance being backfilled.
  sql::Database* const db_;
};

// Backfills an AppCache database after it has been migrated to version 9.
class AppCacheBackfillerVersion9 {
 public:
  // |db| must outlive this instance.
  explicit AppCacheBackfillerVersion9(sql::Database* db) : db_(db) {}

  // Populates the |manifest_parser_version| and |manifest_scope| columns in the
  // Groups table.
  //
  // These columns were added in version 9 of the schema.
  bool BackfillManifestParserVersionAndScope();

 private:
  // Updates the manifest_parser version value of the Cache record identified by
  // |cache_id|.
  // Returns whether the database statement succeeded.
  bool UpdateCacheManifestParserVersion(int64_t cache_id,
                                        int64_t manifest_parser_version);

  // Updates the manifest_scope value of the Cache record identified by
  // |cache_id|.
  // Returns whether the database statement succeeded.
  bool UpdateCacheManifestScope(int64_t cache_id,
                                const std::string& manifest_scope);

  base::Optional<std::string> GetManifestUrlForGroup(int64_t group_id);

  // The AppCacheDatabase instance being backfilled.
  sql::Database* const db_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_APPCACHE_APPCACHE_BACKFILLERS_H_
