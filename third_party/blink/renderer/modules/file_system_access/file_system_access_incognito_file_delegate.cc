// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/file_system_access/file_system_access_incognito_file_delegate.h"

#include "base/files/file.h"
#include "base/notreached.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_access_file_delegate.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

FileSystemAccessFileDelegate* FileSystemAccessFileDelegate::CreateForIncognito(
    ExecutionContext* context,
    mojo::PendingRemote<mojom::blink::FileSystemAccessFileDelegateHost>
        incognito_file_remote) {
  return MakeGarbageCollected<FileSystemAccessIncognitoFileDelegate>(
      context, std::move(incognito_file_remote),
      base::PassKey<FileSystemAccessFileDelegate>());
}

FileSystemAccessIncognitoFileDelegate::FileSystemAccessIncognitoFileDelegate(
    ExecutionContext* context,
    mojo::PendingRemote<mojom::blink::FileSystemAccessFileDelegateHost>
        incognito_file_remote,
    base::PassKey<FileSystemAccessFileDelegate>)
    : mojo_ptr_(context) {
  mojo_ptr_.Bind(std::move(incognito_file_remote),
                 context->GetTaskRunner(TaskType::kMiscPlatformAPI));
  DCHECK(mojo_ptr_.is_bound());
}

void FileSystemAccessIncognitoFileDelegate::Trace(Visitor* visitor) const {
  visitor->Trace(mojo_ptr_);
  FileSystemAccessFileDelegate::Trace(visitor);
}

FileErrorOr<int> FileSystemAccessIncognitoFileDelegate::Read(
    int64_t offset,
    base::span<uint8_t> data) {
  // TODO(crbug.com/1225653): Implement this method.
  NOTIMPLEMENTED();
  return 0;
}

FileErrorOr<int> FileSystemAccessIncognitoFileDelegate::Write(
    int64_t offset,
    const base::span<uint8_t> data) {
  // TODO(crbug.com/1225653): Implement this method.
  NOTIMPLEMENTED();
  return 0;
}

FileErrorOr<int64_t> FileSystemAccessIncognitoFileDelegate::GetLength() {
  // TODO(crbug.com/1225653): Implement this method.
  NOTIMPLEMENTED();
  return 0;
}

bool FileSystemAccessIncognitoFileDelegate::SetLength(int64_t length) {
  // TODO(crbug.com/1225653): Implement this method.
  NOTIMPLEMENTED();
  return false;
}

bool FileSystemAccessIncognitoFileDelegate::Flush() {
  // TODO(crbug.com/1225653): Implement this method.
  NOTIMPLEMENTED();
  return false;
}

void FileSystemAccessIncognitoFileDelegate::Close() {
  // TODO(crbug.com/1225653): Implement this method.
  NOTIMPLEMENTED();
}

}  // namespace blink
