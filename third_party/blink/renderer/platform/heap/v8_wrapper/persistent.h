// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_PERSISTENT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_PERSISTENT_H_

#include "v8/include/cppgc/persistent.h"

namespace blink {

template <typename T>
using Persistent = cppgc::Persistent<T>;

template <typename T>
using WeakPersistent = cppgc::WeakPersistent<T>;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_PERSISTENT_H_
