// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/copy_or_move_hook_delegate.h"

#include "base/files/file.h"
#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "storage/browser/file_system/file_system_url.h"

namespace storage {

CopyOrMoveHookDelegate::CopyOrMoveHookDelegate(bool is_composite)
    : is_composite_(is_composite) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

CopyOrMoveHookDelegate::~CopyOrMoveHookDelegate() = default;

void CopyOrMoveHookDelegate::OnBeginProcessFile(
    const FileSystemURL& source_url,
    const FileSystemURL& destination_url,
    StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run(base::File::FILE_OK);
}

void CopyOrMoveHookDelegate::OnBeginProcessDirectory(
    const FileSystemURL& source_url,
    const FileSystemURL& destination_url,
    StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run(base::File::FILE_OK);
}

void CopyOrMoveHookDelegate::OnProgress(const FileSystemURL& source_url,
                                        const FileSystemURL& destination_url,
                                        int64_t size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void CopyOrMoveHookDelegate::OnError(const FileSystemURL& source_url,
                                     const FileSystemURL& destination_url,
                                     base::File::Error error,
                                     ErrorCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run(ErrorAction::kDefault);
}

void CopyOrMoveHookDelegate::OnEndCopy(const FileSystemURL& source_url,
                                       const FileSystemURL& destination_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void CopyOrMoveHookDelegate::OnEndMove(const FileSystemURL& source_url,
                                       const FileSystemURL& destination_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void CopyOrMoveHookDelegate::OnEndRemoveSource(
    const FileSystemURL& source_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

}  // namespace storage
