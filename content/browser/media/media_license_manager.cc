// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_license_manager.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/not_fatal_until.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "components/services/storage/public/cpp/buckets/constants.h"
#include "components/services/storage/public/cpp/constants.h"
#include "content/browser/media/media_license_database.h"
#include "content/browser/media/media_license_storage_host.h"
#include "media/cdm/cdm_type.h"
#include "sql/database.h"
#include "sql/sqlite_result_code.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom-shared.h"
#include "url/origin.h"

namespace content {

using MediaLicenseStorageHostOpenError =
    MediaLicenseStorageHost::MediaLicenseStorageHostOpenError;

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
    bool in_memory,
    scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy)
    : db_runner_(CreateDatabaseTaskRunner()),
      in_memory_(in_memory),
      special_storage_policy_(std::move(special_storage_policy)),
      quota_manager_proxy_(std::move(quota_manager_proxy)),
      // Using a raw pointer is safe since `quota_client_` is owned by
      // this instance.
      quota_client_(this),
      quota_client_receiver_(&quota_client_) {
  if (quota_manager_proxy_) {
    // Quota client assumes all backends have registered.
    quota_manager_proxy_->RegisterClient(
        quota_client_receiver_.BindNewPipeAndPassRemote(),
        storage::QuotaClientType::kMediaLicense,
        {blink::mojom::StorageType::kTemporary});
  }
}

MediaLicenseManager::~MediaLicenseManager() = default;

void MediaLicenseManager::OpenCdmStorage(
    const CdmStorageBindingContext& binding_context,
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
    // If a pending receiver for this storage key already existed, there is
    // an in-flight `UpdateOrCreateBucket()` call for this storage key.
    return;
  }

  // Get the default bucket for `storage_key`.
  quota_manager_proxy()->UpdateOrCreateBucket(
      storage::BucketInitParams::ForDefaultBucket(storage_key),
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce(&MediaLicenseManager::DidGetBucket,
                     weak_factory_.GetWeakPtr(), storage_key));
}

void MediaLicenseManager::DidGetBucket(
    const blink::StorageKey& storage_key,
    storage::QuotaErrorOr<storage::BucketInfo> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = pending_receivers_.find(storage_key);
  CHECK(it != pending_receivers_.end(), base::NotFatalUntil::M130);

  auto receivers_list = std::move(it->second);
  pending_receivers_.erase(it);
  DCHECK_GT(receivers_list.size(), 0u);

  storage::BucketLocator bucket_locator;
  if (result.has_value()) {
    bucket_locator = result->ToBucketLocator();
  } else {
    // Use the null locator, but update the `storage_key` field so
    // `storage_host` can be identified when it is to be removed from `hosts_`.
    // We could consider falling back to using an in-memory database in this
    // case, but failing here seems easier to reason about from a website
    // author's point of view.
    sql::UmaHistogramSqliteResult(
        "Media.EME.MediaLicenseDatabaseOpenSQLiteError",
        result.error().sqlite_error);
    base::UmaHistogramEnumeration(
        "Media.EME.MediaLicenseDatabaseOpenQuotaError",
        result.error().quota_error);
    MediaLicenseStorageHost::ReportDatabaseOpenError(
        MediaLicenseStorageHostOpenError::kBucketLocatorError, in_memory());
    DCHECK(bucket_locator.id.is_null());
    bucket_locator.storage_key = storage_key;
  }

  // All receivers associated with `storage_key` will be bound to the same host.
  auto storage_host =
      std::make_unique<MediaLicenseStorageHost>(this, bucket_locator);

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

  auto it_hosts = hosts_.find(bucket.storage_key);
  if (it_hosts != hosts_.end()) {
    // Let the host gracefully handle data deletion.
    it_hosts->second->DeleteBucketData(
        base::BindOnce(&MediaLicenseManager::DidDeleteBucketData,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }

  // If we have an in-memory profile, any data for the storage key would have
  // lived in the associated MediaLicenseStorageHost.
  if (in_memory()) {
    std::move(callback).Run(blink::mojom::QuotaStatusCode::kOk);
    return;
  }

  // Otherwise delete database file.
  auto path = GetDatabasePath(bucket);
  db_runner()->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&sql::Database::Delete, path),
      base::BindOnce(&MediaLicenseManager::DidDeleteBucketData,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void MediaLicenseManager::DidDeleteBucketData(
    storage::mojom::QuotaClient::DeleteBucketDataCallback callback,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::move(callback).Run(success ? blink::mojom::QuotaStatusCode::kOk
                                  : blink::mojom::QuotaStatusCode::kUnknown);
}

base::FilePath MediaLicenseManager::GetDatabasePath(
    const storage::BucketLocator& bucket_locator) {
  if (in_memory())
    return base::FilePath();

  auto media_license_dir = quota_manager_proxy()->GetClientBucketPath(
      bucket_locator, storage::QuotaClientType::kMediaLicense);
  return media_license_dir.Append(storage::kMediaLicenseDatabaseFileName);
}

void MediaLicenseManager::OnHostReceiverDisconnect(
    MediaLicenseStorageHost* host,
    base::PassKey<MediaLicenseStorageHost> pass_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(host);

  if (in_memory()) {
    // Don't delete `host` for an in-memory profile, since the data is not safe
    // to delete yet. For example, a site may be re-visited within the same
    // incognito session. `host` will be destroyed when `this` is destroyed.
    return;
  }

  DCHECK_GT(hosts_.count(host->storage_key()), 0ul);
  DCHECK_EQ(hosts_[host->storage_key()].get(), host);

  if (!host->has_empty_receiver_set())
    return;

  size_t count_removed = hosts_.erase(host->storage_key());
  DCHECK_EQ(count_removed, 1u);
}

}  // namespace content
