// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_QUOTA_QUOTA_LIMIT_TYPE_H_
#define STORAGE_BROWSER_QUOTA_QUOTA_LIMIT_TYPE_H_

namespace storage {

enum QuotaLimitType {
  kQuotaLimitTypeUnknown,
  kQuotaLimitTypeLimited,
  kQuotaLimitTypeUnlimited,
  kQuotaLimitTypeLast = kQuotaLimitTypeUnlimited
};

}  // namespace storage

#endif  // STORAGE_BROWSER_QUOTA_QUOTA_LIMIT_TYPE_H_
