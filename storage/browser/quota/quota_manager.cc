// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/quota/quota_manager.h"
#include "base/task/single_thread_task_runner.h"

#include <utility>

namespace storage {

QuotaManager::QuotaManager(
    bool is_incognito,
    const base::FilePath& profile_path,
    scoped_refptr<base::SingleThreadTaskRunner> io_thread,
    scoped_refptr<SpecialStoragePolicy> special_storage_policy,
    const GetQuotaSettingsFunc& get_settings_function,
    bool report_static_storage_quota)
    : QuotaManagerImpl(is_incognito,
                       profile_path,
                       std::move(io_thread),
                       std::move(special_storage_policy),
                       get_settings_function,
                       report_static_storage_quota) {}

QuotaManager::~QuotaManager() = default;

}  // namespace storage
