// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/test/mock_quota_database.h"
#include "mock_quota_database.h"

namespace storage {

MockQuotaDatabase::MockQuotaDatabase(const base::FilePath& path)
    : QuotaDatabase(path) {}
MockQuotaDatabase::~MockQuotaDatabase() = default;

}  // namespace storage
