// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SERVICES_LIBC_INTERCEPTOR_H_
#define SANDBOX_LINUX_SERVICES_LIBC_INTERCEPTOR_H_

#include <vector>

#include "base/files/scoped_file.h"
#include "base/pickle.h"
#include "build/build_config.h"
#include "sandbox/sandbox_export.h"

namespace sandbox {

// Sandbox interception of libc calls.
//
// When we are running in a namespace sandbox certain libc calls will fail
// (localtime being the motivating example - it needs to read /etc/localtime).
// We need to intercept these calls and proxy them to a parent process.
// However, these calls may come from us or from our third_party libraries, so
// in some cases we can't just change the code.
//
// It's for these cases that we have the following setup:
//
// We define global functions for those functions which we wish to override.
// Since we will be first in the dynamic resolution order, the dynamic linker
// will point callers to our versions of these functions. However, we have the
// same binary for both the browser and the renderers, which means that our
// overrides will apply in the browser too.
//
// Our replacement functions must handle both cases, and either proxy the call
// to the parent over the IPC back-channel (see
// https://chromium.googlesource.com/chromium/src/+/master/docs/linux_sandbox_ipc.md)
// or use dlsym with RTLD_NEXT to resolve the symbol, ignoring any symbols in
// the current module. Use SetUseLocaltimeOverride() and SetAmZygoteOrRenderer()
// below to control the mode of operation, which defaults using the dlsym
// approach.
//
// Other avenues:
//
// Our first attempt involved some assembly to patch the GOT of the current
// module. This worked, but was platform specific and doesn't catch the case
// where a library makes a call rather than current module.
//
// We also considered patching the function in place, but this would again by
// platform specific and the above technique seems to work well enough.

// Methods supported over the back-channel to the parent.
// This isn't the full list, values < 32 are reserved for methods called from
// Skia, and values >= 64 are reserved for sandbox_ipc_linux.cc.
enum InterceptedIPCMethods {
  METHOD_LOCALTIME = 32,
};

// Currently, only METHOD_LOCALTIME, returns false if |kind| is otherwise.
SANDBOX_EXPORT bool HandleInterceptedCall(
    int kind,
    int fd,
    base::PickleIterator iter,
    const std::vector<base::ScopedFD>& fds);

// On Linux, localtime is overridden to use a synchronous IPC to the browser
// process to determine the locale. This can be disabled, which causes
// localtime to use UTC instead. https://crbug.com/772503.
SANDBOX_EXPORT void SetUseLocaltimeOverride(bool enable);

// Turns on/off the libc interception. Called by the zygote and inherited by it
// children. |backchannel_fd| must be the fd to use for proxying calls.
SANDBOX_EXPORT void SetAmZygoteOrRenderer(bool enable, int backchannel_fd);

// Initializes libc interception. Must be called before sandbox lock down.
SANDBOX_EXPORT void InitLibcLocaltimeFunctions();

}  // namespace sandbox

#endif  // SANDBOX_LINUX_SERVICES_LIBC_INTERCEPTOR_H_
