// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_THREAD_STATE_SCOPES_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_THREAD_STATE_SCOPES_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/buildflags.h"

#if BUILDFLAG(USE_V8_OILPAN)
#include "third_party/blink/renderer/platform/heap/v8_wrapper/thread_state_scopes.h"
#else  // !USE_V8_OILPAN
#include "third_party/blink/renderer/platform/heap/impl/thread_state_scopes.h"
#endif  // !USE_V8_OILPAN

namespace blink {

#if defined(LEAK_SANITIZER)
class LsanDisabledScope final {
  STACK_ALLOCATED();

 public:
  explicit LsanDisabledScope() { __lsan_disable(); }

  ~LsanDisabledScope() { __lsan_enable(); }

  LsanDisabledScope(const LsanDisabledScope&) = delete;
  LsanDisabledScope& operator=(const LsanDisabledScope&) = delete;
};

#define LEAK_SANITIZER_DISABLED_SCOPE LsanDisabledScope lsan_disabled_scope
#else
#define LEAK_SANITIZER_DISABLED_SCOPE
#endif

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_THREAD_STATE_SCOPES_H_
