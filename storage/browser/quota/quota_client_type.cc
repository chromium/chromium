// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/quota/quota_client_type.h"

#include "base/no_destructor.h"

namespace storage {

const QuotaClientTypes& AllQuotaClientTypes() {
  static base::NoDestructor<QuotaClientTypes> all{{
      QuotaClientType::kFileSystem,
      QuotaClientType::kDatabase,
      QuotaClientType::kIndexedDatabase,
      QuotaClientType::kServiceWorkerCache,
      QuotaClientType::kServiceWorker,
      QuotaClientType::kBackgroundFetch,
      QuotaClientType::kMediaLicense,
  }};
  return *all;
}

}  // namespace storage
