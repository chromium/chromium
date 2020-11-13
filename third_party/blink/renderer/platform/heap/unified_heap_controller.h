// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_UNIFIED_HEAP_CONTROLLER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_UNIFIED_HEAP_CONTROLLER_H_

#include "third_party/blink/renderer/platform/wtf/buildflags.h"

#if BUILDFLAG(USE_V8_OILPAN)
#include "third_party/blink/renderer/platform/heap/v8_wrapper/unified_heap_controller.h"
#else  // !USE_V8_OILPAN
#include "third_party/blink/renderer/platform/heap/impl/unified_heap_controller.h"
#endif  // !USE_V8_OILPAN

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_UNIFIED_HEAP_CONTROLLER_H_
