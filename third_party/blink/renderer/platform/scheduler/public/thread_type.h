// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_THREAD_TYPE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_THREAD_TYPE_H_

#include "third_party/blink/public/platform/web_common.h"

namespace blink {

enum class ThreadType {
  kCompositorThread,
  kDedicatedWorkerThread,
  kSharedWorkerThread,
  kAnimationAndPaintWorkletThread,
  kServiceWorkerThread,
  kOfflineAudioRenderThread,
  kHRTFDatabaseLoaderThread,
  kTestThread,
  kOfflineAudioWorkletThread,
  kRealtimeAudioWorkletThread,
  kSemiRealtimeAudioWorkletThread,
  kFontThread,
  kPreloadScannerThread,
};

BLINK_PLATFORM_EXPORT const char* GetNameForThreadType(ThreadType);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_THREAD_TYPE_H_
