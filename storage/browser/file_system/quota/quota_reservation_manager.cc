// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/quota/quota_reservation_manager.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/callback.h"
#include "storage/browser/file_system/quota/quota_reservation.h"
#include "storage/browser/file_system/quota/quota_reservation_buffer.h"

namespace storage {

QuotaReservationManager::QuotaReservationManager(
    std::unique_ptr<QuotaBackend> backend)
    : backend_(std::move(backend)) {
  sequence_checker_.DetachFromSequence();
}

QuotaReservationManager::~QuotaReservationManager() {
  DCHECK(sequence_checker_.CalledOnValidSequence());
}

void QuotaReservationManager::ReserveQuota(const url::Origin& origin,
                                           FileSystemType type,
                                           int64_t size,
                                           ReserveQuotaCallback callback) {
  DCHECK(!origin.opaque());
  backend_->ReserveQuota(origin, type, size, std::move(callback));
}

void QuotaReservationManager::ReleaseReservedQuota(const url::Origin& origin,
                                                   FileSystemType type,
                                                   int64_t size) {
  DCHECK(!origin.opaque());
  backend_->ReleaseReservedQuota(origin, type, size);
}

void QuotaReservationManager::CommitQuotaUsage(const url::Origin& origin,
                                               FileSystemType type,
                                               int64_t delta) {
  DCHECK(!origin.opaque());
  backend_->CommitQuotaUsage(origin, type, delta);
}

void QuotaReservationManager::IncrementDirtyCount(const url::Origin& origin,
                                                  FileSystemType type) {
  DCHECK(!origin.opaque());
  backend_->IncrementDirtyCount(origin, type);
}

void QuotaReservationManager::DecrementDirtyCount(const url::Origin& origin,
                                                  FileSystemType type) {
  DCHECK(!origin.opaque());
  backend_->DecrementDirtyCount(origin, type);
}

scoped_refptr<QuotaReservationBuffer>
QuotaReservationManager::GetReservationBuffer(const url::Origin& origin,
                                              FileSystemType type) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  DCHECK(!origin.opaque());
  QuotaReservationBuffer** buffer =
      &reservation_buffers_[std::make_pair(origin, type)];
  if (!*buffer) {
    *buffer = new QuotaReservationBuffer(weak_ptr_factory_.GetWeakPtr(), origin,
                                         type);
  }
  return base::WrapRefCounted(*buffer);
}

void QuotaReservationManager::ReleaseReservationBuffer(
    QuotaReservationBuffer* reservation_buffer) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  std::pair<url::Origin, FileSystemType> key(reservation_buffer->origin(),
                                             reservation_buffer->type());
  DCHECK_EQ(reservation_buffers_[key], reservation_buffer);
  reservation_buffers_.erase(key);
}

scoped_refptr<QuotaReservation> QuotaReservationManager::CreateReservation(
    const url::Origin& origin,
    FileSystemType type) {
  DCHECK(!origin.opaque());
  return GetReservationBuffer(origin, type)->CreateReservation();
}

}  // namespace storage
