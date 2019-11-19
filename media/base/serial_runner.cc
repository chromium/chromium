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

// Converts a Closure into a bound function accepting a PipelineStatusCallback.
static void RunClosure(base::OnceClosure closure,
                       PipelineStatusCallback status_cb) {
  std::move(closure).Run();
  std::move(status_cb).Run(PIPELINE_OK);
}

// Converts a bound function accepting a Closure into a bound function
// accepting a PipelineStatusCallback. Since closures have no way of reporting a
// status |status_cb| is executed with PIPELINE_OK.
static void RunBoundClosure(SerialRunner::BoundClosure bound_closure,
                            PipelineStatusCallback status_cb) {
  std::move(bound_closure)
      .Run(base::BindOnce(std::move(status_cb), PIPELINE_OK));
}

// Runs |status_cb| with |last_status| on |task_runner|.
static void RunOnTaskRunner(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    PipelineStatusCallback status_cb,
    PipelineStatus last_status) {
  // Force post to permit cancellation of a series in the scenario where all
  // bound functions run on the same thread.
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(std::move(status_cb), last_status));
}

SerialRunner::Queue::Queue() = default;
SerialRunner::Queue::Queue(Queue&& other) = default;
SerialRunner::Queue::~Queue() = default;

void SerialRunner::Queue::Push(base::OnceClosure closure) {
  bound_fns_.push_back(base::BindOnce(&RunClosure, std::move(closure)));
}

void SerialRunner::Queue::Push(BoundClosure bound_closure) {
  bound_fns_.push_back(
      base::BindOnce(&RunBoundClosure, std::move(bound_closure)));
}

void SerialRunner::Queue::Push(BoundPipelineStatusCallback bound_status_cb) {
  bound_fns_.push_back(std::move(bound_status_cb));
}

SerialRunner::BoundPipelineStatusCallback SerialRunner::Queue::Pop() {
  BoundPipelineStatusCallback bound_fn = std::move(bound_fns_.front());
  bound_fns_.pop_front();
  return bound_fn;
}

bool SerialRunner::Queue::empty() {
  return bound_fns_.empty();
}

SerialRunner::SerialRunner(Queue&& bound_fns, PipelineStatusCallback done_cb)
    : task_runner_(base::ThreadTaskRunnerHandle::Get()),
      bound_fns_(std::move(bound_fns)),
      done_cb_(std::move(done_cb)) {
  // Respect both cancellation and calling stack guarantees for |done_cb|
  // when empty.
  if (bound_fns_.empty()) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&SerialRunner::RunNextInSeries,
                                  weak_factory_.GetWeakPtr(), PIPELINE_OK));
    return;
  }

  RunNextInSeries(PIPELINE_OK);
}

SerialRunner::~SerialRunner() = default;

std::unique_ptr<SerialRunner> SerialRunner::Run(
    Queue&& bound_fns,
    PipelineStatusCallback done_cb) {
  std::unique_ptr<SerialRunner> callback_series(
      new SerialRunner(std::move(bound_fns), std::move(done_cb)));
  return callback_series;
}

void SerialRunner::RunNextInSeries(PipelineStatus last_status) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(done_cb_);

  if (bound_fns_.empty() || last_status != PIPELINE_OK) {
    std::move(done_cb_).Run(last_status);
    return;
  }

  BoundPipelineStatusCallback bound_fn = bound_fns_.Pop();
  std::move(bound_fn).Run(
      base::BindRepeating(&RunOnTaskRunner, task_runner_,
                          base::BindRepeating(&SerialRunner::RunNextInSeries,
                                              weak_factory_.GetWeakPtr())));
}

}  // namespace media
