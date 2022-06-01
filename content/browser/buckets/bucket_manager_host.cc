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
    mojo::PendingReceiver<blink::mojom::BucketManagerHost> receiver,
    const BucketHost::PermissionDecisionCallback& permission_decision) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  permission_decider_map_.emplace(receivers_.Add(this, std::move(receiver)),
                                  permission_decision);
}

void BucketManagerHost::OpenBucket(const std::string& name,
                                   blink::mojom::BucketPoliciesPtr policies,
                                   OpenBucketCallback callback) {
  if (!IsValidBucketName(name)) {
    receivers_.ReportBadMessage("Invalid bucket name");
    return;
  }

  storage::BucketInitParams params(blink::StorageKey(origin_), name);
  if (policies) {
    if (policies->expires)
      params.expiration = *policies->expires;

    if (policies->has_quota)
      params.quota = policies->quota;

    if (policies->has_durability)
      params.durability = policies->durability;

    if (policies->has_persisted) {
      // Only grant persistence if permitted.
      auto it = permission_decider_map_.find(receivers_.current_receiver());
      if (it == permission_decider_map_.end()) {
        NOTREACHED();
        receivers_.ReportBadMessage("Internal error");
        return;
      }
      if (it->second.Run(blink::PermissionType::DURABLE_STORAGE) ==
          blink::mojom::PermissionStatus::GRANTED) {
        params.persistent = policies->persisted;
      }
    }
  }

  manager_->quota_manager_proxy()->UpdateOrCreateBucket(
      params, base::SequencedTaskRunnerHandle::Get(),
      base::BindOnce(&BucketManagerHost::DidGetBucket,
                     weak_factory_.GetWeakPtr(), receivers_.current_receiver(),
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

storage::QuotaManagerProxy* BucketManagerHost::GetQuotaManagerProxy() {
  return manager_->quota_manager_proxy().get();
}

void BucketManagerHost::OnReceiverDisconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  permission_decider_map_.erase(receivers_.current_receiver());
  manager_->OnHostReceiverDisconnect(this, base::PassKey<BucketManagerHost>());
}

void BucketManagerHost::DidGetBucket(
    mojo::ReceiverId receiver_id,
    OpenBucketCallback callback,
    storage::QuotaErrorOr<storage::BucketInfo> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!result.ok() || !receivers_.HasReceiver(receiver_id)) {
    // Getting a bucket can fail if there is a database error, and the receiver
    // could have been disconnected by now.
    std::move(callback).Run(mojo::NullRemote());
    return;
  }

  const auto& bucket = result.value();
  auto it = bucket_map_.find(bucket.name);
  if (it == bucket_map_.end()) {
    it = bucket_map_
             .emplace(bucket.name, std::make_unique<BucketHost>(this, bucket))
             .first;
  }

  auto permission_it = permission_decider_map_.find(receiver_id);
  CHECK(permission_it != permission_decider_map_.end());
  auto pending_remote =
      it->second->CreateStorageBucketBinding(permission_it->second);
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
