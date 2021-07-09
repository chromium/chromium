// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/file_system_access/file_system_sync_access_handle.h"

namespace blink {

FileSystemSyncAccessHandle::FileSystemSyncAccessHandle(
    FileSystemAccessFileDelegate* file_delegate)
    : file_delegate_(file_delegate) {}

void FileSystemSyncAccessHandle::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(file_delegate_);
}

}  // namespace blink
