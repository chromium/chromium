// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/quota_reservation.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/quota/open_file_handle.h"
#include "storage/browser/file_system/quota/quota_reservation.h"
#include "storage/common/file_system/file_system_util.h"

namespace content {

// static
scoped_refptr<QuotaReservation> QuotaReservation::Create(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const GURL& origin_url,
    storage::FileSystemType type) {
  return scoped_refptr<QuotaReservation>(
      new QuotaReservation(file_system_context, origin_url, type));
}

QuotaReservation::QuotaReservation(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const GURL& origin_url,
    storage::FileSystemType file_system_type)
    : file_system_context_(file_system_context) {
  quota_reservation_ =
      file_system_context->CreateQuotaReservationOnFileTaskRunner(
          origin_url, file_system_type);
}

// For unit testing only.
QuotaReservation::QuotaReservation(
    scoped_refptr<storage::QuotaReservation> quota_reservation,
    const GURL& /* origin_url */,
    storage::FileSystemType /* file_system_type */)
    : quota_reservation_(quota_reservation) {
}

QuotaReservation::~QuotaReservation() {
  // We should have no open files at this point.
  DCHECK(files_.size() == 0);
  for (auto it = files_.begin(); it != files_.end(); ++it)
    delete it->second;
}

int64_t QuotaReservation::OpenFile(int32_t id,
                                   const storage::FileSystemURL& url) {
  base::FilePath platform_file_path;
  if (file_system_context_.get()) {
    base::File::Error error =
        file_system_context_->operation_runner()->SyncGetPlatformPath(
            url, &platform_file_path);
    if (error != base::File::FILE_OK) {
      NOTREACHED();
      return 0;
    }
  } else {
    // For test.
    platform_file_path = url.path();
  }

  std::unique_ptr<storage::OpenFileHandle> file_handle =
      quota_reservation_->GetOpenFileHandle(platform_file_path);
  std::pair<FileMap::iterator, bool> insert_result =
      files_.insert(std::make_pair(id, file_handle.get()));
  if (insert_result.second) {
    int64_t max_written_offset = file_handle->GetMaxWrittenOffset();
    ignore_result(file_handle.release());
    return max_written_offset;
  }
  NOTREACHED();
  return 0;
}

void QuotaReservation::CloseFile(int32_t id,
                                 const ppapi::FileGrowth& file_growth) {
  auto it = files_.find(id);
  if (it != files_.end()) {
    it->second->UpdateMaxWrittenOffset(file_growth.max_written_offset);
    it->second->AddAppendModeWriteAmount(file_growth.append_mode_write_amount);
    delete it->second;
    files_.erase(it);
  } else {
    NOTREACHED();
  }
}

void QuotaReservation::ReserveQuota(int64_t amount,
                                    const ppapi::FileGrowthMap& file_growths,
                                    const ReserveQuotaCallback& callback) {
  for (auto it = files_.begin(); it != files_.end(); ++it) {
    auto growth_it = file_growths.find(it->first);
    if (growth_it != file_growths.end()) {
      it->second->UpdateMaxWrittenOffset(growth_it->second.max_written_offset);
      it->second->AddAppendModeWriteAmount(
          growth_it->second.append_mode_write_amount);
    } else {
      NOTREACHED();
    }
  }

  quota_reservation_->RefreshReservation(
      amount, base::Bind(&QuotaReservation::GotReservedQuota, this, callback));
}

void QuotaReservation::OnClientCrash() { quota_reservation_->OnClientCrash(); }

void QuotaReservation::GotReservedQuota(const ReserveQuotaCallback& callback,
                                        base::File::Error error) {
  ppapi::FileSizeMap file_sizes;
  for (auto it = files_.begin(); it != files_.end(); ++it)
    file_sizes[it->first] = it->second->GetMaxWrittenOffset();

  if (file_system_context_.get()) {
    base::PostTask(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(callback, quota_reservation_->remaining_quota(),
                       file_sizes));
  } else {
    // Unit testing code path.
    callback.Run(quota_reservation_->remaining_quota(), file_sizes);
  }
}

void QuotaReservation::DeleteOnCorrectThread() const {
  if (file_system_context_.get() &&
      !file_system_context_->default_file_task_runner()
           ->RunsTasksInCurrentSequence()) {
    file_system_context_->default_file_task_runner()->DeleteSoon(FROM_HERE,
                                                                 this);
  } else {
    // We're on the right thread to delete, or unit test.
    delete this;
  }
}

}  // namespace content
