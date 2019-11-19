// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/serial_extension_host_queue.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/variations/variations_associated_data.h"
#include "extensions/browser/deferred_start_render_host.h"

namespace extensions {

namespace {

// Gets the number of milliseconds to delay between loading ExtensionHosts. By
// default this is 0, but it can be overridden by field trials.
int GetDelayMs() {
  // A sanity check for the maximum delay, to guard against a bad field trial
  // config being pushed that delays loading too much (e.g. using wrong units).
  static const int kMaxDelayMs = 30 * 1000;
  static int delay_ms = -1;
  if (delay_ms == -1) {
    std::string delay_ms_param =
        variations::GetVariationParamValue("ExtensionSpeed", "SerialEHQDelay");
    if (delay_ms_param.empty()) {
      delay_ms = 0;
    } else if (!base::StringToInt(delay_ms_param, &delay_ms)) {
      LOG(ERROR) << "Could not parse SerialEHQDelay: " << delay_ms_param;
      delay_ms = 0;
    } else if (delay_ms < 0 || delay_ms > kMaxDelayMs) {
      LOG(ERROR) << "SerialEHQDelay out of range: " << delay_ms;
      delay_ms = 0;
    }
  }
  return delay_ms;
}

}  // namespace

SerialExtensionHostQueue::SerialExtensionHostQueue() : pending_create_(false) {}

SerialExtensionHostQueue::~SerialExtensionHostQueue() {
}

void SerialExtensionHostQueue::Add(DeferredStartRenderHost* host) {
  queue_.push_back(host);
  PostTask();
}

void SerialExtensionHostQueue::Remove(DeferredStartRenderHost* host) {
  auto it = std::find(queue_.begin(), queue_.end(), host);
  if (it != queue_.end())
    queue_.erase(it);
}

void SerialExtensionHostQueue::PostTask() {
  if (!pending_create_) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&SerialExtensionHostQueue::ProcessOneHost,
                       ptr_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(GetDelayMs()));
    pending_create_ = true;
  }
}

void SerialExtensionHostQueue::ProcessOneHost() {
  pending_create_ = false;
  if (queue_.empty())
    return;  // can happen on shutdown

  queue_.front()->CreateRenderViewNow();
  queue_.pop_front();

  if (!queue_.empty())
    PostTask();
}

}  // namespace extensions
