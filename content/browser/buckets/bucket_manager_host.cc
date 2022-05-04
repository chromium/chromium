// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/buckets/bucket_manager_host.h"

#include "base/containers/contains.h"
#include "base/strings/string_util.h"
#include "base/types/pass_key.h"
#include "components/services/storage/public/cpp/buckets/bucket_info.h"
#include "content/browser/buckets/bucket_manager.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {

namespace {

bool IsValidBucketName(const std::string& name) {
  // Details on bucket name validation and reasoning explained in
  // https://github.com/WICG/storage-buckets/blob/gh-pages/explainer.md
  if (name.empty() || name.length() >= 64)
    return false;

  // The name must only contain characters in a restricted set.
  for (char ch : name) {
    if (base::IsAsciiLower(ch))
      continue;
    if (base::IsAsciiDigit(ch))
      continue;
    if (ch == '_' || ch == '-')
      continue;
    return false;
  }

  // The first character in the name is more restricted.
  if (name[0] == '_' || name[0] == '-')
    return false;
  return true;
}

}  // namespace

BucketManagerHost::BucketManagerHost(BucketManager* manager, url::Origin origin)
    : manager_(manager), origin_(std::move(origin)) {
  DCHECK(manager != nullptr);

  // base::Unretained is safe here because this BucketManagerHost owns
  // `receivers_`. So, the unretained BucketManagerHost is guaranteed to
  // outlive |receivers_| and the closure that it uses.
  receivers_.set_disconnect_handler(base::BindRepeating(
      &BucketManagerHost::OnReceiverDisconnect, base::Unretained(this)));
}

BucketManagerHost::~BucketManagerHost() = default;

void BucketManagerHost::BindReceiver(
    mojo::PendingReceiver<blink::mojom::BucketManagerHost> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  receivers_.Add(this, std::move(receiver));
}

void BucketManagerHost::OpenBucket(const std::string& name,
                                   blink::mojom::BucketPoliciesPtr policies,
                                   OpenBucketCallback callback) {
  if (!IsValidBucketName(name)) {
    receivers_.ReportBadMessage("Invalid bucket name");
    return;
  }

  auto it = bucket_map_.find(name);
  if (it != bucket_map_.end()) {
    std::move(callback).Run(it->second->CreateStorageBucketBinding());
    return;
  }

  storage::BucketInitParams params((blink::StorageKey(origin_)));
  params.name = name;
  if (policies) {
    if (policies->expires)
      params.expiration = *policies->expires;

    params.quota = policies->quota;
  }
  manager_->quota_manager_proxy()->GetOrCreateBucket(
      params, base::SequencedTaskRunnerHandle::Get(),
      base::BindOnce(&BucketManagerHost::DidGetBucket,
                     weak_factory_.GetWeakPtr(), std::move(policies),
                     std::move(callback)));
}

void BucketManagerHost::Keys(KeysCallback callback) {
  std::vector<std::string> keys;
  for (auto& bucket : bucket_map_)
    keys.push_back(bucket.first);
  // TODO(ayui): Update to retrieve from QuotaManager.
  std::move(callback).Run(keys, true);
}

void BucketManagerHost::DeleteBucket(const std::string& name,
                                     DeleteBucketCallback callback) {
  if (!IsValidBucketName(name)) {
    receivers_.ReportBadMessage("Invalid bucket name");
    return;
  }

  manager_->quota_manager_proxy()->DeleteBucket(
      blink::StorageKey(origin_), name, base::SequencedTaskRunnerHandle::Get(),
      base::BindOnce(&BucketManagerHost::DidDeleteBucket,
                     weak_factory_.GetWeakPtr(), name, std::move(callback)));
}

void BucketManagerHost::RemoveBucketHost(const std::string& bucket_name) {
  DCHECK(base::Contains(bucket_map_, bucket_name));
  bucket_map_.erase(bucket_name);
}

void BucketManagerHost::OnReceiverDisconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  manager_->OnHostReceiverDisconnect(this, base::PassKey<BucketManagerHost>());
}

void BucketManagerHost::DidGetBucket(
    blink::mojom::BucketPoliciesPtr policy,
    OpenBucketCallback callback,
    storage::QuotaErrorOr<storage::BucketInfo> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!result.ok()) {
    // Getting a bucket can fail if there is a database error.
    std::move(callback).Run(mojo::NullRemote());
    return;
  }

  const auto& bucket = result.value();
  auto bucket_host =
      std::make_unique<BucketHost>(this, bucket, std::move(policy));
  auto pending_remote = bucket_host->CreateStorageBucketBinding();
  bucket_map_.emplace(bucket.name, std::move(bucket_host));
  std::move(callback).Run(std::move(pending_remote));
}

void BucketManagerHost::DidDeleteBucket(const std::string& bucket_name,
                                        DeleteBucketCallback callback,
                                        blink::mojom::QuotaStatusCode status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (status != blink::mojom::QuotaStatusCode::kOk) {
    std::move(callback).Run(false);
    return;
  }
  bucket_map_.erase(bucket_name);
  std::move(callback).Run(true);
}

}  // namespace content
