// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_SCOPED_PROCESS_HANDLE_H_
#define MOJO_CORE_SCOPED_PROCESS_HANDLE_H_

#include "base/macros.h"
#include "base/process/process_handle.h"
#include "build/build_config.h"

#if defined(OS_WIN)
#include "base/win/scoped_handle.h"
#endif

#if defined(OS_FUCHSIA)
#include <lib/zx/process.h>
#endif

namespace mojo {
namespace core {

// Wraps a |base::ProcessHandle| with additional scoped lifetime semantics on
// applicable platforms. For platforms where process handles aren't ownable
// references, this is just a wrapper around |base::ProcessHandle|.
//
// This essentially exists to support passing around process handles internally
// in a generic way while also supporting Windows process handle ownership
// semantics.
//
// A ScopedProcessHandle will never refer to the current process, and
// constructing a ScopedProcessHandle over the current process's handle is
// considered an error.
class ScopedProcessHandle {
 public:
  ScopedProcessHandle();

  // Assumes ownership of |handle|.
  explicit ScopedProcessHandle(base::ProcessHandle handle);

  ScopedProcessHandle(ScopedProcessHandle&&);

  ~ScopedProcessHandle();

  // Creates a new ScopedProcessHandle from a clone of |handle|.
  static ScopedProcessHandle CloneFrom(base::ProcessHandle handle);

  ScopedProcessHandle& operator=(ScopedProcessHandle&&);

  bool is_valid() const;
  base::ProcessHandle get() const;
  base::ProcessHandle release();

  ScopedProcessHandle Clone() const;

 private:
#if defined(OS_WIN)
  base::win::ScopedHandle handle_;
#elif defined(OS_FUCHSIA)
  zx::process process_;
#else
  base::ProcessHandle handle_ = base::kNullProcessHandle;
#endif

  DISALLOW_COPY_AND_ASSIGN(ScopedProcessHandle);
};

}  // namespace core
}  // namespace mojo

#endif  // MOJO_CORE_SCOPED_PROCESS_HANDLE_H_
