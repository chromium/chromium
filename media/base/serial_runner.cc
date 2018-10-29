// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/serial_runner.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"

namespace media {

// Converts a Closure into a bound function accepting a PipelineStatusCB.
static void RunClosure(
    const base::Closure& closure,
    const PipelineStatusCB& status_cb) {
  closure.Run();
  status_cb.Run(PIPELINE_OK);
}

// Converts a bound function accepting a Closure into a bound function
// accepting a PipelineStatusCB. Since closures have no way of reporting a
// status |status_cb| is executed with PIPELINE_OK.
static void RunBoundClosure(
    const SerialRunner::BoundClosure& bound_closure,
    const PipelineStatusCB& status_cb) {
  bound_closure.Run(base::Bind(status_cb, PIPELINE_OK));
}

// Runs |status_cb| with |last_status| on |task_runner|.
static void RunOnTaskRunner(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    const PipelineStatusCB& status_cb,
    PipelineStatus last_status) {
  // Force post to permit cancellation of a series in the scenario where all
  // bound functions run on the same thread.
  task_runner->PostTask(FROM_HERE, base::BindOnce(status_cb, last_status));
}

SerialRunner::Queue::Queue() = default;
SerialRunner::Queue::Queue(const Queue& other) = default;
SerialRunner::Queue::~Queue() = default;

void SerialRunner::Queue::Push(const base::Closure& closure) {
  bound_fns_.push(base::Bind(&RunClosure, closure));
}

void SerialRunner::Queue::Push(
    const BoundClosure& bound_closure) {
  bound_fns_.push(base::Bind(&RunBoundClosure, bound_closure));
}

void SerialRunner::Queue::Push(
    const BoundPipelineStatusCB& bound_status_cb) {
  bound_fns_.push(bound_status_cb);
}

SerialRunner::BoundPipelineStatusCB SerialRunner::Queue::Pop() {
  BoundPipelineStatusCB bound_fn = bound_fns_.front();
  bound_fns_.pop();
  return bound_fn;
}

bool SerialRunner::Queue::empty() {
  return bound_fns_.empty();
}

SerialRunner::SerialRunner(const Queue& bound_fns,
                           const PipelineStatusCB& done_cb)
    : task_runner_(base::ThreadTaskRunnerHandle::Get()),
      bound_fns_(bound_fns),
      done_cb_(done_cb),
      weak_factory_(this) {
  // Respect both cancellation and calling stack guarantees for |done_cb|
  // when empty.
  if (bound_fns_.empty()) {
    task_runner_->PostTask(FROM_HERE,
                           base::Bind(&SerialRunner::RunNextInSeries,
                                      weak_factory_.GetWeakPtr(),
                                      PIPELINE_OK));
    return;
  }

  RunNextInSeries(PIPELINE_OK);
}

SerialRunner::~SerialRunner() = default;

std::unique_ptr<SerialRunner> SerialRunner::Run(
    const Queue& bound_fns,
    const PipelineStatusCB& done_cb) {
  std::unique_ptr<SerialRunner> callback_series(
      new SerialRunner(bound_fns, done_cb));
  return callback_series;
}

void SerialRunner::RunNextInSeries(PipelineStatus last_status) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(done_cb_);

  if (bound_fns_.empty() || last_status != PIPELINE_OK) {
    std::move(done_cb_).Run(last_status);
    return;
  }

  BoundPipelineStatusCB bound_fn = bound_fns_.Pop();
  bound_fn.Run(base::Bind(
      &RunOnTaskRunner,
      task_runner_,
      base::Bind(&SerialRunner::RunNextInSeries, weak_factory_.GetWeakPtr())));
}

}  // namespace media
