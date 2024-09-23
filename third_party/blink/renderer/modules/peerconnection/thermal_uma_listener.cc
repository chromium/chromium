// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/thermal_uma_listener.h"

#include <memory>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "third_party/blink/public/mojom/peerconnection/peer_connection_tracker.mojom-blink.h"

namespace blink {

namespace {

const base::TimeDelta kStatsReportingPeriod = base::Minutes(1);

enum class ThermalStateUMA {
  kNominal = 0,
  kFair = 1,
  kSerious = 2,
  kCritical = 3,
  kMaxValue = kCritical,
};

ThermalStateUMA ToThermalStateUMA(mojom::blink::DeviceThermalState state) {
  switch (state) {
    case mojom::blink::DeviceThermalState::kNominal:
      return ThermalStateUMA::kNominal;
    case mojom::blink::DeviceThermalState::kFair:
      return ThermalStateUMA::kFair;
    case mojom::blink::DeviceThermalState::kSerious:
      return ThermalStateUMA::kSerious;
    case mojom::blink::DeviceThermalState::kCritical:
      return ThermalStateUMA::kCritical;
    default:
      NOTREACHED_IN_MIGRATION();
      return ThermalStateUMA::kNominal;
  }
}

}  // namespace

// static
std::unique_ptr<ThermalUmaListener> ThermalUmaListener::Create(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  std::unique_ptr<ThermalUmaListener> listener =
      std::make_unique<ThermalUmaListener>(std::move(task_runner));
  listener->ScheduleReport();
  return listener;
}

ThermalUmaListener::ThermalUmaListener(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)),
      current_thermal_state_(mojom::blink::DeviceThermalState::kUnknown),
      weak_ptr_factor_(this) {
  DCHECK(task_runner_);
}

void ThermalUmaListener::OnThermalMeasurement(
    mojom::blink::DeviceThermalState measurement) {
  base::AutoLock crit(lock_);
  current_thermal_state_ = measurement;
}

void ThermalUmaListener::ScheduleReport() {
  task_runner_->PostDelayedTask(FROM_HERE,
                                base::BindOnce(&ThermalUmaListener::ReportStats,
                                               weak_ptr_factor_.GetWeakPtr()),
                                kStatsReportingPeriod);
}

void ThermalUmaListener::ReportStats() {
  {
    base::AutoLock crit(lock_);
    if (current_thermal_state_ != mojom::blink::DeviceThermalState::kUnknown) {
      UMA_HISTOGRAM_ENUMERATION("WebRTC.PeerConnection.ThermalState",
                                ToThermalStateUMA(current_thermal_state_));
    }
  }
  ScheduleReport();
}

}  // namespace blink
