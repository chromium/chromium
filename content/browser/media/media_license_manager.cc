// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_license_manager.h"

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/services/storage/public/cpp/buckets/constants.h"
#include "components/services/storage/public/cpp/constants.h"
#include "content/browser/media/media_license_storage_host.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom-shared.h"

namespace content {

namespace {

// Creates a task runner suitable for running SQLite database operations.
scoped_refptr<base::SequencedTaskRunner> CreateDatabaseTaskRunner() {
  // We use a SequencedTaskRunner so that there is a global ordering to a
  // storage key's directory operations.
  return base::ThreadPool::CreateSequencedTaskRunner({
      // Needed for file I/O.
      base::MayBlock(),

      // Reasonable compromise, given that a few database operations are
      // blocking, while most operations are not. We should be able to do better
      // when we get scheduling APIs on the Web Platform.
      base::TaskPriority::USER_VISIBLE,

      // Needed to allow for clearing site data on shutdown.
      base::TaskShutdownBehavior::BLOCK_SHUTDOWN,
  });
}

}  // namespace

MediaLicenseManager::MediaLicenseManager(
    const base::FilePath& bucket_base_path,
    scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy)
    : db_runner_(CreateDatabaseTaskRunner()),
      bucket_base_path_(bucket_base_path),
      special_storage_policy_(std::move(special_storage_policy)),
      quota_manager_proxy_(std::move(quota_manager_proxy)),
      // Using a raw pointer is safe since `quota_client_` is owned by
      // this instance.
      quota_client_(this),
      quota_client_receiver_(&quota_client_) {
  // TODO(crbug.com/1231162): Register a new backend with the quota client.
}

MediaLicenseManager::~MediaLicenseManager() = default;

void MediaLicenseManager::OpenCdmStorage(
    const BindingContext& binding_context,
    mojo::PendingReceiver<media::mojom::CdmStorage> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto& storage_key = binding_context.storage_key;
  auto it_hosts = hosts_.find(storage_key);
  if (it_hosts != hosts_.end()) {
    // A storage host for this storage key already exists.
    it_hosts->second->BindReceiver(binding_context, std::move(receiver));
    return;
  }

  auto& receiver_list = pending_receivers_[storage_key];
  receiver_list.emplace_back(binding_context, std::move(receiver));
  if (receiver_list.size() > 1) {
    // A pending receiver for this storage key already existed, meaning there is
    // an in-flight `GetOrCreateBucket()` call for this storage key.
    return;
  }

  // Get the default bucket for `storage_key`.
  quota_manager_proxy_->GetOrCreateBucket(
      storage_key, storage::kDefaultBucketName,
      base::SequencedTaskRunnerHandle::Get(),
      base::BindOnce(&MediaLicenseManager::DidGetBucket,
                     weak_factory_.GetWeakPtr(), storage_key));
}

void MediaLicenseManager::DidGetBucket(
    const blink::StorageKey& storage_key,
    storage::QuotaErrorOr<storage::BucketInfo> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(crbug.com/1231162): Handle failure case.
  DCHECK(result.ok());

  // All receivers associated with `storage_key` will be bound to the same host.
  auto storage_host = std::make_unique<MediaLicenseStorageHost>(
      this, result->ToBucketLocator());

  auto it = pending_receivers_.find(storage_key);
  DCHECK(it != pending_receivers_.end());

  auto receivers_list = std::move(it->second);
  pending_receivers_.erase(it);
  DCHECK_GT(receivers_list.size(), 0u);

  for (auto& context_and_receiver : receivers_list) {
    storage_host->BindReceiver(context_and_receiver.first,
                               std::move(context_and_receiver.second));
  }

  hosts_.emplace(storage_key, std::move(storage_host));
}

void MediaLicenseManager::DeleteBucketData(
    const storage::BucketLocator& bucket,
    storage::mojom::QuotaClient::DeleteBucketDataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(crbug.com/1231162): Delete all media license data for `bucket`.

  std::move(callback).Run(blink::mojom::QuotaStatusCode::kOk);
}

base::FilePath MediaLicenseManager::GetDatabasePath(
    const storage::BucketLocator& bucket_locator) {
  if (bucket_base_path_.empty()) {
    // The bucket is in-memory.
    return base::FilePath();
  }

  // The media license database for a given bucket lives at:
  //
  // $PROFILE/WebStorage/<bucketID>/`kMediaLicenseDatabaseFileName`
  base::FilePath bucket_path = bucket_base_path_.AppendASCII(
      base::NumberToString(bucket_locator.id.value()));
  return bucket_path.Append(storage::kMediaLicenseDatabaseFileName);
}

void MediaLicenseManager::OnHostReceiverDisconnect(
    MediaLicenseStorageHost* host,
    base::PassKey<MediaLicenseStorageHost> pass_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(host);

  DCHECK_GT(hosts_.count(host->storage_key()), 0ul);
  DCHECK_EQ(hosts_[host->storage_key()].get(), host);

  if (!host->has_empty_receiver_set())
    return;

  size_t count_removed = hosts_.erase(host->storage_key());
  DCHECK_EQ(count_removed, 1u);
}

}  // namespace content
