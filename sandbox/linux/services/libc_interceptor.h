// Copyright 2018 The Chromium Authors
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
// When we are running in a sandbox, sometimes certain libc functions will fail.
// E.g. getaddrinfo() can fail in all sandboxes as glibc can run arbitrary
// third-party DNS resolution libraries which we have no hope of sandboxing.
//
// These libc functions need to be run in a separate process. If they are not
// called by third party code we can use Chrome's IPC (Mojo) to avoid ever
// calling these libc functions in a sandboxed process. For example
// getaddrinfo() is only called by first-party Chrome code and so it can be
// brokered to an unsandboxed process with regular IPC.
//
// But some of the libc calls are from third party libraries in the sandboxed
// process and they still must succeed. The motivating example is localtime - it
// needs to read /etc/localtime and sometimes a variety of locale files. We need
// to intercept these calls and proxy them to a parent process.
//
// For first-party cases we want to ensure the offending libc function is never
// called in a sandboxed process, and in third-party cases we want to proxy the
// call to a parent process. So in both cases we override the libc symbol: we
// define global functions for those functions which we wish to override. Since
// our own binary will be first in the dynamic resolution order, the dynamic
// linker will point callers to our versions of these functions.
//
// However, we have the same binary for both the browser and the renderers,
// which means that our overrides will apply in the browser too. In the browser
// process, the replacement functions must use dlsym with RTLD_NEXT to resolve
// the actual libc symbol, ignoring any symbols in the current module. In the
// sandboxed process, we need to either proxy the call to the parent over the
// IPC back-channel (see
// https://chromium.googlesource.com/chromium/src/+/main/docs/linux/sandbox_ipc.md),
// or if the libc call is not allowed ("first-party case") then we should
// generate a crash dump and possibly continue with the libc call.
//
// Use SetAmZygoteOrRenderer() below to control the proxying of libc calls such
// as localtime, which defaults using the dlsym approach.
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

// Turns on/off the libc interception. Called by the zygote and inherited by it
// children. |backchannel_fd| must be the fd to use for proxying calls.
SANDBOX_EXPORT void SetAmZygoteOrRenderer(bool enable, int backchannel_fd);

// Initializes libc interception. Must be called before sandbox lock down.
SANDBOX_EXPORT void InitLibcLocaltimeFunctions();

// Any calls to getaddrinfo() will crash in debug builds, or in release/official
// builds will trigger a crash dump and continue to call the actual
// getaddrinfo().
// TODO(mdpenton): change to DisallowGetaddrinfo() once this has been
// sufficiently tested in the wild.
SANDBOX_EXPORT void DiscourageGetaddrinfo();

}  // namespace sandbox

#endif  // SANDBOX_LINUX_SERVICES_LIBC_INTERCEPTOR_H_
