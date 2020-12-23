// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/system_storage/storage_info_provider.h"

#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "components/storage_monitor/storage_info.h"
#include "components/storage_monitor/storage_monitor.h"
#include "content/public/browser/browser_thread.h"

using storage_monitor::StorageInfo;
using storage_monitor::StorageMonitor;

namespace extensions {

using content::BrowserThread;
using api::system_storage::StorageUnitInfo;
using api::system_storage::STORAGE_UNIT_TYPE_FIXED;
using api::system_storage::STORAGE_UNIT_TYPE_REMOVABLE;

namespace systeminfo {

void BuildStorageUnitInfo(const StorageInfo& info, StorageUnitInfo* unit) {
  unit->id = StorageMonitor::GetInstance()->GetTransientIdForDeviceId(
      info.device_id());
  unit->name = base::UTF16ToUTF8(info.GetDisplayName(false));
  // TODO(hmin): Might need to take MTP device into consideration.
  unit->type = StorageInfo::IsRemovableDevice(info.device_id())
                   ? STORAGE_UNIT_TYPE_REMOVABLE
                   : STORAGE_UNIT_TYPE_FIXED;
  unit->capacity = static_cast<double>(info.total_size_in_bytes());
}

}  // namespace systeminfo

// Static member intialization.
base::LazyInstance<scoped_refptr<StorageInfoProvider>>::DestructorAtExit
    StorageInfoProvider::provider_ = LAZY_INSTANCE_INITIALIZER;

StorageInfoProvider::StorageInfoProvider() {
}

StorageInfoProvider::~StorageInfoProvider() {
}

void StorageInfoProvider::InitializeForTesting(
    scoped_refptr<StorageInfoProvider> provider) {
  DCHECK(provider.get() != NULL);
  provider_.Get() = provider;
}

void StorageInfoProvider::PrepareQueryOnUIThread() {
  // Get all available storage devices before invoking |QueryInfo()|.
  GetAllStoragesIntoInfoList();
}

void StorageInfoProvider::InitializeProvider(
    base::OnceClosure do_query_info_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Register the |do_query_info_callback| callback to StorageMonitor.
  // See the comments of StorageMonitor::EnsureInitialized about when the
  // callback gets run.
  StorageMonitor::GetInstance()->EnsureInitialized(
      std::move(do_query_info_callback));
}

bool StorageInfoProvider::QueryInfo() {
  // No info to query since we get all available storage devices' info in
  // |PrepareQueryOnUIThread()|.
  return true;
}

void StorageInfoProvider::GetAllStoragesIntoInfoList() {
  info_.clear();
  std::vector<StorageInfo> storage_list =
      StorageMonitor::GetInstance()->GetAllAvailableStorages();

  for (std::vector<StorageInfo>::const_iterator it = storage_list.begin();
       it != storage_list.end();
       ++it) {
    StorageUnitInfo unit;
    systeminfo::BuildStorageUnitInfo(*it, &unit);
    info_.push_back(std::move(unit));
  }
}

double StorageInfoProvider::GetStorageFreeSpaceFromTransientIdAsync(
    const std::string& transient_id) {
  std::vector<StorageInfo> storage_list =
      StorageMonitor::GetInstance()->GetAllAvailableStorages();

  std::string device_id =
      StorageMonitor::GetInstance()->GetDeviceIdForTransientId(transient_id);

  // Lookup the matched storage info by |device_id|.
  for (std::vector<StorageInfo>::const_iterator it = storage_list.begin();
       it != storage_list.end();
       ++it) {
    if (device_id == it->device_id())
      return static_cast<double>(
          base::SysInfo::AmountOfFreeDiskSpace(base::FilePath(it->location())));
  }

  return -1;
}

// static
StorageInfoProvider* StorageInfoProvider::Get() {
  if (provider_.Get().get() == NULL)
    provider_.Get() = new StorageInfoProvider();
  return provider_.Get().get();
}

}  // namespace extensions
