// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_host_queue.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
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
  auto it = base::ranges::find(queue_, host);
  if (it != queue_.end()) {
    queue_.erase(it);
  }
}

void ExtensionHostQueue::PostTask() {
  if (!pending_create_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ExtensionHostQueue::ProcessOneHost,
                       ptr_factory_.GetWeakPtr()),
        delay_);
    pending_create_ = true;
  }
}

void ExtensionHostQueue::ProcessOneHost() {
  pending_create_ = false;
  if (queue_.empty()) {
    return;  // can happen on shutdown
  }

  queue_.front()->CreateRendererNow();
  queue_.pop_front();

  if (!queue_.empty()) {
    PostTask();
  }
}

}  // namespace extensions
