// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_FILE_SYSTEM_ACCESS_FILE_SYSTEM_SYNC_ACCESS_HANDLE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_FILE_SYSTEM_ACCESS_FILE_SYSTEM_SYNC_ACCESS_HANDLE_H_

#include "third_party/blink/public/mojom/file_system_access/file_system_access_file_handle.mojom-blink.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class FileSystemSyncAccessHandle final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit FileSystemSyncAccessHandle(base::File backing_file);

  FileSystemSyncAccessHandle(const FileSystemSyncAccessHandle&) = delete;
  FileSystemSyncAccessHandle& operator=(const FileSystemSyncAccessHandle&) =
      delete;

  // GarbageCollected
  void Trace(Visitor* visitor) const override;

 private:
  // The file on disk backing the parent FileSystemFileHandle.
  base::File backing_file_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_FILE_SYSTEM_ACCESS_FILE_SYSTEM_SYNC_ACCESS_HANDLE_H_
