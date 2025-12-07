// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mojo/mojo_file_system_access.h"

#include "third_party/blink/public/mojom/file_system_access/file_system_access_file_handle.mojom-blink.h"
#include "third_party/blink/renderer/core/mojo/mojo_handle.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_file_handle.h"

namespace blink {

// static
MojoFileSystemAccess& MojoFileSystemAccess::From(Mojo& mojo) {
  MojoFileSystemAccess* supplement = mojo.GetMojoFileSystemAccess();
  if (!supplement) {
    supplement = MakeGarbageCollected<MojoFileSystemAccess>();
    mojo.SetMojoFileSystemAccess(supplement);
  }
  return *supplement;
}

void MojoFileSystemAccess::Trace(Visitor* visitor) const {}

// static
MojoHandle* MojoFileSystemAccess::getFileSystemAccessTransferToken(
    FileSystemFileHandle* fs_handle) {
  return MakeGarbageCollected<MojoHandle>(
      mojo::ScopedHandleBase<mojo::Handle>::From(
          fs_handle->Transfer().PassPipe()));
}

}  // namespace blink
