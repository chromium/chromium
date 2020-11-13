// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_GARBAGE_COLLECTED_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_GARBAGE_COLLECTED_H_

#include "third_party/blink/renderer/platform/wtf/buildflags.h"

#if BUILDFLAG(USE_V8_OILPAN)
#include "third_party/blink/renderer/platform/heap/v8_wrapper/garbage_collected.h"
#else  // !USE_V8_OILPAN
#include "third_party/blink/renderer/platform/heap/impl/garbage_collected.h"
#endif  // !USE_V8_OILPAN

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_GARBAGE_COLLECTED_H_
