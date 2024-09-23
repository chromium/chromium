// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BROWSER_MESSAGE_FILTER_H_
#define CONTENT_PUBLIC_BROWSER_BROWSER_MESSAGE_FILTER_H_

#include <stddef.h>
#include <stdint.h>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/process/process.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "ipc/ipc_channel_proxy.h"

#if BUILDFLAG(IS_WIN)
#include "base/synchronization/lock.h"
#endif

namespace IPC {
class MessageFilter;
}

namespace content {
struct BrowserMessageFilterTraits;

// Base class for message filters in the browser process.  You can receive and
// send messages on any thread.
//
// BrowserMessageFilters are ref-counted, and a reference to them is held by the
// IPC ChannelProxy for which they have been installed, so do not rely on the
// destructor being called on a specific sequence unless you specify otherwise
// in OnDestruct().
class CONTENT_EXPORT BrowserMessageFilter
    : public base::RefCountedThreadSafe<
          BrowserMessageFilter, BrowserMessageFilterTraits>,
      public IPC::Sender {
 public:
  explicit BrowserMessageFilter(uint32_t message_class_to_filter);
  BrowserMessageFilter(const uint32_t* message_classes_to_filter,
                       size_t num_message_classes_to_filter);

  // These match the corresponding IPC::MessageFilter methods and are always
  // called on the IO thread.
  virtual void OnFilterAdded(IPC::Channel* channel) {}
  virtual void OnFilterRemoved() {}
  virtual void OnChannelClosing() {}
  virtual void OnChannelError() {}
  virtual void OnChannelConnected(int32_t peer_pid) {}

  // Called when the message filter is about to be deleted.  This gives
  // derived classes the option of controlling which thread they're deleted
  // on etc.
  virtual void OnDestruct() const;

  // IPC::Sender implementation.  Can be called on any thread.  Can't send sync
  // messages (since we don't want to block the browser on any other process).
  bool Send(IPC::Message* message) override;

  // If you want the given message to be dispatched to your OnMessageReceived on
  // a different thread, there are two options, either
  // OverrideThreadForMessage or OverrideTaskRunnerForMessage.
  // If neither is overriden, the message will be dispatched on the IO thread.

  // If you want the message to be dispatched on a particular well-known
  // browser thread, change |thread| to the id of the target thread
  virtual void OverrideThreadForMessage(
      const IPC::Message& message,
      BrowserThread::ID* thread) {}

  // If you want the message to be dispatched via the SequencedWorkerPool,
  // return a non-null task runner which will target tasks accordingly.
  // Note: To target the UI thread, please use OverrideThreadForMessage
  // since that has extra checks to avoid deadlocks.
  virtual scoped_refptr<base::SequencedTaskRunner> OverrideTaskRunnerForMessage(
      const IPC::Message& message);

  // Override this to receive messages.
  // Your function will normally be called on the IO thread.  However, if your
  // OverrideXForMessage modifies the thread used to dispatch the message,
  // your function will be called on the requested thread.
  virtual bool OnMessageReceived(const IPC::Message& message) = 0;

  // Can be called on any thread, after OnChannelConnected is called.
  base::ProcessHandle PeerHandle();

  // Can be called on any thread, after OnChannelConnected is called.
  base::ProcessId peer_pid() const { return peer_process_.Pid(); }

  void set_peer_process_for_testing(base::Process peer_process) {
    peer_process_ = std::move(peer_process);
  }

  // Called by bad_message.h helpers if a message couldn't be deserialized. This
  // kills the renderer.  Can be called on any thread.  This doesn't log the
  // error details to UMA, so use the bad_message.h for your module instead.
  virtual void ShutdownForBadMessage();

  const std::vector<uint32_t>& message_classes_to_filter() const {
    return message_classes_to_filter_;
  }

 protected:
  ~BrowserMessageFilter() override;

 private:
  friend class base::RefCountedThreadSafe<BrowserMessageFilter,
                                          BrowserMessageFilterTraits>;

  class Internal;
  friend class AgentSchedulingGroupHost;
  friend class BrowserChildProcessHostImpl;
  friend class BrowserPpapiHost;
  friend class RenderProcessHostImpl;

  // These are private because the only classes that need access to them are
  // made friends above. These are only guaranteed to be valid to call on
  // creation. After that this class could outlive the filter.
  IPC::MessageFilter* GetFilter();

  // This implements IPC::MessageFilter so that we can hide that from child
  // classes. Internal keeps a reference to this class, which is why there's a
  // weak pointer back. This class could outlive Internal based on what the
  // child class does in its OnDestruct method.
  raw_ptr<Internal, AcrossTasksDanglingUntriaged> internal_ = nullptr;

  raw_ptr<IPC::Sender> sender_ = nullptr;
  base::Process peer_process_;

  std::vector<uint32_t> message_classes_to_filter_;

  // Callbacks to be called in OnFilterRemoved().
  std::vector<base::OnceClosure> filter_removed_callbacks_;
};

struct BrowserMessageFilterTraits {
  static void Destruct(const BrowserMessageFilter* filter) {
    filter->OnDestruct();
  }
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BROWSER_MESSAGE_FILTER_H_
