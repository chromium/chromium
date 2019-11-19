// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/public/isolate_holder.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/message_loop/message_loop_current.h"
#include "base/single_thread_task_runner.h"
#include "base/system/sys_info.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "gin/debug_impl.h"
#include "gin/function_template.h"
#include "gin/per_isolate_data.h"
#include "gin/v8_initializer.h"
#include "gin/v8_isolate_memory_dump_provider.h"
#include "gin/v8_shared_memory_dump_provider.h"

namespace gin {

namespace {
v8::ArrayBuffer::Allocator* g_array_buffer_allocator = nullptr;
const intptr_t* g_reference_table = nullptr;
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
    IsolateCreationMode isolate_creation_mode)
    : access_mode_(access_mode), isolate_type_(isolate_type) {
  DCHECK(task_runner);
  DCHECK(task_runner->BelongsToCurrentThread());

  v8::ArrayBuffer::Allocator* allocator = g_array_buffer_allocator;
  CHECK(allocator) << "You need to invoke gin::IsolateHolder::Initialize first";

  isolate_ = v8::Isolate::Allocate();
  isolate_data_.reset(
      new PerIsolateData(isolate_, allocator, access_mode_, task_runner));
  if (isolate_creation_mode == IsolateCreationMode::kCreateSnapshot) {
    // This branch is called when creating a V8 snapshot for Blink.
    // Note SnapshotCreator calls isolate->Enter() in its construction.
    snapshot_creator_.reset(
        new v8::SnapshotCreator(isolate_, g_reference_table));
    DCHECK_EQ(isolate_, snapshot_creator_->GetIsolate());
  } else {
    v8::Isolate::CreateParams params;
    params.code_event_handler = DebugImpl::GetJitCodeEventHandler();
    params.constraints.ConfigureDefaults(
        base::SysInfo::AmountOfPhysicalMemory(),
        base::SysInfo::AmountOfVirtualMemory());
    params.array_buffer_allocator = allocator;
    params.allow_atomics_wait =
        atomics_wait_mode == AllowAtomicsWaitMode::kAllowAtomicsWait;
    params.external_references = g_reference_table;
    params.only_terminate_in_safe_scope = true;

    v8::Isolate::Initialize(isolate_, params);
  }

  // This will attempt register the shared memory dump provider for every
  // IsolateHolder, but only the first registration will have any effect.
  gin::V8SharedMemoryDumpProvider::Register();

  isolate_memory_dump_provider_.reset(
      new V8IsolateMemoryDumpProvider(this, task_runner));
}

IsolateHolder::~IsolateHolder() {
  isolate_memory_dump_provider_.reset();
  isolate_data_.reset();
  isolate_->Dispose();
  isolate_ = nullptr;
}

// static
void IsolateHolder::Initialize(ScriptMode mode,
                               v8::ArrayBuffer::Allocator* allocator,
                               const intptr_t* reference_table) {
  CHECK(allocator);
  V8Initializer::Initialize(mode);
  g_array_buffer_allocator = allocator;
  g_reference_table = reference_table;
}

void IsolateHolder::EnableIdleTasks(
    std::unique_ptr<V8IdleTaskRunner> idle_task_runner) {
  DCHECK(isolate_data_.get());
  isolate_data_->EnableIdleTasks(std::move(idle_task_runner));
}

}  // namespace gin
