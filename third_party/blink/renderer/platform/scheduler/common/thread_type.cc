// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/public/thread_type.h"

#include "base/notreached.h"

namespace blink {

const char* GetNameForThreadType(ThreadType thread_type) {
  switch (thread_type) {
    case ThreadType::kMainThread:
      return "Main thread";
    case ThreadType::kUnspecifiedWorkerThread:
      return "unspecified worker thread";
    case ThreadType::kCompositorThread:
      // Some benchmarks depend on this value.
      return "Compositor";
    case ThreadType::kDedicatedWorkerThread:
      return "DedicatedWorker thread";
    case ThreadType::kSharedWorkerThread:
      return "SharedWorker thread";
    case ThreadType::kAnimationAndPaintWorkletThread:
      return "AnimationWorklet thread";
    case ThreadType::kServiceWorkerThread:
      return "ServiceWorker thread";
    case ThreadType::kFileThread:
      return "File thread";
    case ThreadType::kDatabaseThread:
      return "Database thread";
    case ThreadType::kOfflineAudioRenderThread:
      return "OfflineAudioRender thread";
    case ThreadType::kReverbConvolutionBackgroundThread:
      return "Reverb convolution background thread";
    case ThreadType::kHRTFDatabaseLoaderThread:
      return "HRTF database loader thread";
    case ThreadType::kTestThread:
      return "test thread";
    case ThreadType::kAudioEncoderThread:
      return "Audio encoder thread";
    case ThreadType::kVideoEncoderThread:
      return "Video encoder thread";
    case ThreadType::kOfflineAudioWorkletThread:
      return "Offline AudioWorklet thread";
    case ThreadType::kRealtimeAudioWorkletThread:
      return "Realtime AudioWorklet thread";
    case ThreadType::kSemiRealtimeAudioWorkletThread:
      return "Semi-Realtime AudioWorklet thread";
    case ThreadType::kFontThread:
      return "Font thread";
    case ThreadType::kPreloadScannerThread:
      return "Preload scanner";
  }
  return nullptr;
}

}  // namespace blink
