// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/quota/quota_reservation_buffer.h"

#include <stdint.h>

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "storage/browser/file_system/quota/open_file_handle.h"
#include "storage/browser/file_system/quota/open_file_handle_context.h"
#include "storage/browser/file_system/quota/quota_reservation.h"

namespace storage {

QuotaReservationBuffer::QuotaReservationBuffer(
    base::WeakPtr<QuotaReservationManager> reservation_manager,
    const url::Origin& origin,
    FileSystemType type)
    : reservation_manager_(reservation_manager),
      origin_(origin),
      type_(type),
      reserved_quota_(0) {
  DCHECK(!origin.opaque());
  DCHECK(sequence_checker_.CalledOnValidSequence());
  reservation_manager_->IncrementDirtyCount(origin, type);
}

scoped_refptr<QuotaReservation> QuotaReservationBuffer::CreateReservation() {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  return base::WrapRefCounted(new QuotaReservation(this));
}

std::unique_ptr<OpenFileHandle> QuotaReservationBuffer::GetOpenFileHandle(
    QuotaReservation* reservation,
    const base::FilePath& platform_path) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  OpenFileHandleContext** open_file = &open_files_[platform_path];
  if (!*open_file)
    *open_file = new OpenFileHandleContext(platform_path, this);
  return base::WrapUnique(new OpenFileHandle(reservation, *open_file));
}

void QuotaReservationBuffer::CommitFileGrowth(
    int64_t reserved_quota_consumption,
    int64_t usage_delta) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  if (!reservation_manager_)
    return;
  reservation_manager_->CommitQuotaUsage(origin_, type_, usage_delta);

  if (reserved_quota_consumption > 0) {
    if (reserved_quota_consumption > reserved_quota_) {
      LOG(ERROR) << "Detected over consumption of the storage quota beyond its"
                 << " reservation";
      reserved_quota_consumption = reserved_quota_;
    }

    reserved_quota_ -= reserved_quota_consumption;
    reservation_manager_->ReleaseReservedQuota(origin_, type_,
                                               reserved_quota_consumption);
  }
}

void QuotaReservationBuffer::DetachOpenFileHandleContext(
    OpenFileHandleContext* open_file) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  DCHECK_EQ(open_file, open_files_[open_file->platform_path()]);
  open_files_.erase(open_file->platform_path());
}

void QuotaReservationBuffer::PutReservationToBuffer(int64_t reservation) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  DCHECK_LE(0, reservation);
  reserved_quota_ += reservation;
}

QuotaReservationBuffer::~QuotaReservationBuffer() {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  if (!reservation_manager_)
    return;

  DCHECK_LE(0, reserved_quota_);
  if (reserved_quota_ && reservation_manager_) {
    reservation_manager_->ReserveQuota(
        origin_, type_, -reserved_quota_,
        base::BindOnce(&QuotaReservationBuffer::DecrementDirtyCount,
                       reservation_manager_, origin_, type_));
  }
  reservation_manager_->ReleaseReservationBuffer(this);
}

// static
bool QuotaReservationBuffer::DecrementDirtyCount(
    base::WeakPtr<QuotaReservationManager> reservation_manager,
    const url::Origin& origin,
    FileSystemType type,
    base::File::Error error,
    int64_t delta_unused) {
  DCHECK(!origin.opaque());
  if (error == base::File::FILE_OK && reservation_manager) {
    reservation_manager->DecrementDirtyCount(origin, type);
    return true;
  }
  return false;
}

}  // namespace storage
