/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/wtf/wtf.h"

#include "build/build_config.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"
#include "third_party/blink/renderer/platform/wtf/date_math.h"
#include "third_party/blink/renderer/platform/wtf/dtoa.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/stack_util.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/copy_lchars_from_uchar_source.h"
#include "third_party/blink/renderer/platform/wtf/text/string_statics.h"
#include "third_party/blink/renderer/platform/wtf/thread_specific.h"
#include "third_party/blink/renderer/platform/wtf/threading.h"

namespace WTF {

namespace {

bool g_initialized = false;

#if defined(COMPONENT_BUILD) && BUILDFLAG(IS_WIN)
constinit thread_local bool g_is_main_thread = false;
#endif

}  // namespace

base::PlatformThreadId g_main_thread_identifier;

#if BUILDFLAG(IS_ANDROID)
// On Android going through libc (gettid) is faster than runtime-lib emulation.
bool IsMainThread() {
  return CurrentThread() == g_main_thread_identifier;
}
#elif defined(COMPONENT_BUILD) && BUILDFLAG(IS_WIN)
bool IsMainThread() {
  return g_is_main_thread;
}
#else
constinit thread_local bool g_is_main_thread = false;
#endif

void Initialize() {
  // WTF, and Blink in general, cannot handle being re-initialized.
  // Make that explicit here.
  CHECK(!g_initialized);
  g_initialized = true;
#if !BUILDFLAG(IS_ANDROID)
  g_is_main_thread = true;
#endif
  g_main_thread_identifier = CurrentThread();

  Threading::Initialize();

  internal::InitializeDoubleConverter();

  internal::InitializeMainThreadStackEstimate();
  AtomicString::Init();
  StringStatics::Init();
}

}  // namespace WTF
