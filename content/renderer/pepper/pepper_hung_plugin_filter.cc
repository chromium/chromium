// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_hung_plugin_filter.h"

#include "base/functional/bind.h"
#include "content/child/child_process.h"

namespace content {

namespace {

// We'll consider the plugin hung after not hearing anything for this long.
const int kHungThresholdSec = 10;

// If we ever are blocked for this long, we'll consider the plugin hung, even
// if we continue to get messages (which is why the above hung threshold never
// kicked in). Maybe the plugin is spamming us with events and never unblocking
// and never processing our sync message.
const int kBlockedHardThresholdSec = kHungThresholdSec * 1.5;

}  // namespace

PepperHungPluginFilter::PepperHungPluginFilter()
    : io_task_runner_(ChildProcess::current()->io_task_runner()) {}

void PepperHungPluginFilter::BindHungDetectorHost(
    mojo::PendingRemote<mojom::PepperHungDetectorHost> hung_host) {
  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PepperHungPluginFilter::BindHungDetectorHostOnIOThread,
                     this, std::move(hung_host)));
}

void PepperHungPluginFilter::UnbindHungDetectorHostOnIOThread() {
  hung_host_.reset();
}

void PepperHungPluginFilter::BindHungDetectorHostOnIOThread(
    mojo::PendingRemote<mojom::PepperHungDetectorHost> hung_host) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  hung_host_.Bind(std::move(hung_host));
}

void PepperHungPluginFilter::BeginBlockOnSyncMessage() {
  base::AutoLock lock(lock_);
  last_message_received_ = base::TimeTicks::Now();
  if (pending_sync_message_count_ == 0)
    began_blocking_time_ = last_message_received_;
  pending_sync_message_count_++;

  EnsureTimerScheduled();
}

void PepperHungPluginFilter::EndBlockOnSyncMessage() {
  base::AutoLock lock(lock_);
  pending_sync_message_count_--;
  DCHECK(pending_sync_message_count_ >= 0);

  MayHaveBecomeUnhung();
}

void PepperHungPluginFilter::OnFilterRemoved() {
  base::AutoLock lock(lock_);
  MayHaveBecomeUnhung();
}

void PepperHungPluginFilter::OnChannelError() {
  base::AutoLock lock(lock_);
  MayHaveBecomeUnhung();
}

bool PepperHungPluginFilter::OnMessageReceived(const IPC::Message& message) {
  // Just track incoming message times but don't handle any messages.
  base::AutoLock lock(lock_);
  last_message_received_ = base::TimeTicks::Now();
  MayHaveBecomeUnhung();
  return false;
}

PepperHungPluginFilter::~PepperHungPluginFilter() {}

void PepperHungPluginFilter::HostDispatcherDestroyed() {
  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PepperHungPluginFilter::UnbindHungDetectorHostOnIOThread,
                     this));
}

void PepperHungPluginFilter::EnsureTimerScheduled() {
  lock_.AssertAcquired();
  if (timer_task_pending_)
    return;

  timer_task_pending_ = true;
  io_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&PepperHungPluginFilter::OnHangTimer, this),
      base::Seconds(kHungThresholdSec));
}

void PepperHungPluginFilter::MayHaveBecomeUnhung() {
  lock_.AssertAcquired();
  if (!hung_plugin_showing_ || IsHung())
    return;

  SendHungMessage(false);
  hung_plugin_showing_ = false;
}

base::TimeTicks PepperHungPluginFilter::GetHungTime() const {
  lock_.AssertAcquired();

  DCHECK(pending_sync_message_count_);
  DCHECK(!began_blocking_time_.is_null());
  DCHECK(!last_message_received_.is_null());

  // Always considered hung at the hard threshold.
  base::TimeTicks hard_time =
      began_blocking_time_ + base::Seconds(kBlockedHardThresholdSec);

  // Hung after a soft threshold from last message of any sort.
  base::TimeTicks soft_time =
      last_message_received_ + base::Seconds(kHungThresholdSec);

  return std::min(soft_time, hard_time);
}

bool PepperHungPluginFilter::IsHung() const {
  lock_.AssertAcquired();

  if (!pending_sync_message_count_)
    return false;  // Not blocked on a sync message.

  return base::TimeTicks::Now() > GetHungTime();
}

void PepperHungPluginFilter::OnHangTimer() {
  base::AutoLock lock(lock_);
  timer_task_pending_ = false;

  if (!pending_sync_message_count_)
    return;  // Not blocked any longer.

  base::TimeDelta delay = GetHungTime() - base::TimeTicks::Now();
  if (delay.is_positive()) {
    // Got a timer message while we're waiting on a sync message. We need
    // to schedule another timer message because the latest sync message
    // would not have scheduled one (we only have one out-standing timer at
    // a time).
    timer_task_pending_ = true;
    io_task_runner_->PostDelayedTask(
        FROM_HERE, base::BindOnce(&PepperHungPluginFilter::OnHangTimer, this),
        delay);
    return;
  }

  hung_plugin_showing_ = true;
  SendHungMessage(true);
}

void PepperHungPluginFilter::SendHungMessage(bool is_hung) {
  if (!io_task_runner_->BelongsToCurrentThread()) {
    io_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&PepperHungPluginFilter::SendHungMessage,
                                  this, is_hung));
    return;
  }

  hung_host_->PluginHung(is_hung);
}

}  // namespace content
