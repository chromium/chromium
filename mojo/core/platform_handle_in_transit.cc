// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/platform_handle_in_transit.h"

#include <utility>

#include "base/debug/alias.h"
#include "base/logging.h"
#include "base/process/process_handle.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include <ntstatus.h>

#include "base/win/nt_status.h"
#include "base/win/scoped_handle.h"
#include "mojo/public/cpp/platform/platform_handle_security_util_win.h"
#endif

namespace mojo {
namespace core {

namespace {

#if BUILDFLAG(IS_WIN)
HANDLE TransferHandle(HANDLE handle,
                      base::ProcessHandle from_process,
                      base::ProcessHandle to_process,
                      PlatformHandleInTransit::TransferTargetTrustLevel trust) {
  if (trust == PlatformHandleInTransit::kUntrustedTarget) {
    DcheckIfFileHandleIsUnsafe(handle);
  }

  // Duplicating INVALID_HANDLE_VALUE passes a process handle. If you intend to
  // do this, you must open a valid process handle, not pass the result of
  // GetCurrentProcess(). e.g. https://crbug.com/243339.
  CHECK(handle != INVALID_HANDLE_VALUE);

  HANDLE out_handle;
  BOOL result =
      ::DuplicateHandle(from_process, handle, to_process, &out_handle, 0, FALSE,
                        DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE);
  if (result) {
    return out_handle;
  }

  const DWORD error = ::GetLastError();

  // ERROR_ACCESS_DENIED may indicate that the remote process (which could be
  // either the source or destination process here) is already terminated or has
  // begun termination and therefore no longer has a handle table. We don't want
  // these cases to crash because we know they happen in practice and are
  // largely unavoidable.
  if (error == ERROR_ACCESS_DENIED &&
      base::win::GetLastNtStatus() == STATUS_PROCESS_IS_TERMINATING) {
    DVLOG(1) << "DuplicateHandle from " << from_process << " to " << to_process
             << " for handle " << handle
             << " failed due to process termination";
    return INVALID_HANDLE_VALUE;
  }

  base::debug::Alias(&handle);
  base::debug::Alias(&from_process);
  base::debug::Alias(&to_process);
  base::debug::Alias(&error);
  PLOG(FATAL) << "DuplicateHandle failed from " << from_process << " to "
              << to_process << " for handle " << handle;
  return INVALID_HANDLE_VALUE;
}

void CloseHandleInProcess(HANDLE handle, const base::Process& process) {
  DCHECK_NE(handle, INVALID_HANDLE_VALUE);
  DCHECK(process.IsValid());

  // The handle lives in |process|, so we close it there using a special
  // incantation of |DuplicateHandle()|.
  //
  // See https://msdn.microsoft.com/en-us/library/windows/desktop/ms724251 for
  // this usage of |DuplicateHandle()|, particularly where it says "to close a
  // handle from the source process...". Note that although the documentation
  // says that the target *handle* address must be NULL, it seems that the
  // target process handle being NULL is what really matters here.
  BOOL result = ::DuplicateHandle(process.Handle(), handle, nullptr, &handle, 0,
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
#if BUILDFLAG(IS_WIN)
  if (!owning_process_.IsValid()) {
    DCHECK_EQ(remote_handle_, INVALID_HANDLE_VALUE);
    return;
  }

  CloseHandleInProcess(remote_handle_, owning_process_);
#endif
}

PlatformHandleInTransit& PlatformHandleInTransit::operator=(
    PlatformHandleInTransit&& other) {
#if BUILDFLAG(IS_WIN)
  if (owning_process_.IsValid()) {
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
  DCHECK(!owning_process_.IsValid());
  return std::move(handle_);
}

void PlatformHandleInTransit::CompleteTransit() {
#if BUILDFLAG(IS_WIN)
  remote_handle_ = INVALID_HANDLE_VALUE;
#endif
  handle_.release();
  owning_process_ = base::Process();
}

bool PlatformHandleInTransit::TransferToProcess(
    base::Process target_process,
    TransferTargetTrustLevel trust) {
  DCHECK(target_process.IsValid());
  DCHECK(!owning_process_.IsValid());
  DCHECK(handle_.is_valid());
#if BUILDFLAG(IS_WIN)
  remote_handle_ =
      TransferHandle(handle_.ReleaseHandle(), base::GetCurrentProcessHandle(),
                     target_process.Handle(), trust);
  if (remote_handle_ == INVALID_HANDLE_VALUE)
    return false;
#endif
  owning_process_ = std::move(target_process);
  return true;
}

#if BUILDFLAG(IS_WIN)
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
      TransferHandle(handle, owning_process, base::GetCurrentProcessHandle(),
                     kTrustedTarget)));
}
#endif

}  // namespace core
}  // namespace mojo
