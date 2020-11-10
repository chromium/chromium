// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_THREAD_STATE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_THREAD_STATE_H_

#include "v8/include/cppgc/prefinalizer.h"

namespace blink {

#define USING_PRE_FINALIZER(Class, PreFinalizer) \
  CPPGC_USING_PRE_FINALIZER(Class, PreFinalizer)

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_THREAD_STATE_H_
