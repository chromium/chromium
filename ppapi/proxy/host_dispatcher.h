// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_HOST_DISPATCHER_H_
#define PPAPI_PROXY_HOST_DISPATCHER_H_

#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/process/process.h"
#include "base/task/single_thread_task_runner.h"
#include "ipc/message_filter.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/proxy/dispatcher.h"

struct PPB_Proxy_Private;

namespace ppapi {

struct Preferences;

namespace proxy {

class PPAPI_PROXY_EXPORT HostDispatcher : public Dispatcher {
 public:
  // This interface receives notifications about sync messages being sent by
  // the dispatcher to the plugin process. Some parts of Chrome may need to
  // know whether we are sending a synchronous message to the plugin; e.g. to
  // detect a hung plugin or to avoid re-entering JavaScript.
  //
  // Note that there can be nested sync messages, so the begin/end status
  // actually represents a stack of blocking messages.
  class SyncMessageStatusObserver {
   public:
    // Notification that a sync message is about to be sent out.
    virtual void BeginBlockOnSyncMessage() = 0;

    // Notification that a sync message reply was received and the dispatcher
    // is no longer blocked on a sync message.
    virtual void EndBlockOnSyncMessage() = 0;

   protected:
    virtual ~SyncMessageStatusObserver() {}
  };

  // Constructor for the renderer side. This will take a reference to the
  // SyncMessageStatusReceiver.
  //
  // You must call InitHostWithChannel after the constructor.
  HostDispatcher(PP_Module module,
                 PP_GetInterface_Func local_get_interface,
                 const PpapiPermissions& permissions);

  HostDispatcher(const HostDispatcher&) = delete;
  HostDispatcher& operator=(const HostDispatcher&) = delete;

  ~HostDispatcher();

  // You must call this function before anything else. Returns true on success.
  // The delegate pointer must outlive this class, ownership is not
  // transferred.
  virtual bool InitHostWithChannel(
      Delegate* delegate,
      base::ProcessId peer_pid,
      const IPC::ChannelHandle& channel_handle,
      bool is_client,
      const Preferences& preferences,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // The host side maintains a mapping from PP_Instance to Dispatcher so
  // that we can send the messages to the right channel.
  static HostDispatcher* GetForInstance(PP_Instance instance);
  static void SetForInstance(PP_Instance instance,
                             HostDispatcher* dispatcher);
  static void RemoveForInstance(PP_Instance instance);

  // Returns the host's notion of our PP_Module. This will be different than
  // the plugin's notion of its PP_Module because the plugin process may be
  // used by multiple renderer processes.
  //
  // Use this value instead of a value from the plugin whenever talking to the
  // host.
  PP_Module pp_module() const { return pp_module_; }

  // Dispatcher overrides.
  bool IsPlugin() const override;
  bool Send(IPC::Message* msg) override;

  // IPC::Listener.
  bool OnMessageReceived(const IPC::Message& msg) override;
  void OnChannelError() override;

  // Proxied version of calling GetInterface on the plugin. This will check
  // if the plugin supports the given interface (with caching) and returns the
  // pointer to the proxied interface if it is supported. Returns NULL if the
  // given interface isn't supported by the plugin or the proxy.
  const void* GetProxiedInterface(const std::string& iface_name);

  // See the value below. Call this when processing a scripting message from
  // the plugin that can be reentered. This is set to false at the beginning
  // of processing of each message from the plugin.
  void set_allow_plugin_reentrancy() {
    allow_plugin_reentrancy_ = true;
  }

  // Returns the proxy interface for talking to the implementation.
  const PPB_Proxy_Private* ppb_proxy() const { return ppb_proxy_; }

  // Register an observer that will be invoked when the dispatcher begins
  // sending a sync message and finishes sending a sync message.
  // Returns a OnceClosure that can be used to unregister the observer (the
  // OnceClosure is bound to a weak pointer, so is safe to call even after the
  // HostDispatcher is gone.)
  base::OnceClosure AddSyncMessageStatusObserver(
      SyncMessageStatusObserver* obs);

  void AddFilter(IPC::Listener* listener);

 protected:
  // Overridden from Dispatcher.
  void OnInvalidMessageReceived() override;

 private:
  void OnHostMsgLogWithSource(PP_Instance instance,
                              int int_log_level,
                              const std::string& source,
                              const std::string& value);

  void RemoveSyncMessageStatusObserver(SyncMessageStatusObserver* obs);

  PP_Module pp_module_;

  // Maps interface name to whether that interface is supported. If an interface
  // name is not in the map, that implies that we haven't queried for it yet.
  typedef std::unordered_map<std::string, bool> PluginSupportedMap;
  PluginSupportedMap plugin_supported_;

  // Guaranteed non-NULL.
  const PPB_Proxy_Private* ppb_proxy_;

  // Set to true when the plugin is in a state that it can be reentered by a
  // sync message from the host. We allow reentrancy only when we're processing
  // a sync message from the renderer that is a scripting command. When the
  // plugin is in this state, it needs to accept reentrancy since scripting may
  // ultimately call back into the plugin.
  bool allow_plugin_reentrancy_;

  base::ObserverList<SyncMessageStatusObserver>::Unchecked
      sync_status_observer_list_;

  std::vector<IPC::Listener*> filters_;

  base::WeakPtrFactory<HostDispatcher> weak_ptr_factory_{this};
};

// Create this object on the stack to prevent the module (and hence the
// dispatcher) from being deleted out from under you. This is necessary when
// calling some scripting functions that may delete the plugin.
//
// This class does nothing if used on the plugin side.
class ScopedModuleReference {
 public:
  explicit ScopedModuleReference(Dispatcher* dispatcher);

  ScopedModuleReference(const ScopedModuleReference&) = delete;
  ScopedModuleReference& operator=(const ScopedModuleReference&) = delete;

  ~ScopedModuleReference();

 private:
  HostDispatcher* dispatcher_;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_HOST_DISPATCHER_H_
