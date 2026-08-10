// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_THREAD_LOCAL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_THREAD_LOCAL_H_

#include "base/compiler_specific.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/heap/heap_buildflags.h"
#include "third_party/blink/renderer/platform/platform_export.h"

// On component builds, always hide the thread_local variable behind a call.
// This avoids complexity with "global-dyn" and allows to use "local-dyn"
// instead, across all platforms. On non-component (release) builds, don't hide
// the variable behind the call (to improve performance in access time), but use
// different tls models on different platforms. On Windows, since chrome is
// linked into the chrome.dll which is always linked to chrome.exe at static
// link time (DT_NEEDED in ELF terms), use "init-exec". On Android, since the
// library can be opened with "dlopen" (through JNI), use "local-dyn". On other
// systems (Linux/ChromeOS/MacOS) use the fastest "local-exec".

//         |_____component_____|___non-component___|
// ________|_tls_model__|_hide_|_tls_model__|_hide_|
// Windows | local-dyn  | yes  | init-exec  |  no  |
// Android | local-dyn  | yes  | local-dyn  |  no  |
// Other   | local-dyn  | yes  | local-exec |  no  |

// The call is still cheaper than multiple calls through WTF/base/pthread*
// layers.
#if BUILDFLAG(BLINK_HEAP_INSIDE_SHARED_LIBRARY)
#define BLINK_HEAP_HIDE_THREAD_LOCAL_IN_LIBRARY 1
#else
#define BLINK_HEAP_HIDE_THREAD_LOCAL_IN_LIBRARY 0
#endif

#if BLINK_HEAP_HIDE_THREAD_LOCAL_IN_LIBRARY
#define BLINK_HEAP_THREAD_LOCAL_MODEL "local-dynamic"
#else
#if BUILDFLAG(IS_WIN)
#define BLINK_HEAP_THREAD_LOCAL_MODEL "initial-exec"
#elif BUILDFLAG(IS_ANDROID)
#define BLINK_HEAP_THREAD_LOCAL_MODEL "local-dynamic"
#else
#define BLINK_HEAP_THREAD_LOCAL_MODEL "local-exec"
#endif
#endif

// Only inline the getter where the TLS model resolves the variable with a
// call-free access, so that inlining removes the call boundary entirely:
// "initial-exec" on Windows and "local-exec" on Linux/ChromeOS. Elsewhere the
// access is itself a call -- Android and all component builds go through
// __tls_get_addr ("local-dynamic"), and Apple routes every thread_local
// through Darwin's _tlv_get_addr thunk regardless of tls_model -- so keep the
// out-of-line NOINLINE getter that has always shipped.
// TODO(Shuangshuang): Inlining on Apple would still fold away the outer getter
// call (2 -> 1), but measured a regression on M1. Investigate whether copying
// the _tlv_get_addr sequence into every call site is the cause.
#if !BLINK_HEAP_HIDE_THREAD_LOCAL_IN_LIBRARY && !BUILDFLAG(IS_ANDROID) && \
    !BUILDFLAG(IS_APPLE)
#define BLINK_HEAP_INLINE_THREAD_LOCAL_GETTER 1
#else
#define BLINK_HEAP_INLINE_THREAD_LOCAL_GETTER 0
#endif

#if !BLINK_HEAP_INLINE_THREAD_LOCAL_GETTER

#define BLINK_HEAP_DECLARE_THREAD_LOCAL_GETTER(Name, Type, Member) \
  NOINLINE static Type Name();
#define BLINK_HEAP_DEFINE_THREAD_LOCAL_GETTER(Name, Type, Member) \
  NOINLINE Type Name() {                                          \
    return Member;                                                \
  }

#else  // BLINK_HEAP_INLINE_THREAD_LOCAL_GETTER

#define BLINK_HEAP_DECLARE_THREAD_LOCAL_GETTER(Name, Type, Member) \
  ALWAYS_INLINE static Type Name() {                               \
    return Member;                                                 \
  }
#define BLINK_HEAP_DEFINE_THREAD_LOCAL_GETTER(Name, Type, Member)

#endif  // BLINK_HEAP_INLINE_THREAD_LOCAL_GETTER

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_THREAD_LOCAL_H_
