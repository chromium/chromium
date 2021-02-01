// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_MESSAGING_EXTENSION_MESSAGE_PORT_H_
#define EXTENSIONS_BROWSER_API_MESSAGING_EXTENSION_MESSAGE_PORT_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "extensions/browser/api/messaging/message_port.h"
#include "extensions/browser/service_worker/worker_id.h"
#include "extensions/common/api/messaging/port_id.h"
#include "url/origin.h"

class GURL;

namespace content {
class BrowserContext;
class RenderFrameHost;
class RenderProcessHost;
}  // namespace content

namespace IPC {
class Message;
}  // namespace IPC

namespace extensions {
class ExtensionHost;
class ChannelEndpoint;
struct PortContext;

// A port that manages communication with an extension.
// The port's lifetime will end when either all receivers close the port, or
// when the opener / receiver explicitly closes the channel.
class ExtensionMessagePort : public MessagePort {
 public:
  // Create a port that is tied to frame(s) in a single tab.
  ExtensionMessagePort(base::WeakPtr<ChannelDelegate> channel_delegate,
                       const PortId& port_id,
                       const std::string& extension_id,
                       content::RenderFrameHost* rfh,
                       bool include_child_frames);
  // Create a port that is tied to all frames of an extension, possibly spanning
  // multiple tabs, including the invisible background page, popups, etc.
  ExtensionMessagePort(base::WeakPtr<ChannelDelegate> channel_delegate,
                       const PortId& port_id,
                       const std::string& extension_id,
                       content::RenderProcessHost* extension_process);

  // Creates a port for any ChannelEndpoint which can be for a render frame or
  // Service Worker.
  static std::unique_ptr<ExtensionMessagePort> CreateForEndpoint(
      base::WeakPtr<ChannelDelegate> channel_delegate,
      const PortId& port_id,
      const std::string& extension_id,
      const ChannelEndpoint& endpoint,
      bool include_child_frames);

  ~ExtensionMessagePort() override;

  // MessagePort:
  void RemoveCommonFrames(const MessagePort& port) override;
  bool HasFrame(content::RenderFrameHost* rfh) const override;
  bool IsValidPort() override;
  void RevalidatePort() override;
  void DispatchOnConnect(const std::string& channel_name,
                         std::unique_ptr<base::DictionaryValue> source_tab,
                         int source_frame_id,
                         int guest_process_id,
                         int guest_render_frame_routing_id,
                         const MessagingEndpoint& source_endpoint,
                         const std::string& target_extension_id,
                         const GURL& source_url,
                         base::Optional<url::Origin> source_origin) override;
  void DispatchOnDisconnect(const std::string& error_message) override;
  void DispatchOnMessage(const Message& message) override;
  void IncrementLazyKeepaliveCount() override;
  void DecrementLazyKeepaliveCount() override;
  void OpenPort(int process_id, const PortContext& port_context) override;
  void ClosePort(int process_id, int routing_id, int worker_thread_id) override;

 private:
  class FrameTracker;
  struct IPCTarget;

  ExtensionMessagePort(base::WeakPtr<ChannelDelegate> channel_delegate,
                       const PortId& port_id,
                       content::BrowserContext* browser_context);

  // Registers a frame as a receiver / sender.
  void RegisterFrame(content::RenderFrameHost* rfh);

  // Unregisters a frame as a receiver / sender. When there are no registered
  // frames any more, the port closes via CloseChannel().
  void UnregisterFrame(content::RenderFrameHost* rfh);

  // Returns whether or not a live frame or Service Worker is present for this
  // port.
  bool HasReceivers() const;

  // Methods to register/unregister a Service Worker endpoint for this port.
  void RegisterWorker(const WorkerId& worker_id);
  void UnregisterWorker(const WorkerId& worker_id);
  void UnregisterWorker(int render_process_id, int worker_thread_id);

  // Immediately close the port and its associated channel.
  void CloseChannel();

  using IPCBuilderCallback =
      base::RepeatingCallback<std::unique_ptr<IPC::Message>(const IPCTarget&)>;
  // Sends IPC messages to the renderer for all registered frames and/or service
  // workers.
  void SendToPort(IPCBuilderCallback ipc_builder);

  void SendToIPCTarget(const IPCTarget& target,
                       std::unique_ptr<IPC::Message> message);

  // Builds specific IPCs for a port, with correct frame or worker identifiers.
  std::unique_ptr<IPC::Message> BuildDispatchOnConnectIPC(
      const std::string& channel_name,
      const base::DictionaryValue* source_tab,
      int source_frame_id,
      int guest_process_id,
      int guest_render_frame_routing_id,
      const MessagingEndpoint& source_endpoint,
      const std::string& target_extension_id,
      const GURL& source_url,
      base::Optional<url::Origin> source_origin,
      const IPCTarget& target);
  std::unique_ptr<IPC::Message> BuildDispatchOnDisconnectIPC(
      const std::string& error_message,
      const IPCTarget& target);
  std::unique_ptr<IPC::Message> BuildDeliverMessageIPC(const Message& message,
                                                       const IPCTarget& target);

  base::WeakPtr<ChannelDelegate> weak_channel_delegate_;

  const PortId port_id_;
  std::string extension_id_;
  content::BrowserContext* browser_context_ = nullptr;
  // Only for receivers in an extension process.
  content::RenderProcessHost* extension_process_ = nullptr;

  // When the port is used as a sender, this set contains only one element.
  // If used as a receiver, it may contain any number of frames.
  // This set is populated before the first message is sent to the destination,
  // and shrinks over time when the port is rejected by the recipient frame, or
  // when the frame is removed or unloaded.
  std::set<content::RenderFrameHost*> frames_;

  // Service Worker endpoints for this port.
  std::set<WorkerId> service_workers_;

  // GUIDs of Service Workers that have pending keepalive requests inflight.
  std::map<WorkerId, std::vector<std::string>> pending_keepalive_uuids_;

  // Whether the renderer acknowledged creation of the port. This is used to
  // distinguish abnormal port closure (e.g. no receivers) from explicit port
  // closure (e.g. by the port.disconnect() JavaScript method in the renderer).
  bool did_create_port_ = false;

  // Used in IncrementLazyKeepaliveCount
  ExtensionHost* background_host_ptr_ = nullptr;
  std::unique_ptr<FrameTracker> frame_tracker_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionMessagePort);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_MESSAGING_EXTENSION_MESSAGE_PORT_H_
