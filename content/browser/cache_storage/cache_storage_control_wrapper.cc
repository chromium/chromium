// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/cache_storage_control_wrapper.h"
#include "base/task/sequenced_task_runner.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {

CacheStorageControlWrapper::CacheStorageControlWrapper(
    scoped_refptr<base::SequencedTaskRunner> io_task_runner,
    const base::FilePath& user_data_directory,
    scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
    mojo::PendingRemote<storage::mojom::BlobStorageContext>
        blob_storage_context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(quota_manager_proxy);

  // QuotaManagerProxy::RegisterClient() must be called during construction
  // until crbug.com/1182630 is fixed.
  mojo::PendingRemote<storage::mojom::QuotaClient> cache_storage_client_remote;
  mojo::PendingReceiver<storage::mojom::QuotaClient>
      cache_storage_client_receiver =
          cache_storage_client_remote.InitWithNewPipeAndPassReceiver();
  quota_manager_proxy->RegisterClient(
      std::move(cache_storage_client_remote),
      storage::QuotaClientType::kServiceWorkerCache,
      {blink::mojom::StorageType::kTemporary});
  mojo::PendingRemote<storage::mojom::QuotaClient>
      background_fetch_client_remote;
  mojo::PendingReceiver<storage::mojom::QuotaClient>
      background_fetch_client_receiver =
          background_fetch_client_remote.InitWithNewPipeAndPassReceiver();
  quota_manager_proxy->RegisterClient(
      std::move(background_fetch_client_remote),
      storage::QuotaClientType::kBackgroundFetch,
      {blink::mojom::StorageType::kTemporary});

  cache_storage_context_ = base::SequenceBound<CacheStorageContextImpl>(
      CacheStorageContextImpl::CreateSchedulerTaskRunner(),
      std::move(quota_manager_proxy));
  cache_storage_context_.AsyncCall(&CacheStorageContextImpl::Init)
      .WithArgs(cache_storage_control_.BindNewPipeAndPassReceiver(),
                user_data_directory, std::move(cache_storage_client_receiver),
                std::move(background_fetch_client_receiver),
                std::move(blob_storage_context));

  if (special_storage_policy) {
    // `storage_policy_observer_` is owned by `this` and so it is safe
    // to use base::Unretained here.
    storage_policy_observer_.emplace(
        base::BindRepeating(&CacheStorageControlWrapper::ApplyPolicyUpdates,
                            base::Unretained(this)),
        std::move(io_task_runner), std::move(special_storage_policy));
  }
}

CacheStorageControlWrapper::~CacheStorageControlWrapper() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void CacheStorageControlWrapper::AddReceiver(
    const network::CrossOriginEmbedderPolicy& cross_origin_embedder_policy,
    mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
        coep_reporter_remote,
    const network::DocumentIsolationPolicy& document_isolation_policy,
    const storage::BucketLocator& bucket,
    storage::mojom::CacheStorageOwner owner,
    mojo::PendingReceiver<blink::mojom::CacheStorage> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (storage_policy_observer_)
    storage_policy_observer_->StartTrackingOrigin(bucket.storage_key.origin());
  cache_storage_control_->AddReceiver(
      cross_origin_embedder_policy, std::move(coep_reporter_remote),
      document_isolation_policy, bucket, owner, std::move(receiver));
}

void CacheStorageControlWrapper::AddObserver(
    mojo::PendingRemote<storage::mojom::CacheStorageObserver> observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  cache_storage_control_->AddObserver(std::move(observer));
}

void CacheStorageControlWrapper::ApplyPolicyUpdates(
    std::vector<storage::mojom::StoragePolicyUpdatePtr> policy_updates) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  cache_storage_control_->ApplyPolicyUpdates(std::move(policy_updates));
}

}  // namespace content
