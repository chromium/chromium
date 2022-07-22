// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/compute_pressure/platform_collector.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequenced_task_runner_handle.h"
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
    base::RepeatingCallback<void(PressureSample)> sampling_callback)
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

  // base::Unretained usage is safe here because base::RepeatingTimer guarantees
  // that its callback will not be called after it goes out of scope.
  timer_.Start(FROM_HERE, sampling_interval_,
               base::BindRepeating(&PlatformCollector::UpdateProbe,
                                   base::Unretained(this)));
}

void PlatformCollector::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  timer_.AbandonAndStop();
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

  // Don't report the update result if Stop() was called.
  if (!timer_.IsRunning())
    return;

  // Don't report the first update result.
  if (!got_probe_baseline_) {
    got_probe_baseline_ = true;
    return;
  }

  sampling_callback_.Run(sample);
}

}  // namespace device
