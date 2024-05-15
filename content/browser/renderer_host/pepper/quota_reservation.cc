// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/quota_reservation.h"

#include <memory>
#include <tuple>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/quota/open_file_handle.h"
#include "storage/browser/file_system/quota/quota_reservation.h"
#include "storage/common/file_system/file_system_util.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

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
          blink::StorageKey::CreateFirstParty(url::Origin::Create(origin_url)),
          file_system_type);
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
  if (file_system_context_) {
    base::File::Error error =
        file_system_context_->operation_runner()->SyncGetPlatformPath(
            url, &platform_file_path);
    if (error != base::File::FILE_OK) {
      NOTREACHED_IN_MIGRATION();
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
    std::ignore = file_handle.release();
    return max_written_offset;
  }
  NOTREACHED_IN_MIGRATION();
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
    NOTREACHED_IN_MIGRATION();
  }
}

void QuotaReservation::ReserveQuota(int64_t amount,
                                    const ppapi::FileGrowthMap& file_growths,
                                    ReserveQuotaCallback callback) {
  for (auto it = files_.begin(); it != files_.end(); ++it) {
    auto growth_it = file_growths.find(it->first);
    if (growth_it != file_growths.end()) {
      it->second->UpdateMaxWrittenOffset(growth_it->second.max_written_offset);
      it->second->AddAppendModeWriteAmount(
          growth_it->second.append_mode_write_amount);
    } else {
      NOTREACHED_IN_MIGRATION();
    }
  }

  quota_reservation_->RefreshReservation(
      amount, base::BindOnce(&QuotaReservation::GotReservedQuota, this,
                             std::move(callback)));
}

void QuotaReservation::OnClientCrash() { quota_reservation_->OnClientCrash(); }

void QuotaReservation::GotReservedQuota(ReserveQuotaCallback callback,
                                        base::File::Error error) {
  ppapi::FileSizeMap file_sizes;
  for (auto it = files_.begin(); it != files_.end(); ++it)
    file_sizes[it->first] = it->second->GetMaxWrittenOffset();

  if (file_system_context_) {
    GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       quota_reservation_->remaining_quota(), file_sizes));
  } else {
    // Unit testing code path.
    std::move(callback).Run(quota_reservation_->remaining_quota(), file_sizes);
  }
}

void QuotaReservation::DeleteOnCorrectThread() const {
  if (file_system_context_ && !file_system_context_->default_file_task_runner()
                                   ->RunsTasksInCurrentSequence()) {
    file_system_context_->default_file_task_runner()->DeleteSoon(FROM_HERE,
                                                                 this);
  } else {
    // We're on the right thread to delete, or unit test.
    delete this;
  }
}

}  // namespace content
