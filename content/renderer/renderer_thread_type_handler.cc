// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/renderer_thread_type_handler.h"

#include "base/bind.h"
#include "content/renderer/render_thread_impl.h"

namespace content {

namespace {

RendererThreadTypeHandler* g_renderer_thread_type_handler = nullptr;

}  // namespace

RendererThreadTypeHandler::RendererThreadTypeHandler()
    : main_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {
  base::PlatformThread::SetThreadTypeDelegate(this);
}

RendererThreadTypeHandler::~RendererThreadTypeHandler() {
  base::PlatformThread::SetThreadTypeDelegate(nullptr);
}

// static
void RendererThreadTypeHandler::Create() {
  DCHECK(!g_renderer_thread_type_handler);

  g_renderer_thread_type_handler = new RendererThreadTypeHandler();
}

// static
void RendererThreadTypeHandler::NotifyRenderThreadCreated() {
  if (g_renderer_thread_type_handler) {
    g_renderer_thread_type_handler->ProcessPendingChangeRequests();
  }
}

bool RendererThreadTypeHandler::HandleThreadTypeChange(
    base::PlatformThreadId thread_id,
    base::ThreadType thread_type) {
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&RendererThreadTypeHandler::ProcessCurrentChangeRequest,
                     base::Unretained(this), thread_id, thread_type));
  return true;
}

void RendererThreadTypeHandler::ProcessCurrentChangeRequest(
    base::PlatformThreadId tid,
    base::ThreadType type) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  if (RenderThreadImpl* render_thread = RenderThreadImpl::current()) {
    render_thread->render_message_filter()->SetThreadType(tid, type);
  } else {
    thread_id_to_type_[tid] = type;
  }
}

void RendererThreadTypeHandler::ProcessPendingChangeRequests() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  RenderThreadImpl* render_thread = RenderThreadImpl::current();
  DCHECK(render_thread);

  for (const auto& iter : thread_id_to_type_) {
    render_thread->render_message_filter()->SetThreadType(iter.first,
                                                          iter.second);
  }
  thread_id_to_type_.clear();
}

}  // namespace content
