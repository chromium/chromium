// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/platform_handle_utils.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_FUCHSIA)
#include <lib/zx/process.h>
#elif BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

namespace mojo {
namespace core {

MojoResult UnwrapAndClonePlatformProcessHandle(
    const MojoPlatformProcessHandle* process_handle,
    base::Process& process) {
  if (process_handle->struct_size < sizeof(*process_handle)) {
    return MOJO_RESULT_INVALID_ARGUMENT;
  }

#if BUILDFLAG(IS_WIN)
  base::ProcessHandle in_handle = reinterpret_cast<base::ProcessHandle>(
      static_cast<uintptr_t>(process_handle->value));
#else
  base::ProcessHandle in_handle =
      static_cast<base::ProcessHandle>(process_handle->value);
#endif

  if (in_handle == base::kNullProcessHandle) {
    process = base::Process();
    return MOJO_RESULT_OK;
  }

#if BUILDFLAG(IS_WIN)
  base::ProcessHandle out_handle;
  if (!::DuplicateHandle(::GetCurrentProcess(), in_handle,
                         ::GetCurrentProcess(), &out_handle, 0, FALSE,
                         DUPLICATE_SAME_ACCESS)) {
    return MOJO_RESULT_INVALID_ARGUMENT;
  }
  process = base::Process(out_handle);
#elif BUILDFLAG(IS_FUCHSIA)
  zx::process out;
  if (zx::unowned_process(in_handle)->duplicate(ZX_RIGHT_SAME_RIGHTS, &out) !=
      ZX_OK) {
    return MOJO_RESULT_INVALID_ARGUMENT;
  }
  process = base::Process(out.release());
#else
  process = base::Process(in_handle);
#endif
  return MOJO_RESULT_OK;
}

}  // namespace core
}  // namespace mojo
