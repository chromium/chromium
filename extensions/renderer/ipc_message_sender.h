// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_IPC_MESSAGE_SENDER_H_
#define EXTENSIONS_RENDERER_IPC_MESSAGE_SENDER_H_

#include "base/macros.h"

#include <memory>
#include <string>

#include "extensions/renderer/bindings/api_binding_types.h"

struct ExtensionHostMsg_Request_Params;

namespace base {
class DictionaryValue;
}

namespace extensions {
class ScriptContext;
class WorkerThreadDispatcher;
struct Message;
struct MessageTarget;
struct PortId;

// A class to handle sending bindings-related messages to the browser. Different
// versions handle main thread vs. service worker threads.
class IPCMessageSender {
 public:
  virtual ~IPCMessageSender();

  // Sends a request message to the browser.
  virtual void SendRequestIPC(
      ScriptContext* context,
      std::unique_ptr<ExtensionHostMsg_Request_Params> params) = 0;

  // Handles sending any additional messages required after receiving a response
  // to a request.
  virtual void SendOnRequestResponseReceivedIPC(int request_id) = 0;

  // Sends a message to add/remove an unfiltered listener.
  virtual void SendAddUnfilteredEventListenerIPC(
      ScriptContext* context,
      const std::string& event_name) = 0;
  virtual void SendRemoveUnfilteredEventListenerIPC(
      ScriptContext* context,
      const std::string& event_name) = 0;

  // Sends a message to add/remove a lazy unfiltered listener.
  virtual void SendAddUnfilteredLazyEventListenerIPC(
      ScriptContext* context,
      const std::string& event_name) = 0;
  virtual void SendRemoveUnfilteredLazyEventListenerIPC(
      ScriptContext* context,
      const std::string& event_name) = 0;

  // Sends a message to add/remove a filtered listener.
  virtual void SendAddFilteredEventListenerIPC(
      ScriptContext* context,
      const std::string& event_name,
      const base::DictionaryValue& filter,
      bool is_lazy) = 0;
  virtual void SendRemoveFilteredEventListenerIPC(
      ScriptContext* context,
      const std::string& event_name,
      const base::DictionaryValue& filter,
      bool remove_lazy_listener) = 0;

  // Opens a message channel to the specified target.
  virtual void SendOpenMessageChannel(ScriptContext* script_context,
                                      const PortId& port_id,
                                      const MessageTarget& target,
                                      const std::string& channel_name,
                                      bool include_tls_channel_id) = 0;

  // Sends a message to open/close a mesage port or send a message to an
  // existing port.
  virtual void SendOpenMessagePort(int routing_id, const PortId& port_id) = 0;
  virtual void SendCloseMessagePort(int routing_id,
                                    const PortId& port_id,
                                    bool close_channel) = 0;
  virtual void SendPostMessageToPort(const PortId& port_id,
                                     const Message& message) = 0;

  // Creates an IPCMessageSender for use on the main thread.
  static std::unique_ptr<IPCMessageSender> CreateMainThreadIPCMessageSender();

  // Creates an IPCMessageSender for use on a worker thread.
  static std::unique_ptr<IPCMessageSender> CreateWorkerThreadIPCMessageSender(
      WorkerThreadDispatcher* dispatcher,
      int64_t service_worker_version_id);

 protected:
  IPCMessageSender();

  DISALLOW_COPY_AND_ASSIGN(IPCMessageSender);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_IPC_MESSAGE_SENDER_H_
