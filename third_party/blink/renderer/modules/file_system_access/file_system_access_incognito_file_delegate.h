// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_INCOGNITO_FILE_DELEGATE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_INCOGNITO_FILE_DELEGATE_H_

#include "base/files/file.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_file_handle.mojom-blink.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_access_file_delegate.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {

// Incognito implementation of the FileSystemAccessFileDelegate. All file
// operations are routed to the browser to be written to in-memory files.
class FileSystemAccessIncognitoFileDelegate final
    : public FileSystemAccessFileDelegate {
 public:
  // Instances should only be constructed via
  // `FileSystemAccessFileDelegate::CreateForIncognito()`
  explicit FileSystemAccessIncognitoFileDelegate(
      ExecutionContext* context,
      mojo::PendingRemote<mojom::blink::FileSystemAccessFileDelegateHost>
          incognito_file_remote,
      base::PassKey<FileSystemAccessFileDelegate>);

  FileSystemAccessIncognitoFileDelegate(
      const FileSystemAccessIncognitoFileDelegate&) = delete;
  FileSystemAccessIncognitoFileDelegate& operator=(
      const FileSystemAccessIncognitoFileDelegate&) = delete;

  int Read(int64_t offset, base::span<uint8_t> data) override;
  int Write(int64_t offset, const base::span<uint8_t> data) override;

  int64_t GetLength() override;
  bool SetLength(int64_t length) override;

  bool Flush() override;
  void Close() override;

  bool IsValid() const override { return mojo_ptr_.is_bound(); }

  base::File::Error GetLastFileError() override;

  void Trace(Visitor*) const override;

 private:
  // Used to route file operations to the browser.
  HeapMojoRemote<mojom::blink::FileSystemAccessFileDelegateHost> mojo_ptr_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_INCOGNITO_FILE_DELEGATE_H_
