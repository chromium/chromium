// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/threading.h"

#include <atomic>

#include "build/build_config.h"

namespace blink {

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_WIN)
base::PlatformThreadId CurrentThread() {
  thread_local base::PlatformThreadId g_id = base::PlatformThread::CurrentId();
  return g_id;
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_WIN)

// For debugging only -- whether a non-main thread has been created.

#if DCHECK_IS_ON()
namespace {
std::atomic_bool g_thread_created(false);
}  // namespace

bool IsBeforeThreadCreated() {
  return !g_thread_created;
}

void WillCreateThread() {
  g_thread_created = true;
}

void SetIsBeforeThreadCreatedForTest() {
  g_thread_created = false;
}
#endif

}  // namespace blink
