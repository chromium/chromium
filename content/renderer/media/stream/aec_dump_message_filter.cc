// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/stream/aec_dump_message_filter.h"

#include "base/single_thread_task_runner.h"
#include "content/common/media/aec_dump_messages.h"
#include "content/renderer/media/webrtc_logging.h"
#include "ipc/ipc_logging.h"
#include "ipc/ipc_sender.h"

namespace {
const int kInvalidDelegateId = -1;
}

namespace content {

AecDumpMessageFilter* AecDumpMessageFilter::g_filter = nullptr;

AecDumpMessageFilter::AecDumpMessageFilter(
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner)
    : sender_(nullptr),
      delegate_id_counter_(1),
      io_task_runner_(io_task_runner),
      main_task_runner_(main_task_runner) {
  DCHECK(!g_filter);
  g_filter = this;
}

AecDumpMessageFilter::~AecDumpMessageFilter() {
  DCHECK_EQ(g_filter, this);
  g_filter = nullptr;
}

// static
scoped_refptr<AecDumpMessageFilter> AecDumpMessageFilter::Get() {
  return g_filter;
}

void AecDumpMessageFilter::AddDelegate(
    AecDumpMessageFilter::AecDumpDelegate* delegate) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK(delegate);
  DCHECK_EQ(kInvalidDelegateId, GetIdForDelegate(delegate));

  int id = delegate_id_counter_++;
  delegates_[id] = delegate;

  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&AecDumpMessageFilter::RegisterAecDumpConsumer, this, id));
}

void AecDumpMessageFilter::RemoveDelegate(
    AecDumpMessageFilter::AecDumpDelegate* delegate) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK(delegate);

  int id = GetIdForDelegate(delegate);
  DCHECK_NE(kInvalidDelegateId, id);
  delegates_.erase(id);

  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&AecDumpMessageFilter::UnregisterAecDumpConsumer, this,
                     id));
}

base::Optional<bool> AecDumpMessageFilter::GetOverrideAec3() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  return override_aec3_;
}

void AecDumpMessageFilter::Send(IPC::Message* message) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  if (sender_)
    sender_->Send(message);
  else
    delete message;
}

void AecDumpMessageFilter::RegisterAecDumpConsumer(int id) {
  Send(new AecDumpMsg_RegisterAecDumpConsumer(id));
}

void AecDumpMessageFilter::UnregisterAecDumpConsumer(int id) {
  Send(new AecDumpMsg_UnregisterAecDumpConsumer(id));
}

bool AecDumpMessageFilter::OnMessageReceived(const IPC::Message& message) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(AecDumpMessageFilter, message)
    IPC_MESSAGE_HANDLER(AecDumpMsg_EnableAecDump, OnEnableAecDump)
    IPC_MESSAGE_HANDLER(AecDumpMsg_DisableAecDump, OnDisableAecDump)
    IPC_MESSAGE_HANDLER(AudioProcessingMsg_EnableAec3, OnEnableAec3)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void AecDumpMessageFilter::OnFilterAdded(IPC::Channel* channel) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  sender_ = channel;
}

void AecDumpMessageFilter::OnFilterRemoved() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  // Once removed, a filter will not be used again.  At this time the
  // observer must be notified so it releases its reference.
  OnChannelClosing();
}

void AecDumpMessageFilter::OnChannelClosing() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  sender_ = nullptr;
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&AecDumpMessageFilter::DoChannelClosingOnDelegates, this));
}

void AecDumpMessageFilter::OnEnableAecDump(
    int id,
    IPC::PlatformFileForTransit file_handle) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&AecDumpMessageFilter::DoEnableAecDump, this,
                                id, file_handle));
}

void AecDumpMessageFilter::OnDisableAecDump() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&AecDumpMessageFilter::DoDisableAecDump, this));
}

void AecDumpMessageFilter::OnEnableAec3(bool enable) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&AecDumpMessageFilter::DoEnableAec3, this, enable));
}

void AecDumpMessageFilter::DoEnableAecDump(
    int id,
    IPC::PlatformFileForTransit file_handle) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  auto it = delegates_.find(id);
  if (it != delegates_.end()) {
    it->second->OnAecDumpFile(file_handle);
  } else {
    // Delegate has been removed, we must close the file.
    base::File file = IPC::PlatformFileForTransitToFile(file_handle);
    DCHECK(file.IsValid());
    file.Close();
  }
}

void AecDumpMessageFilter::DoDisableAecDump() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  for (auto it = delegates_.begin(); it != delegates_.end(); ++it) {
    it->second->OnDisableAecDump();
  }
}

void AecDumpMessageFilter::DoChannelClosingOnDelegates() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  for (auto it = delegates_.begin(); it != delegates_.end(); ++it) {
    it->second->OnIpcClosing();
  }
  delegates_.clear();
}

int AecDumpMessageFilter::GetIdForDelegate(
    AecDumpMessageFilter::AecDumpDelegate* delegate) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  for (auto it = delegates_.begin(); it != delegates_.end(); ++it) {
    if (it->second == delegate)
      return it->first;
  }
  return kInvalidDelegateId;
}

void AecDumpMessageFilter::DoEnableAec3(bool enable) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  override_aec3_ = enable;
  io_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&AecDumpMessageFilter::Send, this,
                                new AudioProcessingMsg_Aec3Enabled()));
}

}  // namespace content
