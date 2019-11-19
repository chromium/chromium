// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CACHE_STORAGE_CROSS_SEQUENCE_CROSS_SEQUENCE_CACHE_STORAGE_MANAGER_H_
#define CONTENT_BROWSER_CACHE_STORAGE_CROSS_SEQUENCE_CROSS_SEQUENCE_CACHE_STORAGE_MANAGER_H_

#include "base/sequence_checker.h"
#include "base/threading/sequence_bound.h"
#include "content/browser/cache_storage/cache_storage_manager.h"

namespace content {

class CacheStorageContextWithManager;

// A CacheStorageManager implementation that can be used from one sequence to
// access the real CacheStorageManager executing on a different sequence.  The
// CacheStorageContextImpl will create one of these whenever code calls the
// CacheManager() accessor on the wrong sequence.  The cross-sequence wrapper
// manager is ref-counted and will only live as long as that client code keeps
// its reference alive.  Each cross-sequence wrapper is locked to the sequence
// on which it was created.
class CONTENT_EXPORT CrossSequenceCacheStorageManager
    : public CacheStorageManager {
 public:
  CrossSequenceCacheStorageManager(
      scoped_refptr<base::SequencedTaskRunner> target_task_runner,
      scoped_refptr<CacheStorageContextWithManager> context);

  // CacheStorageManager
  CacheStorageHandle OpenCacheStorage(const url::Origin& origin,
                                      CacheStorageOwner owner) override;
  void GetAllOriginsUsage(
      CacheStorageOwner owner,
      CacheStorageContext::GetUsageInfoCallback callback) override;
  void GetOriginUsage(const url::Origin& origin_url,
                      CacheStorageOwner owner,
                      storage::QuotaClient::GetUsageCallback callback) override;
  void GetOrigins(CacheStorageOwner owner,
                  storage::QuotaClient::GetOriginsCallback callback) override;
  void GetOriginsForHost(
      const std::string& host,
      CacheStorageOwner owner,
      storage::QuotaClient::GetOriginsCallback callback) override;
  void DeleteOriginData(
      const url::Origin& origin,
      CacheStorageOwner owner,
      storage::QuotaClient::DeletionCallback callback) override;
  void DeleteOriginData(const url::Origin& origin,
                        CacheStorageOwner owner) override;
  void SetBlobParametersForCache(
      scoped_refptr<BlobStorageContextWrapper> blob_storage_context) override;

 private:
  ~CrossSequenceCacheStorageManager() override;

  const scoped_refptr<base::SequencedTaskRunner> target_task_runner_;
  const scoped_refptr<CacheStorageContextWithManager> context_;

  // The |inner_| object is SequenceBound<> to the target sequence used by the
  // real manager.
  class Inner;
  base::SequenceBound<Inner> inner_;

  SEQUENCE_CHECKER(sequence_checker_);
  DISALLOW_COPY_AND_ASSIGN(CrossSequenceCacheStorageManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_CACHE_STORAGE_CROSS_SEQUENCE_CROSS_SEQUENCE_CACHE_STORAGE_MANAGER_H_
