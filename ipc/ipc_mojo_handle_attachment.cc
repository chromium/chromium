// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_mojo_handle_attachment.h"

#include <utility>

#include "build/build_config.h"

namespace IPC {
namespace internal {

MojoHandleAttachment::MojoHandleAttachment(mojo::ScopedHandle handle)
    : handle_(std::move(handle)) {}

MojoHandleAttachment::~MojoHandleAttachment() = default;

MessageAttachment::Type MojoHandleAttachment::GetType() const {
  return Type::MOJO_HANDLE;
}

mojo::ScopedHandle MojoHandleAttachment::TakeHandle() {
  return std::move(handle_);
}

}  // namespace internal
}  // namespace IPC
