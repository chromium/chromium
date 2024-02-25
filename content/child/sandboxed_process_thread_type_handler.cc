// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/sandboxed_process_thread_type_handler.h"

#include "content/child/child_thread_impl.h"
#include "content/common/thread_type_switcher.mojom.h"

namespace content {

namespace {

SandboxedProcessThreadTypeHandler* g_sandboxed_process_thread_type_handler =
    nullptr;

}  // namespace

SandboxedProcessThreadTypeHandler::SandboxedProcessThreadTypeHandler() {
  // `thread_type_switcher_receiver_` will be bound when ChildThreadImpl is
  // available.
  thread_type_switcher_receiver_ =
      thread_type_switcher_.BindNewPipeAndPassReceiver();
  base::PlatformThread::SetThreadTypeDelegate(this);
}

SandboxedProcessThreadTypeHandler::~SandboxedProcessThreadTypeHandler() {
  base::PlatformThread::SetThreadTypeDelegate(nullptr);
}

// static
void SandboxedProcessThreadTypeHandler::Create() {
  DCHECK(!g_sandboxed_process_thread_type_handler);

  g_sandboxed_process_thread_type_handler =
      new SandboxedProcessThreadTypeHandler();
}

// static
void SandboxedProcessThreadTypeHandler::NotifyMainChildThreadCreated() {
  if (g_sandboxed_process_thread_type_handler) {
    g_sandboxed_process_thread_type_handler->ConnectThreadTypeSwitcher();
  }
}

// static
SandboxedProcessThreadTypeHandler* SandboxedProcessThreadTypeHandler::Get() {
  return g_sandboxed_process_thread_type_handler;
}

bool SandboxedProcessThreadTypeHandler::HandleThreadTypeChange(
    base::PlatformThreadId thread_id,
    base::ThreadType thread_type) {
  thread_type_switcher_->SetThreadType(thread_id, thread_type);
  return true;
}

void SandboxedProcessThreadTypeHandler::ConnectThreadTypeSwitcher() {
  ChildThreadImpl* main_thread = ChildThreadImpl::current();
  DCHECK(main_thread);
  DCHECK(main_thread->main_thread_runner()->RunsTasksInCurrentSequence());

  main_thread->BindHostReceiver(std::move(thread_type_switcher_receiver_));
}

}  // namespace content
