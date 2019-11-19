// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/public/thread_type.h"

#include "base/logging.h"

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
    case ThreadType::kAudioWorkletThread:
      return "AudioWorklet thread";
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
    case ThreadType::kCount:
      NOTREACHED();
      return nullptr;
  }
  return nullptr;
}

}  // namespace blink
