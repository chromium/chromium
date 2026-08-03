// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/rust/system/scoped_handle_interop.h"

namespace mojo::rust {

ScopedHandleWrapper::ScopedHandleWrapper(mojo::ScopedHandle handle)
    : handle_(std::move(handle)) {}

ScopedHandleWrapper::~ScopedHandleWrapper() = default;

uintptr_t ScopedHandleWrapper::Release(
    std::unique_ptr<ScopedHandleWrapper> wrapper) {
  if (!wrapper) {
    return 0;
  }
  return wrapper->handle_.release().value();
}

std::unique_ptr<ScopedHandleWrapper> ScopedHandleWrapper::Create(
    uintptr_t handle) {
  return std::make_unique<ScopedHandleWrapper>(
      mojo::ScopedHandle(mojo::Handle(handle)));
}

ScopedMessagePipeHandleWrapper::ScopedMessagePipeHandleWrapper(
    mojo::ScopedMessagePipeHandle handle)
    : handle_(std::move(handle)) {}

ScopedMessagePipeHandleWrapper::~ScopedMessagePipeHandleWrapper() = default;

uintptr_t ScopedMessagePipeHandleWrapper::Release(
    std::unique_ptr<ScopedMessagePipeHandleWrapper> wrapper) {
  if (!wrapper) {
    return 0;
  }
  return wrapper->handle_.release().value();
}

std::unique_ptr<ScopedMessagePipeHandleWrapper>
ScopedMessagePipeHandleWrapper::Create(uintptr_t handle) {
  return std::make_unique<ScopedMessagePipeHandleWrapper>(
      mojo::ScopedMessagePipeHandle(mojo::MessagePipeHandle(handle)));
}

ScopedMessageHandleWrapper::ScopedMessageHandleWrapper(
    mojo::ScopedMessageHandle handle)
    : handle_(std::move(handle)) {}

ScopedMessageHandleWrapper::~ScopedMessageHandleWrapper() = default;

uintptr_t ScopedMessageHandleWrapper::Release(
    std::unique_ptr<ScopedMessageHandleWrapper> wrapper) {
  if (!wrapper) {
    return 0;
  }
  return wrapper->handle_.release().value();
}

std::unique_ptr<ScopedMessageHandleWrapper> ScopedMessageHandleWrapper::Create(
    uintptr_t handle) {
  return std::make_unique<ScopedMessageHandleWrapper>(
      mojo::ScopedMessageHandle(mojo::MessageHandle(handle)));
}

}  // namespace mojo::rust
