// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_HEAP_STATS_COLLECTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_HEAP_STATS_COLLECTOR_H_

#include "third_party/blink/renderer/platform/heap/heap_buildflags.h"

#if BUILDFLAG(BLINK_HEAP_USE_V8_OILPAN)
#include "third_party/blink/renderer/platform/heap/v8_wrapper/heap_stats_collector.h"
#else  // !BLINK_HEAP_USE_V8_OILPAN
#include "third_party/blink/renderer/platform/heap/impl/heap_stats_collector.h"
#endif  // !BLINK_HEAP_USE_V8_OILPAN

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_HEAP_STATS_COLLECTOR_H_
