// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/cross_sequence/cross_sequence_cache_storage_manager.h"

#include "content/browser/cache_storage/cache_storage.h"
#include "content/browser/cache_storage/cache_storage_context_impl.h"
#include "content/browser/cache_storage/cross_sequence/cross_sequence_cache_storage.h"
#include "content/browser/cache_storage/cross_sequence/cross_sequence_utils.h"

namespace content {

// The Inner class is SequenceBound<> to the real target manager sequence by
// the outer CrossSequenceCacheStorageManager.  All CacheStorageManager
// operations are proxied to the Inner on the correct sequence via the Post()
// method.  The outer manager is responsible for wrapping any callbacks in
// order to post on the outer's original sequence.
class CrossSequenceCacheStorageManager::Inner {
 public:
  explicit Inner(scoped_refptr<CacheStorageContextWithManager> context)
      : target_manager_(context->CacheManager()) {}

  void GetAllOriginsUsage(CacheStorageOwner owner,
                          CacheStorageContext::GetUsageInfoCallback callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    target_manager_->GetAllOriginsUsage(owner, std::move(callback));
  }

  void GetOriginUsage(const url::Origin& origin_url,
                      CacheStorageOwner owner,
                      storage::QuotaClient::GetUsageCallback callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    target_manager_->GetOriginUsage(origin_url, owner, std::move(callback));
  }

  void GetOrigins(CacheStorageOwner owner,
                  storage::QuotaClient::GetOriginsCallback callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    target_manager_->GetOrigins(owner, std::move(callback));
  }

  void GetOriginsForHost(const std::string& host,
                         CacheStorageOwner owner,
                         storage::QuotaClient::GetOriginsCallback callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    target_manager_->GetOriginsForHost(host, owner, std::move(callback));
  }

  void DeleteOriginData(const url::Origin& origin,
                        CacheStorageOwner owner,
                        storage::QuotaClient::DeletionCallback callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    target_manager_->DeleteOriginData(origin, owner, std::move(callback));
  }

 private:
  const scoped_refptr<CacheStorageManager> target_manager_;
  SEQUENCE_CHECKER(sequence_checker_);
};

CrossSequenceCacheStorageManager::CrossSequenceCacheStorageManager(
    scoped_refptr<base::SequencedTaskRunner> target_task_runner,
    scoped_refptr<CacheStorageContextWithManager> context)
    : target_task_runner_(std::move(target_task_runner)),
      context_(context),
      inner_(target_task_runner_, std::move(context)) {}

CacheStorageHandle CrossSequenceCacheStorageManager::OpenCacheStorage(
    const url::Origin& origin,
    CacheStorageOwner owner) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Construct the CrossSequenceCacheStorage object immediately on our current
  // sequence.  This is necessary in order to return a Handle synchronously.
  // The CrossSequenceCacheStorage object will asynchronously open the real
  // CacheStorage on the correct sequence.
  auto storage = base::MakeRefCounted<CrossSequenceCacheStorage>(
      origin, owner, target_task_runner_, context_);
  return storage->CreateHandle();
}

void CrossSequenceCacheStorageManager::GetAllOriginsUsage(
    CacheStorageOwner owner,
    CacheStorageContext::GetUsageInfoCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  inner_.Post(FROM_HERE, &Inner::GetAllOriginsUsage, owner,
              WrapCallbackForCurrentSequence(std::move(callback)));
}

void CrossSequenceCacheStorageManager::GetOriginUsage(
    const url::Origin& origin_url,
    CacheStorageOwner owner,
    storage::QuotaClient::GetUsageCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  inner_.Post(FROM_HERE, &Inner::GetOriginUsage, origin_url, owner,
              WrapCallbackForCurrentSequence(std::move(callback)));
}

void CrossSequenceCacheStorageManager::GetOrigins(
    CacheStorageOwner owner,
    storage::QuotaClient::GetOriginsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  inner_.Post(FROM_HERE, &Inner::GetOrigins, owner,
              WrapCallbackForCurrentSequence(std::move(callback)));
}

void CrossSequenceCacheStorageManager::GetOriginsForHost(
    const std::string& host,
    CacheStorageOwner owner,
    storage::QuotaClient::GetOriginsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  inner_.Post(FROM_HERE, &Inner::GetOriginsForHost, host, owner,
              WrapCallbackForCurrentSequence(std::move(callback)));
}

void CrossSequenceCacheStorageManager::DeleteOriginData(
    const url::Origin& origin,
    CacheStorageOwner owner,
    storage::QuotaClient::DeletionCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  inner_.Post(FROM_HERE, &Inner::DeleteOriginData, origin, owner,
              WrapCallbackForCurrentSequence(std::move(callback)));
}

void CrossSequenceCacheStorageManager::DeleteOriginData(
    const url::Origin& origin,
    CacheStorageOwner owner) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DeleteOriginData(origin, owner, base::DoNothing());
}

void CrossSequenceCacheStorageManager::SetBlobParametersForCache(
    scoped_refptr<BlobStorageContextWrapper> blob_storage_context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // This method is used for initialization of a real manager and should not
  // be invoked for the cross-sequence wrapper.
  NOTREACHED();
}

CrossSequenceCacheStorageManager::~CrossSequenceCacheStorageManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

}  // namespace content
