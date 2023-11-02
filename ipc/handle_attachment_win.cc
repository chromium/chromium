// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/handle_attachment_win.h"

#include <windows.h>

namespace IPC {
namespace internal {

HandleAttachmentWin::HandleAttachmentWin(const HANDLE& handle) {
  HANDLE duplicated_handle;
  BOOL result =
      ::DuplicateHandle(GetCurrentProcess(), handle, GetCurrentProcess(),
                        &duplicated_handle, 0, FALSE, DUPLICATE_SAME_ACCESS);
  if (result) {
    handle_.Set(duplicated_handle);
  }
}

HandleAttachmentWin::HandleAttachmentWin(const HANDLE& handle,
                                         FromWire from_wire)
    : handle_(handle) {}

HandleAttachmentWin::~HandleAttachmentWin() {}

MessageAttachment::Type HandleAttachmentWin::GetType() const {
  return Type::WIN_HANDLE;
}

}  // namespace internal
}  // namespace IPC
