// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/plugin_dispatcher.h"

#include <map>
#include <memory>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_sync_channel.h"
#include "ipc/ipc_sync_message_filter.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppp_instance.h"
#include "ppapi/proxy/gamepad_resource.h"
#include "ppapi/proxy/interface_list.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/proxy/plugin_globals.h"
#include "ppapi/proxy/plugin_message_filter.h"
#include "ppapi/proxy/plugin_resource_tracker.h"
#include "ppapi/proxy/plugin_var_serialization_rules.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppb_instance_proxy.h"
#include "ppapi/proxy/resource_creation_proxy.h"
#include "ppapi/proxy/resource_reply_thread_registrar.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/shared_impl/resource.h"

namespace ppapi {
namespace proxy {

namespace {

typedef std::map<PP_Instance, PluginDispatcher*> InstanceToPluginDispatcherMap;
InstanceToPluginDispatcherMap* g_instance_to_plugin_dispatcher = NULL;

typedef std::set<PluginDispatcher*> DispatcherSet;
DispatcherSet* g_live_dispatchers = NULL;

}  // namespace

InstanceData::InstanceData()
    : is_request_surrounding_text_pending(false),
      should_do_request_surrounding_text(false) {
}

InstanceData::~InstanceData() {
  // Run any pending mouse lock callback to prevent leaks.
  if (mouse_lock_callback.get())
    mouse_lock_callback->Abort();
}

InstanceData::FlushInfo::FlushInfo()
    : flush_pending(false),
      put_offset(0) {
}

InstanceData::FlushInfo::~FlushInfo() {
}

PluginDispatcher::Sender::Sender(
    base::WeakPtr<PluginDispatcher> plugin_dispatcher,
    scoped_refptr<IPC::SyncMessageFilter> sync_filter)
    : plugin_dispatcher_(plugin_dispatcher), sync_filter_(sync_filter) {}

PluginDispatcher::Sender::~Sender() {}

bool PluginDispatcher::Sender::SendMessage(IPC::Message* msg) {
  // Currently we need to choose between two different mechanisms for sending.
  // On the main thread we use the regular dispatch Send() method, on another
  // thread we use SyncMessageFilter.
  if (PpapiGlobals::Get()
          ->GetMainThreadMessageLoop()
          ->BelongsToCurrentThread()) {
    // The PluginDispatcher may have been destroyed if the channel is gone, but
    // resources are leaked and may still send messages. We ignore those
    // messages. See crbug.com/725033.
    if (plugin_dispatcher_) {
      return plugin_dispatcher_.get()->Dispatcher::Send(msg);
    } else {
      delete msg;
      return false;
    }
  }
  return sync_filter_->Send(msg);
}

bool PluginDispatcher::Sender::Send(IPC::Message* msg) {
  TRACE_EVENT2("ppapi_proxy", "PluginDispatcher::Send", "Class",
               IPC_MESSAGE_ID_CLASS(msg->type()), "Line",
               IPC_MESSAGE_ID_LINE(msg->type()));
  // We always want plugin->renderer messages to arrive in-order. If some sync
  // and some async messages are sent in response to a synchronous
  // renderer->plugin call, the sync reply will be processed before the async
  // reply, and everything will be confused.
  //
  // Allowing all async messages to unblock the renderer means more reentrancy
  // there but gives correct ordering.
  //
  // We don't want reply messages to unblock however, as they will potentially
  // end up on the wrong queue - see crbug.com/122443
  if (!msg->is_reply())
    msg->set_unblock(true);
  if (msg->is_sync()) {
    // Synchronous messages might be re-entrant, so we need to drop the lock.
    ProxyAutoUnlock unlock;
    return SendMessage(msg);
  }
  return SendMessage(msg);
}

PluginDispatcher::PluginDispatcher(PP_GetInterface_Func get_interface,
                                   const PpapiPermissions& permissions,
                                   bool incognito)
    : Dispatcher(get_interface, permissions),
      plugin_delegate_(NULL),
      received_preferences_(false),
      plugin_dispatcher_id_(0),
      incognito_(incognito) {
  sender_ = new Sender(weak_ptr_factory_.GetWeakPtr(),
                       scoped_refptr<IPC::SyncMessageFilter>());
  SetSerializationRules(
      new PluginVarSerializationRules(weak_ptr_factory_.GetWeakPtr()));

  if (!g_live_dispatchers)
    g_live_dispatchers = new DispatcherSet;
  g_live_dispatchers->insert(this);
}

PluginDispatcher::~PluginDispatcher() {
  PluginGlobals::Get()->plugin_var_tracker()->DidDeleteDispatcher(this);

  if (plugin_delegate_)
    plugin_delegate_->Unregister(plugin_dispatcher_id_);

  g_live_dispatchers->erase(this);
  if (g_live_dispatchers->empty()) {
    delete g_live_dispatchers;
    g_live_dispatchers = NULL;
  }
}

// static
PluginDispatcher* PluginDispatcher::GetForInstance(PP_Instance instance) {
  if (!g_instance_to_plugin_dispatcher)
    return NULL;
  InstanceToPluginDispatcherMap::iterator found =
      g_instance_to_plugin_dispatcher->find(instance);
  if (found == g_instance_to_plugin_dispatcher->end())
    return NULL;
  return found->second;
}

// static
PluginDispatcher* PluginDispatcher::GetForResource(const Resource* resource) {
  return GetForInstance(resource->pp_instance());
}

// static
const void* PluginDispatcher::GetBrowserInterface(const char* interface_name) {
  // CAUTION: This function is called directly from the plugin, but we *don't*
  // lock the ProxyLock to avoid excessive locking from C++ wrappers.
  return InterfaceList::GetInstance()->GetInterfaceForPPB(interface_name);
}

// static
void PluginDispatcher::LogWithSource(PP_Instance instance,
                                     PP_LogLevel level,
                                     const std::string& source,
                                     const std::string& value) {
  if (!g_live_dispatchers || !g_instance_to_plugin_dispatcher)
    return;

  if (instance) {
    InstanceToPluginDispatcherMap::iterator found =
        g_instance_to_plugin_dispatcher->find(instance);
    if (found != g_instance_to_plugin_dispatcher->end()) {
      // Send just to this specific dispatcher.
      found->second->Send(new PpapiHostMsg_LogWithSource(
          instance, static_cast<int>(level), source, value));
      return;
    }
  }

  // Instance 0 or invalid, send to all dispatchers.
  for (DispatcherSet::iterator i = g_live_dispatchers->begin();
       i != g_live_dispatchers->end(); ++i) {
    (*i)->Send(new PpapiHostMsg_LogWithSource(
        instance, static_cast<int>(level), source, value));
  }
}

const void* PluginDispatcher::GetPluginInterface(
    const std::string& interface_name) {
  InterfaceMap::iterator found = plugin_interfaces_.find(interface_name);
  if (found == plugin_interfaces_.end()) {
    const void* ret = local_get_interface()(interface_name.c_str());
    plugin_interfaces_.insert(std::make_pair(interface_name, ret));
    return ret;
  }
  return found->second;
}

bool PluginDispatcher::InitPluginWithChannel(
    PluginDelegate* delegate,
    base::ProcessId peer_pid,
    const IPC::ChannelHandle& channel_handle,
    bool is_client) {
  if (!Dispatcher::InitWithChannel(
          delegate, peer_pid, channel_handle, is_client,
          base::SingleThreadTaskRunner::GetCurrentDefault()))
    return false;
  plugin_delegate_ = delegate;
  plugin_dispatcher_id_ = plugin_delegate_->Register(this);

  sender_ = new Sender(weak_ptr_factory_.GetWeakPtr(),
                       channel()->CreateSyncMessageFilter());

  // The message filter will intercept and process certain messages directly
  // on the I/O thread.
  channel()->AddFilter(
      new PluginMessageFilter(
          delegate->GetGloballySeenInstanceIDSet(),
          PluginGlobals::Get()->resource_reply_thread_registrar()));
  return true;
}

bool PluginDispatcher::IsPlugin() const {
  return true;
}

bool PluginDispatcher::Send(IPC::Message* msg) {
  return sender_->Send(msg);
}

bool PluginDispatcher::SendAndStayLocked(IPC::Message* msg) {
  TRACE_EVENT2("ppapi_proxy", "PluginDispatcher::SendAndStayLocked", "Class",
               IPC_MESSAGE_ID_CLASS(msg->type()), "Line",
               IPC_MESSAGE_ID_LINE(msg->type()));
  if (!msg->is_reply())
    msg->set_unblock(true);
  return sender_->SendMessage(msg);
}

bool PluginDispatcher::OnMessageReceived(const IPC::Message& msg) {
  // We need to grab the proxy lock to ensure that we don't collide with the
  // plugin making pepper calls on a different thread.
  ProxyAutoLock lock;
  TRACE_EVENT2("ppapi_proxy", "PluginDispatcher::OnMessageReceived", "Class",
               IPC_MESSAGE_ID_CLASS(msg.type()), "Line",
               IPC_MESSAGE_ID_LINE(msg.type()));

  if (msg.routing_id() == MSG_ROUTING_CONTROL) {
    // Handle some plugin-specific control messages.
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(PluginDispatcher, msg)
      IPC_MESSAGE_HANDLER(PpapiMsg_SupportsInterface, OnMsgSupportsInterface)
      IPC_MESSAGE_HANDLER(PpapiMsg_SetPreferences, OnMsgSetPreferences)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    if (handled)
      return true;
  }
  return Dispatcher::OnMessageReceived(msg);
}

void PluginDispatcher::OnChannelError() {
  Dispatcher::OnChannelError();

  // The renderer has crashed or exited. This channel and all instances
  // associated with it are no longer valid.
  ForceFreeAllInstances();
  // TODO(brettw) free resources too!
  delete this;
}

void PluginDispatcher::DidCreateInstance(PP_Instance instance) {
  if (!g_instance_to_plugin_dispatcher)
    g_instance_to_plugin_dispatcher = new InstanceToPluginDispatcherMap;
  (*g_instance_to_plugin_dispatcher)[instance] = this;
  instance_map_[instance] = std::make_unique<InstanceData>();
}

void PluginDispatcher::DidDestroyInstance(PP_Instance instance) {
  instance_map_.erase(instance);

  if (g_instance_to_plugin_dispatcher) {
    InstanceToPluginDispatcherMap::iterator found =
        g_instance_to_plugin_dispatcher->find(instance);
    if (found != g_instance_to_plugin_dispatcher->end()) {
      DCHECK(found->second == this);
      g_instance_to_plugin_dispatcher->erase(found);
    } else {
      NOTREACHED();
    }
  }
}

InstanceData* PluginDispatcher::GetInstanceData(PP_Instance instance) {
  auto it = instance_map_.find(instance);
  if (it == instance_map_.end())
    return nullptr;
  return it->second.get();
}

thunk::PPB_Instance_API* PluginDispatcher::GetInstanceAPI() {
  return static_cast<PPB_Instance_Proxy*>(
      GetInterfaceProxy(API_ID_PPB_INSTANCE));
}

thunk::ResourceCreationAPI* PluginDispatcher::GetResourceCreationAPI() {
  return static_cast<ResourceCreationProxy*>(
      GetInterfaceProxy(API_ID_RESOURCE_CREATION));
}

void PluginDispatcher::ForceFreeAllInstances() {
  if (!g_instance_to_plugin_dispatcher)
    return;

  // Iterating will remove each item from the map, so we need to make a copy
  // to avoid things changing out from under is.
  InstanceToPluginDispatcherMap temp_map = *g_instance_to_plugin_dispatcher;
  for (InstanceToPluginDispatcherMap::iterator i = temp_map.begin();
       i != temp_map.end(); ++i) {
    if (i->second == this) {
      // Synthesize an "instance destroyed" message, this will notify the
      // plugin and also remove it from our list of tracked plugins.
      PpapiMsg_PPPInstance_DidDestroy msg(API_ID_PPP_INSTANCE, i->first);
      OnMessageReceived(msg);
    }
  }
}

void PluginDispatcher::OnMsgSupportsInterface(
    const std::string& interface_name,
    bool* result) {
  *result = !!GetPluginInterface(interface_name);

  // Do fallback for PPP_Instance. This is a hack here and if we have more
  // cases like this it should be generalized. The PPP_Instance proxy always
  // proxies the 1.1 interface, and then does fallback to 1.0 inside the
  // plugin process (see PPP_Instance_Proxy). So here we return true for
  // supporting the 1.1 interface if either 1.1 or 1.0 is supported.
  if (!*result && interface_name == PPP_INSTANCE_INTERFACE)
    *result = !!GetPluginInterface(PPP_INSTANCE_INTERFACE_1_0);
}

void PluginDispatcher::OnMsgSetPreferences(const Preferences& prefs) {
  // The renderer may send us preferences more than once (currently this
  // happens every time a new plugin instance is created). Since we don't have
  // a way to signal to the plugin that the preferences have changed, changing
  // the default fonts and such in the middle of a running plugin could be
  // confusing to it. As a result, we never allow the preferences to be changed
  // once they're set. The user will have to restart to get new font prefs
  // propagated to plugins.
  if (!received_preferences_) {
    received_preferences_ = true;
    preferences_ = prefs;
  }
}

}  // namespace proxy
}  // namespace ppapi
