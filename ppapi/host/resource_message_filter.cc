// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/host/resource_message_filter.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ipc/ipc_message.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/host/resource_host.h"

namespace ppapi {
namespace host {

namespace internal {

// static
void ResourceMessageFilterDeleteTraits::Destruct(
    const ResourceMessageFilter* filter) {
  if (!filter->deletion_task_runner_->BelongsToCurrentThread()) {
    // During shutdown the object may not be deleted, but it should be okay to
    // leak in that case.
    filter->deletion_task_runner_->DeleteSoon(FROM_HERE, filter);
  } else {
    delete filter;
  }
}

}  // namespace internal

ResourceMessageFilter::ResourceMessageFilter()
    : deletion_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      reply_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      resource_host_(nullptr) {}

ResourceMessageFilter::ResourceMessageFilter(
    scoped_refptr<base::SingleThreadTaskRunner> reply_thread_task_runner)
    : deletion_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      reply_thread_task_runner_(reply_thread_task_runner),
      resource_host_(nullptr) {}

ResourceMessageFilter::~ResourceMessageFilter() {
}

void ResourceMessageFilter::OnFilterAdded(ResourceHost* resource_host) {
  resource_host_ = resource_host;
}

void ResourceMessageFilter::OnFilterDestroyed() {
  resource_host_ = nullptr;
}

bool ResourceMessageFilter::HandleMessage(const IPC::Message& msg,
                                          HostMessageContext* context) {
  scoped_refptr<base::TaskRunner> runner = OverrideTaskRunnerForMessage(msg);
  if (runner.get()) {
    if (runner->RunsTasksInCurrentSequence()) {
      DispatchMessage(msg, *context);
    } else {
      // TODO(raymes): We need to make a copy so the context can be used on
      // other threads. It would be better to have a thread-safe refcounted
      // context.
      HostMessageContext context_copy = *context;
      runner->PostTask(FROM_HERE,
                       base::BindOnce(&ResourceMessageFilter::DispatchMessage,
                                      this, msg, context_copy));
    }
    return true;
  }

  return false;
}

void ResourceMessageFilter::SendReply(const ReplyMessageContext& context,
                                      const IPC::Message& msg) {
  if (!reply_thread_task_runner_->BelongsToCurrentThread()) {
    reply_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ResourceMessageFilter::SendReply, this, context, msg));
    return;
  }
  if (resource_host_)
    resource_host_->SendReply(context, msg);
}

scoped_refptr<base::TaskRunner>
ResourceMessageFilter::OverrideTaskRunnerForMessage(const IPC::Message& msg) {
  return nullptr;
}

void ResourceMessageFilter::DispatchMessage(const IPC::Message& msg,
                                            HostMessageContext context) {
  RunMessageHandlerAndReply(msg, &context);
}

}  // namespace host
}  // namespace ppapi
