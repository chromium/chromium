// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/browser_message_filter.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/debug/dump_without_crashing.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/process/process_handle.h"
#include "base/task/post_task.h"
#include "base/task_runner.h"
#include "build/build_config.h"
#include "content/browser/browser_child_process_host_impl.h"
#include "content/browser/child_process_launcher.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"
#include "ipc/ipc_sync_message.h"
#include "ipc/message_filter.h"

using content::BrowserMessageFilter;

namespace content {

class BrowserMessageFilter::Internal : public IPC::MessageFilter {
 public:
  explicit Internal(BrowserMessageFilter* filter) : filter_(filter) {}

 private:
  ~Internal() override {}

  // IPC::MessageFilter implementation:
  void OnFilterAdded(IPC::Channel* channel) override {
    filter_->sender_ = channel;
    filter_->OnFilterAdded(channel);
  }

  void OnFilterRemoved() override {
    for (auto& callback : filter_->filter_removed_callbacks_)
      std::move(callback).Run();
    filter_->filter_removed_callbacks_.clear();
    filter_->OnFilterRemoved();
  }

  void OnChannelClosing() override {
    filter_->sender_ = nullptr;
    filter_->OnChannelClosing();
  }

  void OnChannelError() override { filter_->OnChannelError(); }

  void OnChannelConnected(int32_t peer_pid) override {
    filter_->peer_process_ = base::Process::OpenWithExtraPrivileges(peer_pid);
    filter_->OnChannelConnected(peer_pid);
  }

  bool OnMessageReceived(const IPC::Message& message) override {
    BrowserThread::ID thread = BrowserThread::IO;
    filter_->OverrideThreadForMessage(message, &thread);

    if (thread == BrowserThread::IO) {
      scoped_refptr<base::SequencedTaskRunner> runner =
          filter_->OverrideTaskRunnerForMessage(message);
      if (runner.get()) {
        runner->PostTask(
            FROM_HERE,
            base::BindOnce(base::IgnoreResult(&Internal::DispatchMessage), this,
                           message));
        return true;
      }
      return DispatchMessage(message);
    }

    base::PostTask(
        FROM_HERE, {thread},
        base::BindOnce(base::IgnoreResult(&Internal::DispatchMessage), this,
                       message));
    return true;
  }

  bool GetSupportedMessageClasses(
      std::vector<uint32_t>* supported_message_classes) const override {
    supported_message_classes->assign(
        filter_->message_classes_to_filter().begin(),
        filter_->message_classes_to_filter().end());
    return true;
  }

  // Dispatches a message to the derived class.
  bool DispatchMessage(const IPC::Message& message) {
    bool rv = filter_->OnMessageReceived(message);
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO) || rv) <<
        "Must handle messages that were dispatched to another thread!";
    return rv;
  }

  scoped_refptr<BrowserMessageFilter> filter_;

  DISALLOW_COPY_AND_ASSIGN(Internal);
};

BrowserMessageFilter::BrowserMessageFilter(uint32_t message_class_to_filter)
    : internal_(nullptr),
      sender_(nullptr),
      message_classes_to_filter_(1, message_class_to_filter) {}

BrowserMessageFilter::BrowserMessageFilter(
    const uint32_t* message_classes_to_filter,
    size_t num_message_classes_to_filter)
    : internal_(nullptr),
      sender_(nullptr),
      message_classes_to_filter_(
          message_classes_to_filter,
          message_classes_to_filter + num_message_classes_to_filter) {
  DCHECK(num_message_classes_to_filter);
}

void BrowserMessageFilter::AddAssociatedInterface(
    const std::string& name,
    const IPC::ChannelProxy::GenericAssociatedInterfaceFactory& factory,
    base::OnceClosure filter_removed_callback) {
  associated_interfaces_.emplace_back(name, factory);
  filter_removed_callbacks_.emplace_back(std::move(filter_removed_callback));
}

base::ProcessHandle BrowserMessageFilter::PeerHandle() {
  return peer_process_.Handle();
}

void BrowserMessageFilter::OnDestruct() const {
  delete this;
}

bool BrowserMessageFilter::Send(IPC::Message* message) {
  if (message->is_sync()) {
    // We don't support sending synchronous messages from the browser.  If we
    // really needed it, we can make this class derive from SyncMessageFilter
    // but it seems better to not allow sending synchronous messages from the
    // browser, since it might allow a corrupt/malicious renderer to hang us.
    NOTREACHED() << "Can't send sync message through BrowserMessageFilter!";
    return false;
  }

  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    base::PostTask(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(base::IgnoreResult(&BrowserMessageFilter::Send), this,
                       message));
    return true;
  }

  if (sender_)
    return sender_->Send(message);

  delete message;
  return false;
}

scoped_refptr<base::SequencedTaskRunner>
BrowserMessageFilter::OverrideTaskRunnerForMessage(
    const IPC::Message& message) {
  return nullptr;
}

void BrowserMessageFilter::ShutdownForBadMessage() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDisableKillAfterBadIPC))
    return;

  if (base::Process::Current().Handle() == peer_process_.Handle()) {
    // Just crash in single process. Matches RenderProcessHostImpl behavior.
    CHECK(false);
  }

  ChildProcessLauncher::TerminateProcess(
      peer_process_, content::RESULT_CODE_KILLED_BAD_MESSAGE);

  // Report a crash, since none will be generated by the killed renderer.
  base::debug::DumpWithoutCrashing();

  // Log the renderer kill to the histogram tracking all kills.
  BrowserChildProcessHostImpl::HistogramBadMessageTerminated(
      PROCESS_TYPE_RENDERER);
}

BrowserMessageFilter::~BrowserMessageFilter() {
}

IPC::MessageFilter* BrowserMessageFilter::GetFilter() {
  // We create this on demand so that if a filter is used in a unit test but
  // never attached to a channel, we don't leak Internal and this;
  DCHECK(!internal_) << "Should only be called once.";
  internal_ = new Internal(this);
  return internal_;
}

void BrowserMessageFilter::RegisterAssociatedInterfaces(
    IPC::ChannelProxy* proxy) {
  for (const auto& entry : associated_interfaces_)
    proxy->AddGenericAssociatedInterfaceForIOThread(entry.first, entry.second);
  associated_interfaces_.clear();
}

}  // namespace content
