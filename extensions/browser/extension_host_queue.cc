// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_host_queue.h"

#include <algorithm>

#include "base/bind.h"
#include "base/location.h"
#include "base/no_destructor.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "extensions/browser/deferred_start_render_host.h"

namespace extensions {

ExtensionHostQueue::ExtensionHostQueue() : pending_create_(false) {}

ExtensionHostQueue::~ExtensionHostQueue() = default;

// static
ExtensionHostQueue& ExtensionHostQueue::GetInstance() {
  static base::NoDestructor<ExtensionHostQueue> queue;
  return *queue;
}

void ExtensionHostQueue::Add(DeferredStartRenderHost* host) {
  queue_.push_back(host);
  PostTask();
}

void ExtensionHostQueue::Remove(DeferredStartRenderHost* host) {
  auto it = std::find(queue_.begin(), queue_.end(), host);
  if (it != queue_.end())
    queue_.erase(it);
}

void ExtensionHostQueue::PostTask() {
  if (!pending_create_) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ExtensionHostQueue::ProcessOneHost,
                       ptr_factory_.GetWeakPtr()),
        delay_);
    pending_create_ = true;
  }
}

void ExtensionHostQueue::ProcessOneHost() {
  pending_create_ = false;
  if (queue_.empty())
    return;  // can happen on shutdown

  queue_.front()->CreateRendererNow();
  queue_.pop_front();

  if (!queue_.empty())
    PostTask();
}

}  // namespace extensions
