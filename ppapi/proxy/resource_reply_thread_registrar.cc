// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/resource_reply_thread_registrar.h"

#include "base/check.h"
#include "base/task/single_thread_task_runner.h"
#include "ipc/ipc_message.h"
#include "ppapi/proxy/resource_message_params.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/shared_impl/tracked_callback.h"

namespace ppapi {
namespace proxy {

ResourceReplyThreadRegistrar::ResourceReplyThreadRegistrar(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread)
    : main_thread_(main_thread) {
}

ResourceReplyThreadRegistrar::~ResourceReplyThreadRegistrar() {
}

void ResourceReplyThreadRegistrar::Register(
    PP_Resource resource,
    int32_t sequence_number,
    scoped_refptr<TrackedCallback> reply_thread_hint) {
  ProxyLock::AssertAcquiredDebugOnly();

  // Use the main thread if |reply_thread_hint| is NULL or blocking.
  if (!reply_thread_hint.get() || reply_thread_hint->is_blocking())
    return;

  DCHECK(reply_thread_hint->target_loop());
  scoped_refptr<base::SingleThreadTaskRunner> reply_thread(
      reply_thread_hint->target_loop()->GetTaskRunner());
  {
    base::AutoLock auto_lock(lock_);

    if (reply_thread.get() == main_thread_.get())
      return;

    map_[resource][sequence_number] = reply_thread;
  }
}

void ResourceReplyThreadRegistrar::Unregister(PP_Resource resource) {
  base::AutoLock auto_lock(lock_);
  map_.erase(resource);
}

scoped_refptr<base::SingleThreadTaskRunner>
ResourceReplyThreadRegistrar::GetTargetThread(
    const ResourceMessageReplyParams& reply_params,
    const IPC::Message& nested_msg) {
  base::AutoLock auto_lock(lock_);
  ResourceMap::iterator resource_iter = map_.find(reply_params.pp_resource());
  if (resource_iter != map_.end()) {
    SequenceThreadMap::iterator sequence_thread_iter =
        resource_iter->second.find(reply_params.sequence());
    if (sequence_thread_iter != resource_iter->second.end()) {
      scoped_refptr<base::SingleThreadTaskRunner> target =
          sequence_thread_iter->second;
      resource_iter->second.erase(sequence_thread_iter);
      return target;
    }
  }

  return main_thread_;
}

}  // namespace proxy
}  // namespace ppapi
