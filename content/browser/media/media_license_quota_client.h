// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_MEDIA_LICENSE_QUOTA_CLIENT_H_
#define CONTENT_BROWSER_MEDIA_MEDIA_LICENSE_QUOTA_CLIENT_H_

#include "components/services/storage/public/mojom/quota_client.mojom.h"
#include "content/common/content_export.h"

namespace content {
class MediaLicenseManager;

// Integrates media licenses with the quota system.
//
// Each MediaLicenseManager owns exactly one MediaLicenseQuotaClient.
class CONTENT_EXPORT MediaLicenseQuotaClient
    : public storage::mojom::QuotaClient {
 public:
  explicit MediaLicenseQuotaClient(MediaLicenseManager* manager);
  ~MediaLicenseQuotaClient() override;

  // storage::mojom::QuotaClient implementation:
  void GetBucketUsage(const storage::BucketLocator& bucket,
                      GetBucketUsageCallback callback) override;
  void GetStorageKeysForType(blink::mojom::StorageType type,
                             GetStorageKeysForTypeCallback callback) override;
  void DeleteBucketData(const storage::BucketLocator& bucket,
                        DeleteBucketDataCallback callback) override;
  void PerformStorageCleanup(blink::mojom::StorageType type,
                             PerformStorageCleanupCallback callback) override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  const raw_ptr<MediaLicenseManager> manager_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_MEDIA_LICENSE_QUOTA_CLIENT_H_
