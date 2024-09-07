// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/public/isolate_holder.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <memory>
#include <optional>
#include <utility>

#include "base/check_op.h"
#include "base/system/sys_info.h"
#include "base/task/current_thread.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "gin/debug_impl.h"
#include "gin/function_template.h"
#include "gin/per_isolate_data.h"
#include "gin/v8_initializer.h"
#include "gin/v8_isolate_memory_dump_provider.h"
#include "gin/v8_shared_memory_dump_provider.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-locker.h"
#include "v8/include/v8-snapshot.h"

namespace gin {

namespace {
v8::ArrayBuffer::Allocator* g_array_buffer_allocator = nullptr;
const intptr_t* g_reference_table = nullptr;
v8::FatalErrorCallback g_fatal_error_callback = nullptr;
v8::OOMErrorCallback g_oom_error_callback = nullptr;

std::unique_ptr<v8::Isolate::CreateParams> getModifiedIsolateParams(
    std::unique_ptr<v8::Isolate::CreateParams> params,
    IsolateHolder::AllowAtomicsWaitMode atomics_wait_mode,
    v8::CreateHistogramCallback create_histogram_callback,
    v8::AddHistogramSampleCallback add_histogram_sample_callback) {
  params->create_histogram_callback = create_histogram_callback;
  params->add_histogram_sample_callback = add_histogram_sample_callback;
  params->allow_atomics_wait =
      atomics_wait_mode ==
      IsolateHolder::AllowAtomicsWaitMode::kAllowAtomicsWait;
  params->array_buffer_allocator = g_array_buffer_allocator;
  return params;
}
}  // namespace

IsolateHolder::IsolateHolder(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    IsolateType isolate_type)
    : IsolateHolder(std::move(task_runner),
                    AccessMode::kSingleThread,
                    isolate_type) {}

IsolateHolder::IsolateHolder(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    AccessMode access_mode,
    IsolateType isolate_type)
    : IsolateHolder(std::move(task_runner),
                    access_mode,
                    kAllowAtomicsWait,
                    isolate_type,
                    IsolateCreationMode::kNormal) {}

IsolateHolder::IsolateHolder(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    AccessMode access_mode,
    AllowAtomicsWaitMode atomics_wait_mode,
    IsolateType isolate_type,
    IsolateCreationMode isolate_creation_mode,
    v8::CreateHistogramCallback create_histogram_callback,
    v8::AddHistogramSampleCallback add_histogram_sample_callback,
    scoped_refptr<base::SingleThreadTaskRunner> user_visible_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> best_effort_task_runner)
    : IsolateHolder(std::move(task_runner),
                    access_mode,
                    isolate_type,
                    getModifiedIsolateParams(getDefaultIsolateParams(),
                                             atomics_wait_mode,
                                             create_histogram_callback,
                                             add_histogram_sample_callback),
                    isolate_creation_mode,
                    std::move(user_visible_task_runner),
                    std::move(best_effort_task_runner)) {}

IsolateHolder::IsolateHolder(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    AccessMode access_mode,
    IsolateType isolate_type,
    std::unique_ptr<v8::Isolate::CreateParams> params,
    IsolateCreationMode isolate_creation_mode,
    scoped_refptr<base::SingleThreadTaskRunner> user_visible_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> best_effort_task_runner)
    : access_mode_(access_mode), isolate_type_(isolate_type) {
  CHECK(Initialized())
      << "You need to invoke gin::IsolateHolder::Initialize first";

  DCHECK(task_runner);
  DCHECK(task_runner->BelongsToCurrentThread());

  v8::ArrayBuffer::Allocator* allocator = params->array_buffer_allocator;
  DCHECK(allocator);

  isolate_ = v8::Isolate::Allocate();
  isolate_data_ = std::make_unique<PerIsolateData>(
      isolate_, allocator, access_mode_, task_runner,
      std::move(user_visible_task_runner), std::move(best_effort_task_runner));
  //  TODO(crbug.com/40854483): Refactor such that caller need not
  //  provide params when creating a snapshot.
  if (isolate_creation_mode == IsolateCreationMode::kCreateSnapshot) {
    // This branch is called when creating a V8 snapshot for Blink.
    // Note SnapshotCreator calls isolate->Enter() in its construction.
    snapshot_creator_ =
        std::make_unique<v8::SnapshotCreator>(isolate_, g_reference_table);
    DCHECK_EQ(isolate_, snapshot_creator_->GetIsolate());
  } else {
    v8::Isolate::Initialize(isolate_, *params);
  }

  // This will attempt register the shared memory dump provider for every
  // IsolateHolder, but only the first registration will have any effect.
  gin::V8SharedMemoryDumpProvider::Register();

  isolate_memory_dump_provider_ =
      std::make_unique<V8IsolateMemoryDumpProvider>(this, task_runner);
}

IsolateHolder::~IsolateHolder() {
  isolate_memory_dump_provider_.reset();
  {
    std::optional<v8::Locker> locker;
    if (access_mode_ == AccessMode::kUseLocker) {
      locker.emplace(isolate_);
    }
    v8::Isolate::Scope isolate_scope(isolate_);
    isolate_data_->NotifyBeforeDispose();
  }
  // Calling Isolate::Dispose makes sure all threads which might access
  // PerIsolateData are finished.
  isolate_->Dispose();
  isolate_data_->NotifyDisposed();
  isolate_data_.reset();
  isolate_ = nullptr;
}

// static
void IsolateHolder::Initialize(ScriptMode mode,
                               v8::ArrayBuffer::Allocator* allocator,
                               const intptr_t* reference_table,
                               const std::string js_command_line_flags,
                               v8::FatalErrorCallback fatal_error_callback,
                               v8::OOMErrorCallback oom_error_callback) {
  CHECK(allocator);
  V8Initializer::Initialize(mode, js_command_line_flags, oom_error_callback);
  g_array_buffer_allocator = allocator;
  g_reference_table = reference_table;
  g_fatal_error_callback = fatal_error_callback;
  g_oom_error_callback = oom_error_callback;
}

// static
bool IsolateHolder::Initialized() {
  return g_array_buffer_allocator;
}

// static
std::unique_ptr<v8::Isolate::CreateParams>
IsolateHolder::getDefaultIsolateParams() {
  CHECK(Initialized())
      << "You need to invoke gin::IsolateHolder::Initialize first";
  // TODO(https://crbug.com/v8/13092): Remove usage of unique_ptr once V8
  // supports the move constructor on CreateParams.
  std::unique_ptr<v8::Isolate::CreateParams> params =
      std::make_unique<v8::Isolate::CreateParams>();
  params->code_event_handler = DebugImpl::GetJitCodeEventHandler();
  params->constraints.ConfigureDefaults(base::SysInfo::AmountOfPhysicalMemory(),
                                        base::SysInfo::AmountOfVirtualMemory());
  params->array_buffer_allocator = g_array_buffer_allocator;
  params->allow_atomics_wait = true;
  params->external_references = g_reference_table;
  params->embedder_wrapper_type_index = kWrapperInfoIndex;
  params->embedder_wrapper_object_index = kEncodedValueIndex;
  params->fatal_error_callback = g_fatal_error_callback;
  params->oom_error_callback = g_oom_error_callback;
  return params;
}

void IsolateHolder::EnableIdleTasks(
    std::unique_ptr<V8IdleTaskRunner> idle_task_runner) {
  DCHECK(isolate_data_.get());
  isolate_data_->EnableIdleTasks(std::move(idle_task_runner));
}

}  // namespace gin
