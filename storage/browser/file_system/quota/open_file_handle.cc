// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/quota/open_file_handle.h"

#include <stdint.h>

#include "storage/browser/file_system/quota/open_file_handle_context.h"
#include "storage/browser/file_system/quota/quota_reservation.h"

namespace storage {

OpenFileHandle::~OpenFileHandle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void OpenFileHandle::UpdateMaxWrittenOffset(int64_t offset) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  int64_t growth = context_->UpdateMaxWrittenOffset(offset);
  if (growth > 0)
    reservation_->ConsumeReservation(growth);
}

void OpenFileHandle::AddAppendModeWriteAmount(int64_t amount) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (amount <= 0)
    return;

  context_->AddAppendModeWriteAmount(amount);
  reservation_->ConsumeReservation(amount);
}

int64_t OpenFileHandle::GetEstimatedFileSize() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return context_->GetEstimatedFileSize();
}

int64_t OpenFileHandle::GetMaxWrittenOffset() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return context_->GetMaxWrittenOffset();
}

const base::FilePath& OpenFileHandle::platform_path() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return context_->platform_path();
}

OpenFileHandle::OpenFileHandle(QuotaReservation* reservation,
                               OpenFileHandleContext* context)
    : reservation_(reservation), context_(context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

}  // namespace storage
