// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PPAPI_PLUGIN_PROCESS_HOST_H_
#define CONTENT_BROWSER_PPAPI_PLUGIN_PROCESS_HOST_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/queue.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/process/process.h"
#include "content/browser/renderer_host/pepper/browser_ppapi_host_impl.h"
#include "content/public/browser/browser_child_process_host_delegate.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "ipc/ipc_sender.h"
#include "ppapi/shared_impl/ppapi_permissions.h"
#include "url/origin.h"

namespace content {
class BrowserChildProcessHostImpl;
struct ContentPluginInfo;

// Process host for PPAPI plugin processes.
class PpapiPluginProcessHost : public BrowserChildProcessHostDelegate,
                               public IPC::Sender {
 public:
  class Client {
   public:
    // Gets the information about the renderer that's requesting the channel.
    // If |renderer_handle| is base::kNullProcessHandle, this channel is used by
    // the browser itself.
    virtual void GetPpapiChannelInfo(base::ProcessHandle* renderer_handle,
                                     int* renderer_id) = 0;

    // Called when the channel is asynchronously opened to the plugin or on
    // error. On error, the parameters should be:
    //   base::kNullProcessHandle
    //   IPC::ChannelHandle(),
    //   0
    virtual void OnPpapiChannelOpened(
        const IPC::ChannelHandle& channel_handle,
        base::ProcessId plugin_pid,
        int plugin_child_id) = 0;

    // Returns true if the current connection is incognito.
    virtual bool Incognito() = 0;

   protected:
    virtual ~Client() {}
  };

  class PluginClient : public Client {
   protected:
    ~PluginClient() override {}
  };

  PpapiPluginProcessHost(const PpapiPluginProcessHost&) = delete;
  PpapiPluginProcessHost& operator=(const PpapiPluginProcessHost&) = delete;

  ~PpapiPluginProcessHost() override;

  static PpapiPluginProcessHost* CreatePluginHost(
      const ContentPluginInfo& info,
      const base::FilePath& profile_data_directory,
      const std::optional<url::Origin>& origin_lock);

  // Notification that a PP_Instance has been created and the associated
  // renderer related data including the RenderFrame/Process pair for the given
  // plugin. This is necessary so that when the plugin calls us with a
  // PP_Instance we can find the `RenderFrame` associated with it without
  // trusting the plugin.
  static void DidCreateOutOfProcessInstance(
      int plugin_process_id,
      int32_t pp_instance,
      const PepperRendererInstanceData& instance_data);

  // The opposite of DIdCreate... above.
  static void DidDeleteOutOfProcessInstance(int plugin_process_id,
                                            int32_t pp_instance);

  // Returns the instances that match the specified process name.
  // It can only be called on the IO thread.
  static void FindByName(const std::u16string& name,
                         std::vector<PpapiPluginProcessHost*>* hosts);

  // IPC::Sender implementation:
  bool Send(IPC::Message* message) override;

  // Opens a new channel to the plugin. The client will be notified when the
  // channel is ready or if there's an error.
  void OpenChannelToPlugin(Client* client);

  BrowserPpapiHostImpl* host_impl() { return host_impl_.get(); }
  BrowserChildProcessHostImpl* process() { return process_.get(); }
  const std::optional<url::Origin>& origin_lock() const { return origin_lock_; }
  const base::FilePath& plugin_path() const { return plugin_path_; }
  const base::FilePath& profile_data_directory() const {
    return profile_data_directory_;
  }

  // The client pointer must remain valid until its callback is issued.

 private:
  class PluginNetworkObserver;

  // Constructors for plugin process hosts.
  // You must call Init before doing anything else.
  PpapiPluginProcessHost(const ContentPluginInfo& info,
                         const base::FilePath& profile_data_directory,
                         const std::optional<url::Origin>& origin_lock);

  // Actually launches the process with the given plugin info. Returns true
  // on success (the process was spawned).
  bool Init(const ContentPluginInfo& info);

  void RequestPluginChannel(Client* client);

  void OnProcessLaunched() override;
  void OnProcessCrashed(int exit_code) override;
  void BindHostReceiver(mojo::GenericPendingReceiver receiver) override;
  bool OnMessageReceived(const IPC::Message& msg) override;
  void OnChannelConnected(int32_t peer_pid) override;
  void OnChannelError() override;

  void CancelRequests();

  // IPC message handlers.
  void OnRendererPluginChannelCreated(const IPC::ChannelHandle& handle);

  ppapi::PpapiPermissions permissions_;
  std::unique_ptr<BrowserPpapiHostImpl> host_impl_;

  // Observes network changes. May be NULL.
  std::unique_ptr<PluginNetworkObserver> network_observer_;

  // Channel requests that we are waiting to send to the plugin process once
  // the channel is opened.
  std::vector<raw_ptr<Client, VectorExperimental>> pending_requests_;

  // Channel requests that we have already sent to the plugin process, but
  // haven't heard back about yet.
  base::queue<raw_ptr<Client, CtnExperimental>> sent_requests_;

  // Path to the plugin library.
  base::FilePath plugin_path_;

  // Path to the top-level plugin data directory (differs based upon profile).
  const base::FilePath profile_data_directory_;

  // Specific origin to which this is bound, omitted to allow any origin to
  // re-use the plugin host.
  const std::optional<url::Origin> origin_lock_;

  std::unique_ptr<BrowserChildProcessHostImpl> process_;
};

class PpapiPluginProcessHostIterator
    : public BrowserChildProcessHostTypeIterator<
          PpapiPluginProcessHost> {
 public:
  PpapiPluginProcessHostIterator()
      : BrowserChildProcessHostTypeIterator<
          PpapiPluginProcessHost>(PROCESS_TYPE_PPAPI_PLUGIN) {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_PPAPI_PLUGIN_PROCESS_HOST_H_
