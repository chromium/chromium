// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/quota/storage_directory_util.h"

#include "components/services/storage/public/cpp/constants.h"

namespace storage {

base::FilePath CreateBucketPath(const base::FilePath& profile_path,
                                const BucketLocator& bucket) {
  return profile_path.Append(kWebStorageDirectory)
      .AppendASCII(base::NumberToString(bucket.id.value()));
}

base::FilePath CreateClientBucketPath(const base::FilePath& profile_path,
                                      const BucketLocator& bucket,
                                      QuotaClientType client_type) {
  base::FilePath bucket_directory = CreateBucketPath(profile_path, bucket);

  switch (client_type) {
    case QuotaClientType::kFileSystem:
      return bucket_directory.Append(kFileSystemDirectory);
    case QuotaClientType::kIndexedDatabase:
      return bucket_directory.Append(kIndexedDbDirectory);
    case QuotaClientType::kBackgroundFetch:
      return bucket_directory.Append(kBackgroundFetchDirectory);
    case QuotaClientType::kServiceWorkerCache:
      return bucket_directory.Append(kCacheStorageDirectory);
    case QuotaClientType::kServiceWorker:
      return bucket_directory.Append(kScriptCacheDirectory);
    case QuotaClientType::kMediaLicense:
      return bucket_directory.Append(kMediaLicenseDirectory);
    case QuotaClientType::kDatabase:
      NOTREACHED() << "Unsupported QuotaClientType";
  }
}

}  // namespace storage
