// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_IPC_MESSAGE_SENDER_H_
#define EXTENSIONS_RENDERER_IPC_MESSAGE_SENDER_H_

#include <memory>
#include <string>

#include "base/values.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/mojom/frame.mojom-forward.h"
#include "extensions/renderer/bindings/api_binding_types.h"

namespace extensions {

enum class ChannelType;
class ScriptContext;
class WorkerThreadDispatcher;
struct Message;
struct MessageTarget;
struct PortId;

// A class to handle sending bindings-related messages to the browser. Different
// versions handle main thread vs. service worker threads.
class IPCMessageSender {
 public:
  IPCMessageSender(const IPCMessageSender&) = delete;
  IPCMessageSender& operator=(const IPCMessageSender&) = delete;

  virtual ~IPCMessageSender();

  // Used to distinguish API calls & events from each other in activity log.
  enum class ActivityLogCallType { APICALL, EVENT };

  // Sends a request message to the browser.
  virtual void SendRequestIPC(ScriptContext* context,
                              mojom::RequestParamsPtr params) = 0;

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
  virtual void SendAddFilteredEventListenerIPC(ScriptContext* context,
                                               const std::string& event_name,
                                               const base::Value::Dict& filter,
                                               bool is_lazy) = 0;
  virtual void SendRemoveFilteredEventListenerIPC(
      ScriptContext* context,
      const std::string& event_name,
      const base::Value::Dict& filter,
      bool remove_lazy_listener) = 0;

  // Opens a message channel to the specified target.
  virtual void SendOpenMessageChannel(ScriptContext* script_context,
                                      const PortId& port_id,
                                      const MessageTarget& target,
                                      ChannelType channel_type,
                                      const std::string& channel_name) = 0;

  // Sends a message to open/close a mesage port or send a message to an
  // existing port.
  virtual void SendOpenMessagePort(int routing_id, const PortId& port_id) = 0;
  virtual void SendCloseMessagePort(int routing_id,
                                    const PortId& port_id,
                                    bool close_channel) = 0;
  virtual void SendPostMessageToPort(const PortId& port_id,
                                     const Message& message) = 0;

  // Sends a message indicating that a receiver of a message indicated that it
  // plans to send a response later.
  virtual void SendMessageResponsePending(int routing_id,
                                          const PortId& port_id) = 0;

  // Sends activityLog IPC to the browser process.
  virtual void SendActivityLogIPC(const ExtensionId& extension_id,
                                  ActivityLogCallType call_type,
                                  const std::string& call_name,
                                  base::Value::List args,
                                  const std::string& extra) = 0;

  // Creates an IPCMessageSender for use on the main thread.
  static std::unique_ptr<IPCMessageSender> CreateMainThreadIPCMessageSender();

  // Creates an IPCMessageSender for use on a worker thread.
  static std::unique_ptr<IPCMessageSender> CreateWorkerThreadIPCMessageSender(
      WorkerThreadDispatcher* dispatcher,
      int64_t service_worker_version_id);

 protected:
  IPCMessageSender();
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_IPC_MESSAGE_SENDER_H_
