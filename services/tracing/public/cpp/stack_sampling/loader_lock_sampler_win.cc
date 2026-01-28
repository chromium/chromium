// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/stack_sampling/loader_lock_sampler_win.h"

#include <ostream>

#include "base/check.h"
#include "base/win/windows_types.h"
#include "services/tracing/public/cpp/buildflags.h"

#if !BUILDFLAG(ENABLE_LOADER_LOCK_SAMPLING)
static_assert(false,
              "Loader lock sampling should only be compiled in 64-bit builds.");
#endif

// Function signatures are derived from
// https://www.geoffchappell.com/studies/windows/win32/ntdll/api/ldrapi/lockloaderlock.htm
// The signature there only works on 32-bit - use of ULONG_PTR for the cookie
// is from https://www.winehq.org/pipermail/wine-cvs/2014-June/102116.html.
//
// This signature should work for 32-bit and 64-bit builds but it is only
// enabled for 64-bit for now because 32-bit is not well tested.
// (TracingSampleProfilerTest.SampleLoaderLockAlwaysHeld fails for an unknown
// reason.)
//
// TODO(crbug.com/40124365): Read the critical section directly, as crashpad
// does in third_party/crashpad/crashpad/util/win/loader_lock.cc. This should
// work on all versions since it's stable enough to ship with crashpad.
extern "C" NTSTATUS NTAPI LdrLockLoaderLock(ULONG flags,
                                            ULONG* state,
                                            ULONG_PTR* cookie);
extern "C" NTSTATUS NTAPI LdrUnlockLoaderLock(ULONG flags, ULONG_PTR cookie);

namespace tracing {

ProbingLoaderLockSampler::ProbingLoaderLockSampler() {
  // IsLoaderLockHeld should always be called from the same thread but it
  // doesn't need to be this thread.
  DETACH_FROM_THREAD(thread_checker_);
}

ProbingLoaderLockSampler::~ProbingLoaderLockSampler() = default;

bool ProbingLoaderLockSampler::IsLoaderLockHeld() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // All constants are derived from
  // https://www.geoffchappell.com/studies/windows/win32/ntdll/api/ldrapi/lockloaderlock.htm.
  constexpr ULONG kDoNotWaitFlag = 0x2;
  constexpr ULONG kEnteredLockState = 0x01;

  ULONG state = 0;
  ULONG_PTR cookie = 0;
  NTSTATUS status = ::LdrLockLoaderLock(kDoNotWaitFlag, &state, &cookie);
  if (status < 0) {
    // Loader lock state unknown; default to false.
    return false;
  }

  if (state == kEnteredLockState) {
    // Keeping the loader lock would be very bad, so raise an exception if that
    // happens.
    constexpr ULONG kRaiseExceptionOnErrorFlag = 0x1;
    ::LdrUnlockLoaderLock(kRaiseExceptionOnErrorFlag, cookie);
    // Since this thread was able to take the loader lock, no other thread held
    // it during the sample.
    return false;
  }

  // Since this thread was not able to take the loader lock, another thread
  // held it during the sample.
  return true;
}

}  // namespace tracing
