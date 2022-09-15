// Copyright 2006-2008 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_NAMED_PIPE_INTERCEPTION_H_
#define SANDBOX_WIN_SRC_NAMED_PIPE_INTERCEPTION_H_

#include "sandbox/win/src/nt_internals.h"
#include "sandbox/win/src/sandbox_types.h"

namespace sandbox {

extern "C" {

typedef HANDLE(WINAPI* CreateNamedPipeWFunction)(
    LPCWSTR lpName,
    DWORD dwOpenMode,
    DWORD dwPipeMode,
    DWORD nMaxInstances,
    DWORD nOutBufferSize,
    DWORD nInBufferSize,
    DWORD nDefaultTimeOut,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes);

// Interception of CreateNamedPipeW in kernel32.dll
SANDBOX_INTERCEPT HANDLE WINAPI
TargetCreateNamedPipeW(CreateNamedPipeWFunction orig_CreateNamedPipeW,
                       LPCWSTR pipe_name,
                       DWORD open_mode,
                       DWORD pipe_mode,
                       DWORD max_instance,
                       DWORD out_buffer_size,
                       DWORD in_buffer_size,
                       DWORD default_timeout,
                       LPSECURITY_ATTRIBUTES security_attributes);

}  // extern "C"

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_NAMED_PIPE_INTERCEPTION_H_
