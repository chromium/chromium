// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/quota/quota_reservation.h"

#include <stdint.h>

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "storage/browser/file_system/quota/open_file_handle.h"
#include "storage/browser/file_system/quota/quota_reservation_buffer.h"

namespace storage {

void QuotaReservation::RefreshReservation(int64_t size,
                                          StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!running_refresh_request_);
  DCHECK(!client_crashed_);
  if (!reservation_manager())
    return;

  running_refresh_request_ = true;

  reservation_manager()->ReserveQuota(
      origin(), type(), size - remaining_quota_,
      base::BindOnce(&QuotaReservation::AdaptDidUpdateReservedQuota,
                     weak_ptr_factory_.GetWeakPtr(), remaining_quota_,
                     std::move(callback)));

  if (running_refresh_request_)
    remaining_quota_ = 0;
}

std::unique_ptr<OpenFileHandle> QuotaReservation::GetOpenFileHandle(
    const base::FilePath& platform_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!client_crashed_);
  return reservation_buffer_->GetOpenFileHandle(this, platform_path);
}

void QuotaReservation::OnClientCrash() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  client_crashed_ = true;

  if (remaining_quota_) {
    reservation_buffer_->PutReservationToBuffer(remaining_quota_);
    remaining_quota_ = 0;
  }
}

void QuotaReservation::ConsumeReservation(int64_t size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_LT(0, size);
  DCHECK_LE(size, remaining_quota_);
  if (client_crashed_)
    return;

  remaining_quota_ -= size;
  reservation_buffer_->PutReservationToBuffer(size);
}

QuotaReservationManager* QuotaReservation::reservation_manager() {
  return reservation_buffer_->reservation_manager();
}

const url::Origin& QuotaReservation::origin() const {
  return reservation_buffer_->origin();
}

FileSystemType QuotaReservation::type() const {
  return reservation_buffer_->type();
}

QuotaReservation::QuotaReservation(QuotaReservationBuffer* reservation_buffer)
    : client_crashed_(false),
      running_refresh_request_(false),
      remaining_quota_(0),
      reservation_buffer_(reservation_buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

QuotaReservation::~QuotaReservation() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (remaining_quota_ && reservation_manager()) {
    reservation_manager()->ReleaseReservedQuota(origin(), type(),
                                                remaining_quota_);
  }
}

// static
bool QuotaReservation::AdaptDidUpdateReservedQuota(
    const base::WeakPtr<QuotaReservation>& reservation,
    int64_t previous_size,
    StatusCallback callback,
    base::File::Error error,
    int64_t delta) {
  if (!reservation)
    return false;

  return reservation->DidUpdateReservedQuota(previous_size, std::move(callback),
                                             error, delta);
}

bool QuotaReservation::DidUpdateReservedQuota(int64_t previous_size,
                                              StatusCallback callback,
                                              base::File::Error error,
                                              int64_t delta) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(running_refresh_request_);
  running_refresh_request_ = false;

  if (client_crashed_) {
    std::move(callback).Run(base::File::FILE_ERROR_ABORT);
    return false;
  }

  if (error == base::File::FILE_OK)
    remaining_quota_ = previous_size + delta;
  std::move(callback).Run(error);
  return true;
}

}  // namespace storage
