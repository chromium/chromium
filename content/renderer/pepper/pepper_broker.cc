// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_broker.h"

#include "build/build_config.h"
#include "content/renderer/pepper/pepper_proxy_channel_delegate_impl.h"
#include "content/renderer/pepper/plugin_module.h"
#include "content/renderer/pepper/ppb_broker_impl.h"
#include "content/renderer/pepper/renderer_restrict_dispatch_group.h"
#include "ipc/ipc_channel_handle.h"
#include "ppapi/proxy/broker_dispatcher.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/platform_file.h"

#if defined(OS_POSIX)
#include "base/posix/eintr_wrapper.h"
#endif

#if defined(OS_WIN)
#include <windows.h>
#endif

namespace content {

namespace {

base::SyncSocket::Handle DuplicateHandle(base::SyncSocket::Handle handle) {
  base::SyncSocket::Handle out_handle = base::SyncSocket::kInvalidHandle;
#if defined(OS_WIN)
  DWORD options = DUPLICATE_SAME_ACCESS;
  if (!::DuplicateHandle(::GetCurrentProcess(),
                         handle,
                         ::GetCurrentProcess(),
                         &out_handle,
                         0,
                         FALSE,
                         options)) {
    out_handle = base::SyncSocket::kInvalidHandle;
  }
#elif defined(OS_POSIX)
  // If asked to close the source, we can simply re-use the source fd instead of
  // dup()ing and close()ing.
  out_handle = HANDLE_EINTR(::dup(handle));
#else
#error Not implemented.
#endif
  return out_handle;
}

}  // namespace

PepperBrokerDispatcherWrapper::PepperBrokerDispatcherWrapper() {}

PepperBrokerDispatcherWrapper::~PepperBrokerDispatcherWrapper() {}

bool PepperBrokerDispatcherWrapper::Init(
    base::ProcessId broker_pid,
    const IPC::ChannelHandle& channel_handle) {
  if (!channel_handle.is_mojo_channel_handle())
    return false;

  dispatcher_delegate_.reset(new PepperProxyChannelDelegateImpl);
  dispatcher_.reset(new ppapi::proxy::BrokerHostDispatcher());

  if (!dispatcher_->InitBrokerWithChannel(dispatcher_delegate_.get(),
                                          broker_pid,
                                          channel_handle,
                                          true)) {  // Client.
    dispatcher_.reset();
    dispatcher_delegate_.reset();
    return false;
  }
  dispatcher_->channel()->SetRestrictDispatchChannelGroup(
      kRendererRestrictDispatchGroup_Pepper);
  return true;
}

// Does not take ownership of the local pipe.
int32_t PepperBrokerDispatcherWrapper::SendHandleToBroker(
    PP_Instance instance,
    base::SyncSocket::Handle handle) {
  IPC::PlatformFileForTransit foreign_socket_handle =
      dispatcher_->ShareHandleWithRemote(handle, false);
  if (foreign_socket_handle == IPC::InvalidPlatformFileForTransit())
    return PP_ERROR_FAILED;

  int32_t result = PP_ERROR_FAILED;
  if (!dispatcher_->Send(new PpapiMsg_ConnectToPlugin(
          instance, foreign_socket_handle, &result))) {
    // The plugin did not receive the handle, so it must be closed.
    // The easiest way to clean it up is to just put it in an object
    // and then close it. This failure case is not performance critical.
    // The handle could still leak if Send succeeded but the IPC later failed.
    base::SyncSocket temp_socket(
        IPC::PlatformFileForTransitToPlatformFile(foreign_socket_handle));
    return PP_ERROR_FAILED;
  }

  return result;
}

PepperBroker::PepperBroker(PluginModule* plugin_module)
    : plugin_module_(plugin_module) {
  DCHECK(plugin_module_);

  plugin_module_->SetBroker(this);
}

PepperBroker::~PepperBroker() {
  ReportFailureToClients(PP_ERROR_ABORTED);
  plugin_module_->SetBroker(nullptr);
  plugin_module_ = nullptr;
}

// If the channel is not ready, queue the connection.
void PepperBroker::AddPendingConnect(PPB_Broker_Impl* client) {
  DCHECK(pending_connects_.find(client) == pending_connects_.end())
      << "Connect was already called for this client";

  // Ensure this object and the associated broker exist as long as the
  // client exists. There is a corresponding Release() call in Disconnect(),
  // which is called when the PPB_Broker_Impl is destroyed. The only other
  // possible reference is in pending_connect_broker_, which only holds a
  // transient reference. This ensures the broker is available as long as the
  // plugin needs it and allows the plugin to release the broker when it is no
  // longer using it.
  AddRef();

  pending_connects_[client].client = client->AsWeakPtr();
}

void PepperBroker::Disconnect(PPB_Broker_Impl* client) {
  // Remove the pending connect if one exists. This class will not call client's
  // callback.
  pending_connects_.erase(client);

  // TODO(ddorwin): Send message disconnect message using dispatcher_.

  // Release the reference added in Connect().
  // This must be the last statement because it may delete this object.
  Release();
}

void PepperBroker::OnBrokerChannelConnected(
    base::ProcessId broker_pid,
    const IPC::ChannelHandle& channel_handle) {
  std::unique_ptr<PepperBrokerDispatcherWrapper> dispatcher(
      new PepperBrokerDispatcherWrapper);
  if (!dispatcher->Init(broker_pid, channel_handle)) {
    ReportFailureToClients(PP_ERROR_FAILED);
    return;
  }

  dispatcher_.reset(dispatcher.release());

  // Process all pending channel requests from the plugins.
  for (auto i = pending_connects_.begin(); i != pending_connects_.end();) {
    base::WeakPtr<PPB_Broker_Impl>& weak_ptr = i->second.client;
    if (!i->second.is_authorized) {
      ++i;
      continue;
    }

    if (weak_ptr.get())
      ConnectPluginToBroker(weak_ptr.get());

    pending_connects_.erase(i++);
  }
}

void PepperBroker::OnBrokerPermissionResult(PPB_Broker_Impl* client,
                                            bool result) {
  auto entry = pending_connects_.find(client);
  if (entry == pending_connects_.end())
    return;

  if (!entry->second.client.get()) {
    // Client has gone away.
    pending_connects_.erase(entry);
    return;
  }

  if (!result) {
    // Report failure.
    client->BrokerConnected(
        ppapi::PlatformFileToInt(base::SyncSocket::kInvalidHandle),
        PP_ERROR_NOACCESS);
    pending_connects_.erase(entry);
    return;
  }

  if (dispatcher_) {
    ConnectPluginToBroker(client);
    pending_connects_.erase(entry);
    return;
  }

  // Mark the request as authorized, continue waiting for the broker
  // connection.
  DCHECK(!entry->second.is_authorized);
  entry->second.is_authorized = true;
}

PepperBroker::PendingConnection::PendingConnection() : is_authorized(false) {}

PepperBroker::PendingConnection::PendingConnection(
    const PendingConnection& other) = default;

PepperBroker::PendingConnection::~PendingConnection() {}

void PepperBroker::ReportFailureToClients(int error_code) {
  DCHECK_NE(PP_OK, error_code);
  for (auto i = pending_connects_.begin(); i != pending_connects_.end(); ++i) {
    base::WeakPtr<PPB_Broker_Impl>& weak_ptr = i->second.client;
    if (weak_ptr.get()) {
      weak_ptr->BrokerConnected(
          ppapi::PlatformFileToInt(base::SyncSocket::kInvalidHandle),
          error_code);
    }
  }
  pending_connects_.clear();
}

void PepperBroker::ConnectPluginToBroker(PPB_Broker_Impl* client) {
  base::SyncSocket::Handle plugin_handle = base::SyncSocket::kInvalidHandle;
  int32_t result = PP_OK;

  // The socket objects will be deleted when this function exits, closing the
  // handles. Any uses of the socket must duplicate them.
  std::unique_ptr<base::SyncSocket> broker_socket(new base::SyncSocket());
  std::unique_ptr<base::SyncSocket> plugin_socket(new base::SyncSocket());
  if (base::SyncSocket::CreatePair(broker_socket.get(), plugin_socket.get())) {
    result = dispatcher_->SendHandleToBroker(client->pp_instance(),
                                             broker_socket->handle());

    // If the broker has its pipe handle, duplicate the plugin's handle.
    // Otherwise, the plugin's handle will be automatically closed.
    if (result == PP_OK)
      plugin_handle = DuplicateHandle(plugin_socket->handle());
  } else {
    result = PP_ERROR_FAILED;
  }

  // TODO(ddorwin): Change the IPC to asynchronous: Queue an object containing
  // client and plugin_socket.release(), then return.
  // That message handler will then call client->BrokerConnected() with the
  // saved pipe handle.
  // Temporarily, just call back.
  client->BrokerConnected(ppapi::PlatformFileToInt(plugin_handle), result);
}

}  // namespace content
