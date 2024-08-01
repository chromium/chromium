// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/use_case.h"

namespace blink::scheduler {

// static
const char* UseCaseToString(UseCase use_case) {
  switch (use_case) {
    case UseCase::kNone:
      return "none";
    case UseCase::kCompositorGesture:
      return "compositor_gesture";
    case UseCase::kMainThreadCustomInputHandling:
      return "main_thread_custom_input_handling";
    case UseCase::kSynchronizedGesture:
      return "synchronized_gesture";
    case UseCase::kTouchstart:
      return "touchstart";
    case UseCase::kEarlyLoading:
      return "early_loading";
    case UseCase::kLoading:
      return "loading";
    case UseCase::kMainThreadGesture:
      return "main_thread_gesture";
    case UseCase::kDiscreteInputResponse:
      return "discrete_input_response";
  }
}

}  // namespace blink::scheduler
