// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/quota/open_file_handle_context.h"

#include <stdint.h>

#include "base/files/file_util.h"
#include "storage/browser/file_system/quota/quota_reservation_buffer.h"

namespace storage {

OpenFileHandleContext::OpenFileHandleContext(
    const base::FilePath& platform_path,
    QuotaReservationBuffer* reservation_buffer)
    : initial_file_size_(0),
      maximum_written_offset_(0),
      append_mode_write_amount_(0),
      platform_path_(platform_path),
      reservation_buffer_(reservation_buffer) {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  base::GetFileSize(platform_path, &initial_file_size_);
  maximum_written_offset_ = initial_file_size_;
}

int64_t OpenFileHandleContext::UpdateMaxWrittenOffset(int64_t offset) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  if (offset <= maximum_written_offset_)
    return 0;

  int64_t growth = offset - maximum_written_offset_;
  maximum_written_offset_ = offset;
  return growth;
}

void OpenFileHandleContext::AddAppendModeWriteAmount(int64_t amount) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  append_mode_write_amount_ += amount;
}

int64_t OpenFileHandleContext::GetEstimatedFileSize() const {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  return maximum_written_offset_ + append_mode_write_amount_;
}

int64_t OpenFileHandleContext::GetMaxWrittenOffset() const {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  return maximum_written_offset_;
}

OpenFileHandleContext::~OpenFileHandleContext() {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  // TODO(tzik): Optimize this for single operation.

  int64_t file_size = 0;
  base::GetFileSize(platform_path_, &file_size);
  int64_t usage_delta = file_size - initial_file_size_;

  // |reserved_quota_consumption| may be greater than the recorded file growth
  // when a plugin crashed before reporting its consumption.
  // In this case, the reserved quota for the plugin should be handled as
  // consumed quota.
  int64_t reserved_quota_consumption =
      std::max(GetEstimatedFileSize(), file_size) - initial_file_size_;

  reservation_buffer_->CommitFileGrowth(reserved_quota_consumption,
                                        usage_delta);
  reservation_buffer_->DetachOpenFileHandleContext(this);
}

}  // namespace storage
