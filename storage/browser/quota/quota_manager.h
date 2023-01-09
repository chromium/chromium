// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_QUOTA_QUOTA_MANAGER_H_
#define STORAGE_BROWSER_QUOTA_QUOTA_MANAGER_H_

#include "base/task/single_thread_task_runner.h"
#include "storage/browser/quota/quota_manager_impl.h"

namespace storage {

// QuotaManager will eventually become a mojo interface facilitating
// inter-process access to QuotaManagerImpl. As an intermediary step,
// QuotaManager will become an abstract base class, which will be implemented by
// QuotaManagerProxy.
//
// As a first step, QuotaManager is an alias for QuotaManagerImpl.
class COMPONENT_EXPORT(STORAGE_BROWSER) QuotaManager : public QuotaManagerImpl {
 public:
  QuotaManager(bool is_incognito,
               const base::FilePath& profile_path,
               scoped_refptr<base::SingleThreadTaskRunner> io_thread,
               base::RepeatingClosure quota_change_callback,
               scoped_refptr<SpecialStoragePolicy> special_storage_policy,
               const GetQuotaSettingsFunc& get_settings_function);

 protected:
  ~QuotaManager() override;
};

}  // namespace storage

#endif  // STORAGE_BROWSER_QUOTA_QUOTA_MANAGER_H_
