// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PPAPI_PLUGIN_PROCESS_HOST_H_
#define CONTENT_BROWSER_PPAPI_PLUGIN_PROCESS_HOST_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/containers/queue.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/process/process.h"
#include "base/strings/string16.h"
#include "content/browser/renderer_host/pepper/browser_ppapi_host_impl.h"
#include "content/browser/renderer_host/pepper/pepper_message_filter.h"
#include "content/public/browser/browser_child_process_host_delegate.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "ipc/ipc_sender.h"
#include "ppapi/shared_impl/ppapi_permissions.h"
#include "url/origin.h"

namespace content {
class BrowserChildProcessHostImpl;
struct PepperPluginInfo;

// Process host for PPAPI plugin and broker processes.
// When used for the broker, interpret all references to "plugin" with "broker".
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

  class BrokerClient : public Client {
   protected:
    ~BrokerClient() override {}
  };

  ~PpapiPluginProcessHost() override;

  static PpapiPluginProcessHost* CreatePluginHost(
      const PepperPluginInfo& info,
      const base::FilePath& profile_data_directory,
      const base::Optional<url::Origin>& origin_lock);

  static PpapiPluginProcessHost* CreateBrokerHost(
      const PepperPluginInfo& info);

  // Notification that a PP_Instance has been created and the associated
  // renderer related data including the RenderView/Process pair for the given
  // plugin. This is necessary so that when the plugin calls us with a
  // PP_Instance we can find the RenderView associated with it without trusting
  // the plugin.
  static void DidCreateOutOfProcessInstance(
      int plugin_process_id,
      int32_t pp_instance,
      const PepperRendererInstanceData& instance_data);

  // The opposite of DIdCreate... above.
  static void DidDeleteOutOfProcessInstance(int plugin_process_id,
                                            int32_t pp_instance);

  // Notification that a Plugin instance has been throttled or unthrottled.
  static void OnPluginInstanceThrottleStateChange(int plugin_process_id,
                                                  int32_t pp_instance,
                                                  bool is_throttled);

  // Returns the instances that match the specified process name.
  // It can only be called on the IO thread.
  static void FindByName(const base::string16& name,
                         std::vector<PpapiPluginProcessHost*>* hosts);

  // IPC::Sender implementation:
  bool Send(IPC::Message* message) override;

  // Opens a new channel to the plugin. The client will be notified when the
  // channel is ready or if there's an error.
  void OpenChannelToPlugin(Client* client);

  BrowserPpapiHostImpl* host_impl() { return host_impl_.get(); }
  BrowserChildProcessHostImpl* process() { return process_.get(); }
  const base::Optional<url::Origin>& origin_lock() const {
    return origin_lock_;
  }
  const base::FilePath& plugin_path() const { return plugin_path_; }
  const base::FilePath& profile_data_directory() const {
    return profile_data_directory_;
  }

  // The client pointer must remain valid until its callback is issued.

 private:
  class PluginNetworkObserver;

  // Constructors for plugin and broker process hosts, respectively.
  // You must call Init before doing anything else.
  PpapiPluginProcessHost(const PepperPluginInfo& info,
                         const base::FilePath& profile_data_directory,
                         const base::Optional<url::Origin>& origin_lock);
  PpapiPluginProcessHost();

  // Actually launches the process with the given plugin info. Returns true
  // on success (the process was spawned).
  bool Init(const PepperPluginInfo& info);

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

  // Handles most requests from the plugin. May be NULL.
  scoped_refptr<PepperMessageFilter> filter_;

  ppapi::PpapiPermissions permissions_;
  std::unique_ptr<BrowserPpapiHostImpl> host_impl_;

  // Observes network changes. May be NULL.
  std::unique_ptr<PluginNetworkObserver> network_observer_;

  // Channel requests that we are waiting to send to the plugin process once
  // the channel is opened.
  std::vector<Client*> pending_requests_;

  // Channel requests that we have already sent to the plugin process, but
  // haven't heard back about yet.
  base::queue<Client*> sent_requests_;

  // Path to the plugin library.
  base::FilePath plugin_path_;

  // Path to the top-level plugin data directory (differs based upon profile).
  const base::FilePath profile_data_directory_;

  // Specific origin to which this is bound, omitted to allow any origin to
  // re-use the plugin host.
  const base::Optional<url::Origin> origin_lock_;

  const bool is_broker_;

  std::unique_ptr<BrowserChildProcessHostImpl> process_;

  DISALLOW_COPY_AND_ASSIGN(PpapiPluginProcessHost);
};

class PpapiPluginProcessHostIterator
    : public BrowserChildProcessHostTypeIterator<
          PpapiPluginProcessHost> {
 public:
  PpapiPluginProcessHostIterator()
      : BrowserChildProcessHostTypeIterator<
          PpapiPluginProcessHost>(PROCESS_TYPE_PPAPI_PLUGIN) {}
};

class PpapiBrokerProcessHostIterator
    : public BrowserChildProcessHostTypeIterator<
          PpapiPluginProcessHost> {
 public:
  PpapiBrokerProcessHostIterator()
      : BrowserChildProcessHostTypeIterator<
          PpapiPluginProcessHost>(PROCESS_TYPE_PPAPI_BROKER) {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_PPAPI_PLUGIN_PROCESS_HOST_H_

