// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_license_quota_client.h"

#include "base/sequence_checker.h"
#include "content/browser/media/media_license_manager.h"

namespace content {

MediaLicenseQuotaClient::MediaLicenseQuotaClient(MediaLicenseManager* manager)
    : manager_(manager) {}

MediaLicenseQuotaClient::~MediaLicenseQuotaClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void MediaLicenseQuotaClient::GetBucketUsage(
    const storage::BucketLocator& bucket,
    GetBucketUsageCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Media license data does not count against quota.
  // TODO(crbug.com/40218094): Consider counting this data against quota.
  std::move(callback).Run(0);
}

void MediaLicenseQuotaClient::GetStorageKeysForType(
    blink::mojom::StorageType type,
    GetStorageKeysForTypeCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(type, blink::mojom::StorageType::kTemporary);

  // This method is only used to bootstrap existing Storage API data into the
  // QuotaDatabase. Since this is a new backend, there is no existing data to
  // bootstrap.
  std::move(callback).Run({});
}

void MediaLicenseQuotaClient::DeleteBucketData(
    const storage::BucketLocator& bucket,
    DeleteBucketDataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  manager_->DeleteBucketData(bucket, std::move(callback));
}

void MediaLicenseQuotaClient::PerformStorageCleanup(
    blink::mojom::StorageType type,
    PerformStorageCleanupCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Nothing to do here.
  std::move(callback).Run();
}

}  // namespace content