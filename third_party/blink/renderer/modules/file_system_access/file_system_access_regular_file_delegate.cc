// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/file_system_access/file_system_access_regular_file_delegate.h"

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

FileSystemAccessFileDelegate* FileSystemAccessFileDelegate::Create(
    base::File backing_file) {
  return MakeGarbageCollected<FileSystemAccessRegularFileDelegate>(
      std::move(backing_file), base::PassKey<FileSystemAccessFileDelegate>());
}

FileSystemAccessRegularFileDelegate::FileSystemAccessRegularFileDelegate(
    base::File backing_file,
    base::PassKey<FileSystemAccessFileDelegate>)
    : backing_file_(std::move(backing_file)) {}

}  // namespace blink
