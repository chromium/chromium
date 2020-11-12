// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/platform_handle_in_transit.h"

#include <utility>

#include "base/logging.h"
#include "base/process/process_handle.h"
#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>

#include "base/win/scoped_handle.h"
#endif

namespace mojo {
namespace core {

namespace {

#if defined(OS_WIN)
HANDLE TransferHandle(HANDLE handle,
                      base::ProcessHandle from_process,
                      base::ProcessHandle to_process) {
  BOOL result =
      ::DuplicateHandle(from_process, handle, to_process, &handle, 0, FALSE,
                        DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE);
  if (result) {
    return handle;
  } else {
    DPLOG(ERROR) << "DuplicateHandle failed";
    return INVALID_HANDLE_VALUE;
  }
}

void CloseHandleInProcess(HANDLE handle, const ScopedProcessHandle& process) {
  DCHECK_NE(handle, INVALID_HANDLE_VALUE);
  DCHECK(process.is_valid());

  // The handle lives in |process|, so we close it there using a special
  // incantation of |DuplicateHandle()|.
  //
  // See https://msdn.microsoft.com/en-us/library/windows/desktop/ms724251 for
  // this usage of |DuplicateHandle()|, particularly where it says "to close a
  // handle from the source process...". Note that although the documentation
  // says that the target *handle* address must be NULL, it seems that the
  // target process handle being NULL is what really matters here.
  BOOL result = ::DuplicateHandle(process.get(), handle, NULL, &handle, 0,
                                  FALSE, DUPLICATE_CLOSE_SOURCE);
  if (!result) {
    DPLOG(ERROR) << "DuplicateHandle failed";
  }
}
#endif

}  // namespace

PlatformHandleInTransit::PlatformHandleInTransit() = default;

PlatformHandleInTransit::PlatformHandleInTransit(PlatformHandle handle)
    : handle_(std::move(handle)) {}

PlatformHandleInTransit::PlatformHandleInTransit(
    PlatformHandleInTransit&& other) {
  *this = std::move(other);
}

PlatformHandleInTransit::~PlatformHandleInTransit() {
#if defined(OS_WIN)
  if (!owning_process_.is_valid()) {
    DCHECK_EQ(remote_handle_, INVALID_HANDLE_VALUE);
    return;
  }

  CloseHandleInProcess(remote_handle_, owning_process_);
#endif
}

PlatformHandleInTransit& PlatformHandleInTransit::operator=(
    PlatformHandleInTransit&& other) {
#if defined(OS_WIN)
  if (owning_process_.is_valid()) {
    DCHECK_NE(remote_handle_, INVALID_HANDLE_VALUE);
    CloseHandleInProcess(remote_handle_, owning_process_);
  }

  remote_handle_ = INVALID_HANDLE_VALUE;
  std::swap(remote_handle_, other.remote_handle_);
#endif
  handle_ = std::move(other.handle_);
  owning_process_ = std::move(other.owning_process_);
  return *this;
}

PlatformHandle PlatformHandleInTransit::TakeHandle() {
  DCHECK(!owning_process_.is_valid());
  return std::move(handle_);
}

void PlatformHandleInTransit::CompleteTransit() {
#if defined(OS_WIN)
  remote_handle_ = INVALID_HANDLE_VALUE;
#endif
  handle_.release();
  owning_process_ = ScopedProcessHandle();
}

bool PlatformHandleInTransit::TransferToProcess(
    ScopedProcessHandle target_process) {
  DCHECK(target_process.is_valid());
  DCHECK(!owning_process_.is_valid());
  DCHECK(handle_.is_valid());
#if defined(OS_WIN)
  remote_handle_ =
      TransferHandle(handle_.ReleaseHandle(), base::GetCurrentProcessHandle(),
                     target_process.get());
  if (remote_handle_ == INVALID_HANDLE_VALUE)
    return false;
#endif
  owning_process_ = std::move(target_process);
  return true;
}

#if defined(OS_WIN)
// static
bool PlatformHandleInTransit::IsPseudoHandle(HANDLE handle) {
  // Note that there appears to be no official documentation covering the
  // existence of specific pseudo handle values. In practice it's clear that
  // e.g. -1 is the current process, -2 is the current thread, etc. The largest
  // negative value known to be an issue with DuplicateHandle in the fuzzer is
  // -12.
  //
  // Note that there is virtually no risk of a real handle value falling within
  // this range and being misclassified as a pseudo handle.
  constexpr int kMinimumKnownPseudoHandleValue = -12;
  const auto value = static_cast<int32_t>(reinterpret_cast<uintptr_t>(handle));
  return value < 0 && value >= kMinimumKnownPseudoHandleValue;
}

// static
PlatformHandle PlatformHandleInTransit::TakeIncomingRemoteHandle(
    HANDLE handle,
    base::ProcessHandle owning_process) {
  return PlatformHandle(base::win::ScopedHandle(
      TransferHandle(handle, owning_process, base::GetCurrentProcessHandle())));
}
#endif

}  // namespace core
}  // namespace mojo
