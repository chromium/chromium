// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/debug_impl.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include "v8/include/v8-initialization.h"
#endif

namespace gin {

namespace {
v8::JitCodeEventHandler g_jit_code_event_handler = NULL;
}  // namespace

// static
void Debug::SetJitCodeEventHandler(v8::JitCodeEventHandler event_handler) {
  g_jit_code_event_handler = event_handler;
}

#if BUILDFLAG(IS_WIN)
// static
void Debug::SetUnhandledExceptionCallback(
    v8::UnhandledExceptionCallback callback) {
  v8::V8::SetUnhandledExceptionCallback(callback);
}
#endif

// static
v8::JitCodeEventHandler DebugImpl::GetJitCodeEventHandler() {
  return g_jit_code_event_handler;
}

}  // namespace gin
