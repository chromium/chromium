// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/compute_pressure/platform_collector.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "services/device/compute_pressure/cpu_probe.h"
#include "services/device/compute_pressure/pressure_sample.h"

namespace device {

namespace {

scoped_refptr<base::SequencedTaskRunner> CreateProbeTaskRunner() {
  // While some samples can be collected without doing blocking operations,
  // this isn't guaranteed on all operating systems.
  return base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
}

}  // namespace

PlatformCollector::PlatformCollector(
    std::unique_ptr<CpuProbe> probe,
    base::TimeDelta sampling_interval,
    base::RepeatingCallback<void(mojom::PressureState)> sampling_callback)
    : probe_task_runner_(CreateProbeTaskRunner()),
      probe_(std::move(probe)),
      sampling_interval_(sampling_interval),
      sampling_callback_(std::move(sampling_callback)) {
  DCHECK(sampling_callback_);
}

PlatformCollector::~PlatformCollector() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  probe_task_runner_->DeleteSoon(FROM_HERE, std::move(probe_));
}

void PlatformCollector::EnsureStarted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(has_probe()) << __func__
                      << " should not be called if has_probe() returns false";

  DCHECK(probe_);
  if (timer_.IsRunning())
    return;

  DCHECK(!got_probe_baseline_) << "got_probe_baseline_ incorrectly reset";

  // Schedule the first CpuProbe update right away. This update's result will
  // not be reported, thanks to the accounting done by `got_probe_baseline_`.
  UpdateProbe();

  timer_.Start(FROM_HERE, sampling_interval_,
               base::BindRepeating(&PlatformCollector::UpdateProbe,
                                   weak_factory_.GetWeakPtr()));
}

void PlatformCollector::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  timer_.AbandonAndStop();
  // There are still a number of calls to DidUpdateProbe() queued via
  // PostTaskAndReplyWithResult() in UpdateProbe(). This can make sure
  // all pending posted tasks will not run because of the invalid WeakPtrs.
  weak_factory_.InvalidateWeakPtrs();
  got_probe_baseline_ = false;
}

void PlatformCollector::UpdateProbe() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(has_probe());

  // Raw CpuProbe pointer usage is safe here because the CpuProbe instance will
  // be destroyed by queueing a task on `probe_task_runner_`. That task must get
  // queued after this task.
  probe_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          [](CpuProbe* probe) -> PressureSample {
            probe->Update();
            return probe->LastSample();
          },
          probe_.get()),
      base::BindOnce(&PlatformCollector::DidUpdateProbe,
                     weak_factory_.GetWeakPtr()));
}

void PlatformCollector::DidUpdateProbe(PressureSample sample) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(timer_.IsRunning());

  // Don't report the first update result.
  if (!got_probe_baseline_) {
    got_probe_baseline_ = true;
    return;
  }

  sampling_callback_.Run(CalculateState(sample));
}

mojom::PressureState PlatformCollector::CalculateState(
    PressureSample sample) const {
  // TODO(crbug.com/1342528): A more advanced algorithm that calculates
  // PressureState using PressureSample needs to be determined.
  // At this moment the algorithm is the simplest possible
  // with thresholds defining the state.
  mojom::PressureState state = mojom::PressureState::kNominal;
  if (sample.cpu_utilization < 0.3)
    state = mojom::PressureState::kNominal;
  else if (sample.cpu_utilization < 0.6)
    state = mojom::PressureState::kFair;
  else if (sample.cpu_utilization < 0.9)
    state = mojom::PressureState::kSerious;
  else if (sample.cpu_utilization <= 1.00)
    state = mojom::PressureState::kCritical;
  else
    NOTREACHED() << "unexpected value: " << sample.cpu_utilization;

  return state;
}

}  // namespace device
