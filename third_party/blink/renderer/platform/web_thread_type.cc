// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/web_thread_type.h"

#include "base/logging.h"

namespace blink {

const char* GetNameForThreadType(WebThreadType thread_type) {
  switch (thread_type) {
    case WebThreadType::kMainThread:
      return "Main thread";
    case WebThreadType::kUnspecifiedWorkerThread:
      return "unspecified worker thread";
    case WebThreadType::kCompositorThread:
      // Some benchmarks depend on this value.
      return "Compositor";
    case WebThreadType::kDedicatedWorkerThread:
      return "DedicatedWorker thread";
    case WebThreadType::kSharedWorkerThread:
      return "SharedWorker thread";
    case WebThreadType::kAnimationAndPaintWorkletThread:
      return "AnimationWorklet thread";
    case WebThreadType::kServiceWorkerThread:
      return "ServiceWorker thread";
    case WebThreadType::kAudioWorkletThread:
      return "AudioWorklet thread";
    case WebThreadType::kFileThread:
      return "File thread";
    case WebThreadType::kDatabaseThread:
      return "Database thread";
    case WebThreadType::kWebAudioThread:
      return "WebAudio thread";
    case WebThreadType::kScriptStreamerThread:
      return "ScriptStreamer thread";
    case WebThreadType::kOfflineAudioRenderThread:
      return "OfflineAudioRender thread";
    case WebThreadType::kReverbConvolutionBackgroundThread:
      return "Reverb convolution background thread";
    case WebThreadType::kHRTFDatabaseLoaderThread:
      return "HRTF database loader thread";
    case WebThreadType::kTestThread:
      return "test thread";
    case WebThreadType::kCount:
      NOTREACHED();
      return nullptr;
  }
  return nullptr;
}

}  // namespace blink
