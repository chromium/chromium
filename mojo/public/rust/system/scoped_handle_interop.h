// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_RUST_SYSTEM_SCOPED_HANDLE_INTEROP_H_
#define MOJO_PUBLIC_RUST_SYSTEM_SCOPED_HANDLE_INTEROP_H_

#include "mojo/public/cpp/system/handle.h"
#include "mojo/public/cpp/system/message_pipe.h"

// This file defines wrapper types that expose the various Scoped*Handle types
// to Rust, so they can be safely converted into Rust's handle types.
//
// Unfortunately, cxx doesn't handle templates, so we need one wrapper type for
// each handle type we expose.
//
// Each wrapper type provides a static member function `Release` which takes
// a unique pointer. Passing a unique pointer makes the ownership transfer
// obvious via the type system. Similarly, they provide a way to create a
// unique pointer given a raw handle.
//
// The ScopedHandle type guarantees that its contents are either a live, valid
// handle, or the invalid handle value 0 (if e.g. it has been moved from).

namespace mojo::rust {

class ScopedHandleWrapper {
 public:
  explicit ScopedHandleWrapper(mojo::ScopedHandle handle);
  ~ScopedHandleWrapper();

  ScopedHandleWrapper(const ScopedHandleWrapper&) = delete;
  ScopedHandleWrapper& operator=(const ScopedHandleWrapper&) = delete;

  static uintptr_t Release(std::unique_ptr<ScopedHandleWrapper> wrapper);

  static std::unique_ptr<ScopedHandleWrapper> Create(uintptr_t handle);

  mojo::ScopedHandle take_handle() { return std::move(handle_); }

 private:
  mojo::ScopedHandle handle_;
};

class ScopedMessagePipeHandleWrapper {
 public:
  explicit ScopedMessagePipeHandleWrapper(mojo::ScopedMessagePipeHandle handle);
  ~ScopedMessagePipeHandleWrapper();

  ScopedMessagePipeHandleWrapper(const ScopedMessagePipeHandleWrapper&) =
      delete;
  ScopedMessagePipeHandleWrapper& operator=(
      const ScopedMessagePipeHandleWrapper&) = delete;

  static uintptr_t Release(
      std::unique_ptr<ScopedMessagePipeHandleWrapper> wrapper);

  static std::unique_ptr<ScopedMessagePipeHandleWrapper> Create(
      uintptr_t handle);

  mojo::ScopedMessagePipeHandle take_handle() { return std::move(handle_); }

 private:
  mojo::ScopedMessagePipeHandle handle_;
};

}  // namespace mojo::rust

#endif  // MOJO_PUBLIC_RUST_SYSTEM_SCOPED_HANDLE_INTEROP_H_
