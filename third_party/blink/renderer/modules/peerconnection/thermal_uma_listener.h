// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_THERMAL_UMA_LISTENER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_THERMAL_UMA_LISTENER_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "third_party/blink/public/mojom/peerconnection/peer_connection_tracker.mojom-blink.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

// Tracks the thermal stats and logs to a UMA histogram.
class MODULES_EXPORT ThermalUmaListener {
 public:
  static std::unique_ptr<ThermalUmaListener> Create(
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  explicit ThermalUmaListener(
      scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~ThermalUmaListener() = default;

  void OnThermalMeasurement(mojom::blink::DeviceThermalState measurement);

 private:
  void ReportStats();
  void ScheduleReport();

  base::Lock lock_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  mojom::blink::DeviceThermalState current_thermal_state_ GUARDED_BY(&lock_);
  base::WeakPtrFactory<ThermalUmaListener> weak_ptr_factor_;
};

}  //  namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_THERMAL_UMA_LISTENER_H_
