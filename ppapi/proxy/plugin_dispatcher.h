// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PLUGIN_DISPATCHER_H_
#define PPAPI_PROXY_PLUGIN_DISPATCHER_H_

#include <stdint.h>

#include <set>
#include <string>
#include <unordered_map>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "build/build_config.h"
#include "ipc/ipc_sync_channel.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/ppb_console.h"
#include "ppapi/proxy/dispatcher.h"
#include "ppapi/proxy/message_handler.h"
#include "ppapi/shared_impl/ppapi_preferences.h"
#include "ppapi/shared_impl/ppb_view_shared.h"
#include "ppapi/shared_impl/singleton_resource_id.h"
#include "ppapi/shared_impl/tracked_callback.h"

namespace IPC {
class SyncMessageFilter;
}

namespace ppapi {

struct Preferences;
class Resource;

namespace thunk {
class PPB_Instance_API;
class ResourceCreationAPI;
}

namespace proxy {

// Used to keep track of per-instance data.
struct PPAPI_PROXY_EXPORT InstanceData {
  InstanceData();
  ~InstanceData();

  ViewData view;

  // When non-NULL, indicates the callback to execute when mouse lock is lost.
  scoped_refptr<TrackedCallback> mouse_lock_callback;

  // A map of singleton resources which are lazily created.
  typedef std::map<SingletonResourceID, scoped_refptr<Resource>>
      SingletonResourceMap;
  SingletonResourceMap singleton_resources;

  // Calls to |RequestSurroundingText()| are done by posted tasks. Track whether
  // a) a task is pending, to avoid redundant calls, and b) whether we should
  // actually call |RequestSurroundingText()|, to avoid stale calls (i.e.,
  // calling when we shouldn't).
  bool is_request_surrounding_text_pending;
  bool should_do_request_surrounding_text;

  // The message handler which should handle JavaScript->Plugin messages, if
  // one has been registered, otherwise NULL.
  std::unique_ptr<MessageHandler> message_handler;

  // Flush info for PpapiCommandBufferProxy::OrderingBarrier().
  struct PPAPI_PROXY_EXPORT FlushInfo {
    FlushInfo();
    ~FlushInfo();
    bool flush_pending;
    HostResource resource;
    int32_t put_offset;
  };
  FlushInfo flush_info;
};

class PPAPI_PROXY_EXPORT LockedSender {
 public:
  // Unlike |Send()|, this function continues to hold the Pepper proxy lock
  // until we are finished sending |msg|, even if it is a synchronous message.
  virtual bool SendAndStayLocked(IPC::Message* msg) = 0;

