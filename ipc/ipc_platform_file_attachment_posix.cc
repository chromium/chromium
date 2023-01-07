// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_platform_file_attachment_posix.h"

#include <tuple>
#include <utility>

namespace IPC {
namespace internal {

PlatformFileAttachment::PlatformFileAttachment(base::PlatformFile file)
    : file_(file) {
}

PlatformFileAttachment::PlatformFileAttachment(base::ScopedFD file)
    : file_(file.get()), owning_(std::move(file)) {}

PlatformFileAttachment::~PlatformFileAttachment() = default;

MessageAttachment::Type PlatformFileAttachment::GetType() const {
  return Type::PLATFORM_FILE;
}

base::PlatformFile PlatformFileAttachment::TakePlatformFile() {
  std::ignore = owning_.release();
  return file_;
}

base::PlatformFile GetPlatformFile(
    scoped_refptr<MessageAttachment> attachment) {
  DCHECK_EQ(attachment->GetType(), MessageAttachment::Type::PLATFORM_FILE);
  return static_cast<PlatformFileAttachment*>(attachment.get())->file();
}

}  // namespace internal
}  // namespace IPC
