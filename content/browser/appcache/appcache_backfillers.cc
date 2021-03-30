// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_backfillers.h"

#include "content/browser/appcache/appcache_update_job.h"
#include "net/http/http_request_headers.h"
#include "sql/statement.h"
#include "storage/common/quota/padding_key.h"
#include "url/gurl.h"

namespace content {

namespace {

int64_t ComputeEntryPaddingSize(std::string response_url,
                                std::string manifest_url) {
  if (GURL(response_url).GetOrigin() == GURL(manifest_url).GetOrigin())
    return 0;
  return storage::ComputeRandomResponsePadding();
}

// Iterates over each Cache record; execute |callable| on each iteration.
//
// ForEachCallable: (int64_t cache_id, int64_t group_id) -> bool.
//
// Returns whether the database queries succeeded.
template <typename ForEachCallable>
bool ForEachCache(sql::Database* db, const ForEachCallable& callable) {
  static const char kSql[] = "SELECT cache_id, group_id FROM Caches";
  sql::Statement statement(db->GetUniqueStatement(kSql));
  while (statement.Step()) {
    int64_t cache_id = statement.ColumnInt64(0);
    int64_t group_id = statement.ColumnInt64(1);
    if (!callable(cache_id, group_id))
      return false;
  }
  return true;
}

}  // namespace

// AppCacheBackfillerVersion8

bool AppCacheBackfillerVersion8::BackfillPaddingSizes() {
  return ForEachCache(db_, [&](int64_t cache_id, int64_t group_id) -> bool {
    base::Optional<std::string> manifest_url = GetManifestUrlForGroup(group_id);
    if (!manifest_url.has_value())
      return false;

    int64_t cache_padding_size = 0;
    if (!ForEachEntry(
            cache_id,
            [&](std::string response_url, int64_t response_id) -> bool {
              int64_t entry_padding_size =
                  ComputeEntryPaddingSize(response_url, manifest_url.value());
              cache_padding_size += entry_padding_size;
              return UpdateEntryPaddingSize(response_id, entry_padding_size);
            })) {
      return false;
    }

    return UpdateCachePaddingSize(cache_id, cache_padding_size);
  });
}

template <typename ForEachCallable>
bool AppCacheBackfillerVersion8::ForEachEntry(int64_t cache_id,
                                              const ForEachCallable& callable) {
  static const char kSql[] =
      "SELECT url, response_id, cache_id FROM Entries WHERE cache_id = ?";
  sql::Statement statement(db_->GetUniqueStatement(kSql));
  statement.BindInt64(0, cache_id);
  while (statement.Step()) {
    std::string url = statement.ColumnString(0);
    int64_t response_id = statement.ColumnInt64(1);
    if (!callable(url, response_id))
      return false;
  }
  return true;
}

base::Optional<std::string> AppCacheBackfillerVersion8::GetManifestUrlForGroup(
    int64_t group_id) {
  static const char kSql[] =
      "SELECT manifest_url, group_id FROM Groups WHERE group_id = ?";
  sql::Statement statement(db_->GetUniqueStatement(kSql));
  statement.BindInt64(0, group_id);
  if (!statement.Step())
    return base::nullopt;
  return statement.ColumnString(0);
}

bool AppCacheBackfillerVersion8::UpdateEntryPaddingSize(int64_t response_id,
                                                        int64_t padding_size) {
  static const char kSql[] =
      "UPDATE Entries SET padding_size = ? WHERE response_id = ?";
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, padding_size);
  statement.BindInt64(1, response_id);
  return statement.Run();
}

bool AppCacheBackfillerVersion8::UpdateCachePaddingSize(int64_t cache_id,
                                                        int64_t padding_size) {
  static const char kSql[] =
      "UPDATE Caches SET padding_size = ? WHERE cache_id = ?";
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, padding_size);
  statement.BindInt64(1, cache_id);
  return statement.Run();
}

// AppCacheBackfillerVersion9

bool AppCacheBackfillerVersion9::BackfillManifestParserVersionAndScope() {
  return ForEachCache(db_, [&](int64_t cache_id, int64_t group_id) -> bool {
    base::Optional<std::string> manifest_url = GetManifestUrlForGroup(group_id);
    if (!manifest_url.has_value())
      return false;

    // |manifest_parser_version| should be set to 0 to recognize that scope
    // checking wasn't enabled when this group record was written.
    int64_t manifest_parser_version = 0;
    if (!UpdateCacheManifestParserVersion(cache_id, manifest_parser_version))
      return false;

    // This is where pre-version 9 DBs will end up with a manifest scope
    // that previously didn't have one.  Update using the entire origin for a
    // scope.  Will end up getting "fixed" on the next update from version 0 to
    // version 1 once manifest scoping is enabled.
    std::string manifest_scope = "/";
    return UpdateCacheManifestScope(cache_id, manifest_scope);
  });
}

bool AppCacheBackfillerVersion9::UpdateCacheManifestParserVersion(
    int64_t cache_id,
    int64_t manifest_parser_version) {
  static const char kSql[] =
      "UPDATE Caches SET manifest_parser_version = ? WHERE"
      " cache_id = ?";
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, manifest_parser_version);
  statement.BindInt64(1, cache_id);
  return statement.Run();
}

bool AppCacheBackfillerVersion9::UpdateCacheManifestScope(
    int64_t cache_id,
    const std::string& manifest_scope) {
  static const char kSql[] =
      "UPDATE Caches SET manifest_scope = ? WHERE"
      " cache_id = ?";
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindString(0, manifest_scope);
  statement.BindInt64(1, cache_id);
  return statement.Run();
}

base::Optional<std::string> AppCacheBackfillerVersion9::GetManifestUrlForGroup(
    int64_t group_id) {
  static const char kSql[] =
      "SELECT manifest_url, group_id FROM Groups WHERE group_id = ?";
  sql::Statement statement(db_->GetUniqueStatement(kSql));
  statement.BindInt64(0, group_id);
  if (!statement.Step())
    return base::nullopt;
  return statement.ColumnString(0);
}

}  // namespace content
