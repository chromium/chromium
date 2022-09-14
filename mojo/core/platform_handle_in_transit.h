// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_PLATFORM_HANDLE_IN_TRANSIT_H_
#define MOJO_CORE_PLATFORM_HANDLE_IN_TRANSIT_H_

#include "base/process/process.h"
#include "build/build_config.h"
#include "mojo/public/cpp/platform/platform_handle.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

namespace mojo {
namespace core {

// Owns a PlatformHandle which may actually belong to another process. On
// Windows and (sometimes) Mac, handles in a message object may take on values
// which only have meaning in the context of a remote process.
//
// This class provides a safe way of scoping the lifetime of such handles so
// that they don't leak when transmission can't be completed.
class PlatformHandleInTransit {
 public:
  PlatformHandleInTransit();
  explicit PlatformHandleInTransit(PlatformHandle handle);
  PlatformHandleInTransit(PlatformHandleInTransit&&);

  PlatformHandleInTransit(const PlatformHandleInTransit&) = delete;
  PlatformHandleInTransit& operator=(const PlatformHandleInTransit&) = delete;

  ~PlatformHandleInTransit();

  PlatformHandleInTransit& operator=(PlatformHandleInTransit&&);

  // Accessor for the owned handle. Must be owned by the calling process.
  const PlatformHandle& handle() const {
    DCHECK(!owning_process_.IsValid());
    return handle_;
  }

  // Returns the process which owns this handle. If this is invalid, the handle
  // is owned by the current process.
  const base::Process& owning_process() const { return owning_process_; }

  // Takes ownership of the held handle as-is. The handle must belong to the
  // current process.
  PlatformHandle TakeHandle();

  // Discards the handle owned by this object. The implication is that its
  // value has been successfully communicated to the owning process and the
  // calling process is no longer responsible for managing the handle's
  // lifetime.
  void CompleteTransit();

  // Designates the relative trust level of the destination process compared to
  // the source process, in the context of a handle transfer operation. This
  // may be expanded to more granular degrees of trust in the future.
  enum TransferTargetTrustLevel {
    // No special constraints on what can be transferred or how.
    kTrustedTarget,

    // On some platforms, transfers with this destination type may be restricted
    // to block certain types of handles.
    kUntrustedTarget,
  };

  // Transfers ownership of this (local) handle to |target_process|.
  bool TransferToProcess(base::Process target_process,
                         TransferTargetTrustLevel trust = kTrustedTarget);

#if BUILDFLAG(IS_WIN)
  HANDLE remote_handle() const { return remote_handle_; }

  // Indicates whether |handle| is a known pseudo handle value. In a fuzzing
  // environment we merely simulate IPC, so we end up accepting "remote" handle
  // values from our own process. This means that unlike in production
  // scenarios, we may end up successfully calling DuplicateHandle on a fuzzed
  // pseudo handle value (in production if a remote process sent us a pseudo
  // handle value, DuplicateHandle would always fail).
  //
  // For some reason, a small number of special pseudo handle values always
  // duplicate to the same real handle value when DUPLICATE_CLOSE_SOURCE is
  // specified, presumably because the returned handle is closed before it's
  // even returned. For example, duplicating -10 with DUPLICATE_CLOSE_SOURCE
  // always yields the handle value 0x50. This ends up interacting poorly with
  // the rest of Mojo's handle deserialization code and eventually crashes
  // in ScopedHandleVerifier.
  //
  // We avoid the issue by explicitly discarding any known pseudo handle values,
  // since they are always invalid when received from a remote process anyway
  // and thus always signal a misbehaving client.
  static bool IsPseudoHandle(HANDLE handle);

  // Returns a new local handle, with ownership of |handle| being transferred
  // from |owning_process| to the caller.
  static PlatformHandle TakeIncomingRemoteHandle(
      HANDLE handle,
      base::ProcessHandle owning_process);
#endif

 private:
#if BUILDFLAG(IS_WIN)
  // We don't use a ScopedHandle (or, by extension, PlatformHandle) here because
  // the handle verifier expects all handle values to be owned by this process.
  // On Windows we use |handle_| for locally owned handles and |remote_handle_|
  // otherwise. On all other platforms we use |handle_| regardless of ownership.
  HANDLE remote_handle_ = INVALID_HANDLE_VALUE;
#endif

  PlatformHandle handle_;
  base::Process owning_process_;
};

}  // namespace core
}  // namespace mojo

#endif  // MOJO_CORE_PLATFORM_HANDLE_IN_TRANSIT_H_