 protected:
  virtual ~LockedSender() {}
};

class PPAPI_PROXY_EXPORT PluginDispatcher : public Dispatcher,
                                            public LockedSender {
 public:
  class PPAPI_PROXY_EXPORT PluginDelegate : public ProxyChannel::Delegate {
   public:
    // Returns the set used for globally uniquifying PP_Instances. This same
    // set must be returned for all channels.
    //
    // DEREFERENCE ONLY ON THE I/O THREAD.
    virtual std::set<PP_Instance>* GetGloballySeenInstanceIDSet() = 0;

    // Registers the plugin dispatcher and returns an ID.
    // Plugin dispatcher IDs will be used to dispatch messages from the browser.
    // Each call to Register() has to be matched with a call to Unregister().
    virtual uint32_t Register(PluginDispatcher* plugin_dispatcher) = 0;
    virtual void Unregister(uint32_t plugin_dispatcher_id) = 0;
  };

  class Sender : public IPC::Sender,
                 public base::RefCountedThreadSafe<PluginDispatcher::Sender> {
   public:
    Sender(base::WeakPtr<PluginDispatcher> plugin_dispatcher,
           scoped_refptr<IPC::SyncMessageFilter> sync_filter);

    Sender(const Sender&) = delete;
    Sender& operator=(const Sender&) = delete;

    ~Sender() override;

    bool SendMessage(IPC::Message* msg);

    // IPC::Sender
    bool Send(IPC::Message* msg) override;

   private:
    base::WeakPtr<PluginDispatcher> plugin_dispatcher_;
    scoped_refptr<IPC::SyncMessageFilter> sync_filter_;
  };

  // Constructor for the plugin side. The init and shutdown functions will be
  // will be automatically called when requested by the renderer side. The
  // module ID will be set upon receipt of the InitializeModule message.
  //
  // Note about permissions: On the plugin side, the dispatcher and the plugin
  // run in the same address space (including in nacl). This means that the
  // permissions here are subject to malicious modification and bypass, and
  // an exploited or malicious plugin could send any IPC messages and just
  // bypass the permissions. All permissions must be checked "for realz" in the
  // host process when receiving messages. We check them on the plugin side
  // primarily to keep honest plugins honest, especially with respect to
  // dev interfaces that they "shouldn't" be using.
  //
  // You must call InitPluginWithChannel after the constructor.
  PluginDispatcher(PP_GetInterface_Func get_interface,
                   const PpapiPermissions& permissions,
                   bool incognito);

  PluginDispatcher(const PluginDispatcher&) = delete;
  PluginDispatcher& operator=(const PluginDispatcher&) = delete;

  virtual ~PluginDispatcher();

  // The plugin side maintains a mapping from PP_Instance to Dispatcher so
  // that we can send the messages to the right channel if there are multiple
  // renderers sharing the same plugin. This mapping is maintained by
  // DidCreateInstance/DidDestroyInstance.
  static PluginDispatcher* GetForInstance(PP_Instance instance);

  // Same as GetForInstance but retrieves the instance from the given resource
  // object as a convenience. Returns NULL on failure.
  static PluginDispatcher* GetForResource(const Resource* resource);

  // Implements the GetInterface function for the plugin to call to retrieve
  // a browser interface.
  static const void* GetBrowserInterface(const char* interface_name);

  // Logs the given log message to the given instance, or, if the instance is
  // invalid, to all instances associated with all dispatchers. Used for
  // global log messages.
  static void LogWithSource(PP_Instance instance,
                            PP_LogLevel level,
                            const std::string& source,
                            const std::string& value);

  const void* GetPluginInterface(const std::string& interface_name);

  // You must call this function before anything else. Returns true on success.
  // The delegate pointer must outlive this class, ownership is not
  // transferred.
  bool InitPluginWithChannel(PluginDelegate* delegate,
                             base::ProcessId peer_pid,
                             const IPC::ChannelHandle& channel_handle,
                             bool is_client);

  // Dispatcher overrides.
  bool IsPlugin() const override;
  // Send the message to the renderer. If |msg| is a synchronous message, we
  // will unlock the ProxyLock so that we can handle incoming messages from the
  // renderer.
  bool Send(IPC::Message* msg) override;

  // Unlike |Send()|, this function continues to hold the Pepper proxy lock
  // until we are finished sending |msg|, even if it is a synchronous message.
  bool SendAndStayLocked(IPC::Message* msg) override;

  // IPC::Listener implementation.
  bool OnMessageReceived(const IPC::Message& msg) override;
  void OnChannelError() override;

  // Keeps track of which dispatcher to use for each instance, active instances
  // and tracks associated data like the current size.
  void DidCreateInstance(PP_Instance instance);
  void DidDestroyInstance(PP_Instance instance);

  // Gets the data for an existing instance, or NULL if the instance id doesn't
  // correspond to a known instance.
  InstanceData* GetInstanceData(PP_Instance instance);

  // Returns the corresponding API. These are APIs not associated with a
  // resource. Guaranteed non-NULL.
  thunk::PPB_Instance_API* GetInstanceAPI();
  thunk::ResourceCreationAPI* GetResourceCreationAPI();

  // Returns the Preferences.
  const Preferences& preferences() const { return preferences_; }

  uint32_t plugin_dispatcher_id() const { return plugin_dispatcher_id_; }
  bool incognito() const { return incognito_; }

  scoped_refptr<Sender> sender() { return sender_; }

 private:
  friend class PluginDispatcherTest;

  // Notifies all live instances that they're now closed. This is used when
  // a renderer crashes or some other error is received.
  void ForceFreeAllInstances();

  // IPC message handlers.
  void OnMsgSupportsInterface(const std::string& interface_name, bool* result);
  void OnMsgSetPreferences(const Preferences& prefs);

  PluginDelegate* plugin_delegate_;

  // Contains all the plugin interfaces we've queried. The mapped value will
  // be the pointer to the interface pointer supplied by the plugin if it's
  // supported, or NULL if it's not supported. This allows us to cache failures
  // and not req-query if a plugin doesn't support the interface.
  typedef std::unordered_map<std::string, const void*> InterfaceMap;
  InterfaceMap plugin_interfaces_;

  typedef std::unordered_map<PP_Instance, std::unique_ptr<InstanceData>>
      InstanceDataMap;
  InstanceDataMap instance_map_;

  // The preferences sent from the host. We only want to set this once, which
  // is what the received_preferences_ indicates. See OnMsgSetPreferences.
  bool received_preferences_;
  Preferences preferences_;

  uint32_t plugin_dispatcher_id_;

  // Set to true when the instances associated with this dispatcher are
  // incognito mode.
  bool incognito_;

  scoped_refptr<Sender> sender_;

  base::WeakPtrFactory<PluginDispatcher> weak_ptr_factory_{this};
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PLUGIN_DISPATCHER_H_
