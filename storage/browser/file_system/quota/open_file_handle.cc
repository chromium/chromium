// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/quota/open_file_handle.h"

#include <stdint.h>

#include "storage/browser/file_system/quota/open_file_handle_context.h"
#include "storage/browser/file_system/quota/quota_reservation.h"

namespace storage {

OpenFileHandle::~OpenFileHandle() {
  DCHECK(sequence_checker_.CalledOnValidSequence());
}

void OpenFileHandle::UpdateMaxWrittenOffset(int64_t offset) {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  int64_t growth = context_->UpdateMaxWrittenOffset(offset);
  if (growth > 0)
    reservation_->ConsumeReservation(growth);
}

void OpenFileHandle::AddAppendModeWriteAmount(int64_t amount) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  if (amount <= 0)
    return;

  context_->AddAppendModeWriteAmount(amount);
  reservation_->ConsumeReservation(amount);
}

int64_t OpenFileHandle::GetEstimatedFileSize() const {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  return context_->GetEstimatedFileSize();
}

int64_t OpenFileHandle::GetMaxWrittenOffset() const {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  return context_->GetMaxWrittenOffset();
}

const base::FilePath& OpenFileHandle::platform_path() const {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  return context_->platform_path();
}

OpenFileHandle::OpenFileHandle(QuotaReservation* reservation,
                               OpenFileHandleContext* context)
    : reservation_(reservation), context_(context) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
}

}  // namespace storage
