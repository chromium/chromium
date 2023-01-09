// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_HUNG_PLUGIN_FILTER_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_HUNG_PLUGIN_FILTER_H_

#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "content/common/pepper_plugin.mojom.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_sync_message_filter.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ppapi/proxy/host_dispatcher.h"

namespace content {

// This class monitors a renderer <-> pepper plugin channel on the I/O thread
// of the renderer for a hung plugin.
//
// If the plugin is not responding to sync messages, it will notify the browser
// process and give the user the option to kill the hung plugin.
//
// Note that this class must be threadsafe since it will get the begin/end
// block notifications on the main thread, but the filter is run on the I/O
// thread. This is important since when we're blocked on a sync message to a
// hung plugin, the main thread is frozen.
//
// NOTE: This class is refcounted (via IPC::MessageFilter).
class PepperHungPluginFilter
    : public ppapi::proxy::HostDispatcher::SyncMessageStatusObserver,
      public IPC::MessageFilter {
 public:
  PepperHungPluginFilter();

  PepperHungPluginFilter(const PepperHungPluginFilter&) = delete;
  PepperHungPluginFilter& operator=(const PepperHungPluginFilter&) = delete;

  // The |hung_host| is the mojo channel to inform the browser about the new
  // hung status.
  void BindHungDetectorHost(
      mojo::PendingRemote<mojom::PepperHungDetectorHost> hung_host);

  // SyncMessageStatusReceiver implementation.
  void BeginBlockOnSyncMessage() override;
  void EndBlockOnSyncMessage() override;

  // MessageFilter implementation.
  void OnFilterRemoved() override;
  void OnChannelError() override;
  bool OnMessageReceived(const IPC::Message& message) override;

  // Notification the HostDispatcher on the main thread has been destroyed.
  void HostDispatcherDestroyed();

 protected:
  ~PepperHungPluginFilter() override;

 private:
  // Binds the mojo channel on the IO thread (where it will be used).
  void BindHungDetectorHostOnIOThread(
      mojo::PendingRemote<mojom::PepperHungDetectorHost> hung_host);

  // Unbinds the mojo channel on the IO thread.
  void UnbindHungDetectorHostOnIOThread();

  // Makes sure that the hung timer is scheduled.
  void EnsureTimerScheduled();

  // Checks whether the plugin could have transitioned from hung to unhung and
  // notifies the browser if so.
  void MayHaveBecomeUnhung();

  // Calculate the point at which the plugin could next be considered hung.
  base::TimeTicks GetHungTime() const;

  // Checks if the plugin is considered hung based on whether it has been
  // blocked for long enough.
  bool IsHung() const;

  // Timer handler that checks for a hang after a timeout.
  void OnHangTimer();

  // Sends the hung/unhung message to the browser process.
  void SendHungMessage(bool is_hung);

  base::Lock lock_;

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // The channel back to the browser for informing it of the new status.
  // This remote is bound and used on the IO thread.
  mojo::Remote<mojom::PepperHungDetectorHost> hung_host_;

  // The time when we start being blocked on a sync message. If there are
  // nested ones, this is the time of the outermost one.
  //
  // This will be is_null() if we've never blocked.
  base::TimeTicks began_blocking_time_;

  // Time that the last message was received from the plugin.
  //
  // This will be is_null() if we've never received any messages.
  base::TimeTicks last_message_received_;

  // Number of nested sync messages that we're blocked on.
  int pending_sync_message_count_ = 0;

  // True when we've sent the "plugin is hung" message to the browser. We track
  // this so we know to look for it becoming unhung and send the corresponding
  // message to the browser.
  bool hung_plugin_showing_ = false;

  bool timer_task_pending_ = false;
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_HUNG_PLUGIN_FILTER_H_
