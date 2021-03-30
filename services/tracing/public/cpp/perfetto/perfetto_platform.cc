// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/perfetto_platform.h"

#include "base/deferred_sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/tracing/perfetto_task_runner.h"

namespace tracing {

PerfettoPlatform::PerfettoPlatform()
    : deferred_task_runner_(new base::DeferredSequencedTaskRunner()),
      thread_local_object_([](void* object) {
        delete static_cast<ThreadLocalObject*>(object);
      }) {}

PerfettoPlatform::~PerfettoPlatform() = default;

void PerfettoPlatform::StartTaskRunner(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  DCHECK(!did_start_task_runner_);
  deferred_task_runner_->StartWithTaskRunner(task_runner);
  did_start_task_runner_ = true;
}

base::SequencedTaskRunner* PerfettoPlatform::task_runner() const {
  return deferred_task_runner_.get();
}

PerfettoPlatform::ThreadLocalObject*
PerfettoPlatform::GetOrCreateThreadLocalObject() {
  auto* object = static_cast<ThreadLocalObject*>(thread_local_object_.Get());
  if (!object) {
    object = ThreadLocalObject::CreateInstance().release();
    thread_local_object_.Set(object);
  }
  return object;
}

std::unique_ptr<perfetto::base::TaskRunner> PerfettoPlatform::CreateTaskRunner(
    const CreateTaskRunnerArgs&) {
  // We can't create a real task runner yet because the ThreadPool may not be
  // initialized. Instead, we point Perfetto to a buffering task runner which
  // will become active as soon as the thread pool is up (see StartTaskRunner).
  return std::make_unique<base::tracing::PerfettoTaskRunner>(
      deferred_task_runner_);
}

std::string PerfettoPlatform::GetCurrentProcessName() {
  // TODO(skyostil): Unimplemented since we're not registering producers through
  // the client API yet.
  return "";
}

}  // namespace tracing