// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CONTROLLER_BLINK_LEAK_DETECTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CONTROLLER_BLINK_LEAK_DETECTOR_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/leak_detector/leak_detector.mojom-blink.h"
#include "third_party/blink/renderer/controller/controller_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/timer.h"

namespace blink {

// Implementation of Leak Detector.
class CONTROLLER_EXPORT BlinkLeakDetector : public mojom::blink::LeakDetector {
 public:
  static void Create(mojo::PendingReceiver<mojom::blink::LeakDetector>);

  BlinkLeakDetector();
  ~BlinkLeakDetector() override;

 private:
  // mojom::blink::LeakDetector implementation.
  void PerformLeakDetection(PerformLeakDetectionCallback) override;

  void TimerFiredGC(TimerBase*);
  void ReportResult();

  TaskRunnerTimer<BlinkLeakDetector> delayed_gc_timer_;
  int number_of_gc_needed_ = 0;
  PerformLeakDetectionCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(BlinkLeakDetector);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CONTROLLER_BLINK_LEAK_DETECTOR_H_
