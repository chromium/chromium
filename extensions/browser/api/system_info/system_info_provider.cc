// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/system_info/system_info_provider.h"

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/storage_monitor/storage_info.h"
#include "components/storage_monitor/storage_monitor.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/api/system_storage.h"

using storage_monitor::StorageInfo;
using storage_monitor::StorageMonitor;

namespace extensions {

using api::system_storage::StorageUnitInfo;
using api::system_storage::StorageUnitType;

namespace systeminfo {

void BuildStorageUnitInfo(const StorageInfo& info, StorageUnitInfo* unit) {
  unit->id = StorageMonitor::GetInstance()->GetTransientIdForDeviceId(
      info.device_id());
  unit->name = base::UTF16ToUTF8(info.GetDisplayName(false));
  // TODO(hmin): Might need to take MTP device into consideration.
  unit->type = StorageInfo::IsRemovableDevice(info.device_id())
                   ? StorageUnitType::kRemovable
                   : StorageUnitType::kFixed;
  unit->capacity = static_cast<double>(info.total_size_in_bytes());
}

}  // namespace systeminfo

SystemInfoProvider::SystemInfoProvider()
    : is_waiting_for_completion_(false),
      task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(),
           /* default priority, */
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {}

SystemInfoProvider::~SystemInfoProvider() {
}

void SystemInfoProvider::PrepareQueryOnUIThread() {
}

void SystemInfoProvider::InitializeProvider(
    base::OnceClosure do_query_info_callback) {
  std::move(do_query_info_callback).Run();
}

void SystemInfoProvider::StartQueryInfo(QueryInfoCompletionCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!callback.is_null());

  callbacks_.push(std::move(callback));

  if (is_waiting_for_completion_) {
    return;
  }

  is_waiting_for_completion_ = true;

  InitializeProvider(base::BindOnce(
      &SystemInfoProvider::StartQueryInfoPostInitialization, this));
}

void SystemInfoProvider::OnQueryCompleted(bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  while (!callbacks_.empty()) {
    QueryInfoCompletionCallback callback = std::move(callbacks_.front());
    std::move(callback).Run(success);
    callbacks_.pop();
  }

  is_waiting_for_completion_ = false;
}

void SystemInfoProvider::StartQueryInfoPostInitialization() {
  PrepareQueryOnUIThread();
  // Post the custom query info task to blocking pool for information querying
  // and reply with OnQueryCompleted.
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&SystemInfoProvider::QueryInfo, this),
      base::BindOnce(&SystemInfoProvider::OnQueryCompleted, this));
}

}  // namespace extensions
