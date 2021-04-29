// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_APPCACHE_APPCACHE_CACHE_TEST_HELPER_H_
#define CONTENT_BROWSER_APPCACHE_APPCACHE_CACHE_TEST_HELPER_H_

#include "content/browser/appcache/mock_appcache_service.h"

namespace content {

// Helper class to read cache info from and write cache info to disk.
class AppCacheCacheTestHelper : public AppCacheStorage::Delegate {
 public:
  // Helpers to collect cache entry information in a single location.
  struct CacheEntry {
    CacheEntry(int types,
               std::string expect_if_modified_since,
               std::string expect_if_none_match,
               bool headers_allowed,
               std::unique_ptr<net::HttpResponseInfo> response_info,
               const std::string& body);
    ~CacheEntry();

    int types;  // The combination of AppCacheEntry::Type values for this entry.
    std::string expect_if_modified_since;
    std::string expect_if_none_match;
    bool headers_allowed = true;
    std::unique_ptr<net::HttpResponseInfo> response_info;
    std::string body;
  };

  using CacheEntries = std::map<GURL, std::unique_ptr<CacheEntry>>;

  static void AddCacheEntry(
      CacheEntries* cache_entries,
      const GURL& url,
      int types,
      std::string expect_if_modified_since,
      std::string expect_if_none_match,
      bool headers_allowed,
      std::unique_ptr<net::HttpResponseInfo> response_info,
      const std::string& body);

  AppCacheCacheTestHelper(const MockAppCacheService* service,
                          const GURL& manifest_url,
                          AppCache* const cache,
                          CacheEntries cache_entries,
                          base::OnceCallback<void(int)> post_write_callback);
  ~AppCacheCacheTestHelper() override;

  AppCache* write_cache() { return cache_; }
  const CacheEntries& cache_entries() { return cache_entries_; }
  const CacheEntries& read_cache_entries() { return read_cache_entries_; }

  void PrepareForRead(AppCache* cache, base::OnceClosure post_read_callback);
  void Read();
  void Write();
  void OnResponseInfoLoaded(AppCacheResponseInfo* response_info,
                            int64_t response_id) override;

 private:
  enum class State {
    kIdle,
    kReadInfo,
    kReadData,
    kWriteInfo,
    kWriteData,
  };

  void AsyncRead(int result);
  void AsyncWrite(int result);

  const MockAppCacheService* const service_;
  const GURL manifest_url_;
  AppCache* const cache_;
  CacheEntries cache_entries_;
  State state_;

  // Used for writing cache info and data.
  CacheEntries::const_iterator write_it_;
  std::unique_ptr<AppCacheResponseWriter> response_writer_;
  base::OnceCallback<void(int)> post_write_callback_;

  // Used for reading cache info and data.
  AppCache* read_cache_;
  AppCache::EntryMap::const_iterator read_it_;
  int64_t read_entry_response_id_;
  scoped_refptr<AppCacheResponseInfo> read_info_response_info_;
  scoped_refptr<net::IOBuffer> read_data_buffer_;
  std::string read_data_loaded_data_;
  std::unique_ptr<AppCacheResponseReader> read_data_response_reader_;
  CacheEntries read_cache_entries_;
  base::OnceClosure post_read_callback_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_APPCACHE_APPCACHE_CACHE_TEST_HELPER_H_
