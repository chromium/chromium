// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_MESSAGING_EXTENSION_MESSAGE_PORT_H_
#define EXTENSIONS_BROWSER_API_MESSAGING_EXTENSION_MESSAGE_PORT_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/pass_key.h"
#include "base/uuid.h"
#include "extensions/browser/api/messaging/message_port.h"
#include "extensions/browser/service_worker/worker_id.h"
#include "extensions/common/api/messaging/port_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/origin.h"

class GURL;

namespace content {
class BrowserContext;
class RenderFrameHost;
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
                       content::RenderFrameHost* render_frame_host,
                       bool include_child_frames);

  // Create a port that is tied to all frames and service workers of an
  // extension. Should only be used for a receiver port.
  static std::unique_ptr<ExtensionMessagePort> CreateForExtension(
      base::WeakPtr<ChannelDelegate> channel_delegate,
      const PortId& port_id,
      const std::string& extension_id,
      content::BrowserContext* browser_context);

  // Creates a port for any ChannelEndpoint which can be for a render frame or
  // Service Worker.
  static std::unique_ptr<ExtensionMessagePort> CreateForEndpoint(
      base::WeakPtr<ChannelDelegate> channel_delegate,
      const PortId& port_id,
      const std::string& extension_id,
      const ChannelEndpoint& endpoint);

  ExtensionMessagePort(base::WeakPtr<ChannelDelegate> channel_delegate,
                       const PortId& port_id,
                       const ExtensionId& extension_id,
                       content::BrowserContext* browser_context,
                       base::PassKey<ExtensionMessagePort>);

  ExtensionMessagePort(const ExtensionMessagePort&) = delete;
  ~ExtensionMessagePort() override;

  ExtensionMessagePort& operator=(const ExtensionMessagePort&) = delete;

  // MessagePort:
  void RemoveCommonFrames(const MessagePort& port) override;
  bool HasFrame(content::RenderFrameHost* render_frame_host) const override;
  bool IsValidPort() override;
  void RevalidatePort() override;
  void DispatchOnConnect(ChannelType channel_type,
                         const std::string& channel_name,
                         absl::optional<base::Value::Dict> source_tab,
                         const ExtensionApiFrameIdMap::FrameData& source_frame,
                         int guest_process_id,
                         int guest_render_frame_routing_id,
                         const MessagingEndpoint& source_endpoint,
                         const std::string& target_extension_id,
                         const GURL& source_url,
                         absl::optional<url::Origin> source_origin) override;
  void DispatchOnDisconnect(const std::string& error_message) override;
  void DispatchOnMessage(const Message& message) override;
  void IncrementLazyKeepaliveCount(Activity::Type activity_type) override;
  void DecrementLazyKeepaliveCount(Activity::Type activity_type) override;
  void OpenPort(int process_id, const PortContext& port_context) override;
  void ClosePort(int process_id, int routing_id, int worker_thread_id) override;
  void NotifyResponsePending() override;

 private:
  class FrameTracker;
  struct IPCTarget;

  // Registers a frame as a receiver / sender.
  void RegisterFrame(content::RenderFrameHost* render_frame_host);

  // Unregisters a frame as a receiver / sender. When there are no registered
  // frames any more, the port closes via CloseChannel().
  void UnregisterFrame(content::RenderFrameHost* render_frame_host);

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
      ChannelType channel_type,
      const std::string& channel_name,
      const base::Value::Dict* source_tab,
      const ExtensionApiFrameIdMap::FrameData& source_frame,
      int guest_process_id,
      int guest_render_frame_routing_id,
      const MessagingEndpoint& source_endpoint,
      const std::string& target_extension_id,
      const GURL& source_url,
      absl::optional<url::Origin> source_origin,
      const IPCTarget& target);
  std::unique_ptr<IPC::Message> BuildDispatchOnDisconnectIPC(
      const std::string& error_message,
      const IPCTarget& target);
  std::unique_ptr<IPC::Message> BuildDeliverMessageIPC(const Message& message,
                                                       const IPCTarget& target);

  // Check if this activity of this type on this port would keep servicer worker
  // alive.
  bool IsServiceWorkerActivity(Activity::Type activity_type);

  base::WeakPtr<ChannelDelegate> weak_channel_delegate_;

  const PortId port_id_;
  std::string extension_id_;
  raw_ptr<content::BrowserContext> browser_context_ = nullptr;

  // Whether this port corresponds to *all* extension contexts. Should only be
  // true for a receiver port.
  bool for_all_extension_contexts_ = false;

  // When the port is used as a sender, this set contains only one element.
  // If used as a receiver, it may contain any number of frames.
  // This set is populated before the first message is sent to the destination,
  // and shrinks over time when the port is rejected by the recipient frame, or
  // when the frame is removed or unloaded.
  std::set<content::RenderFrameHost*> frames_;

  // Service Worker endpoints for this port.
  std::set<WorkerId> service_workers_;

  // GUIDs of Service Workers that have pending keepalive requests inflight.
  std::map<WorkerId, std::vector<base::Uuid>> pending_keepalive_uuids_;

  // Whether the renderer acknowledged creation of the port. This is used to
  // distinguish abnormal port closure (e.g. no receivers) from explicit port
  // closure (e.g. by the port.disconnect() JavaScript method in the renderer).
  bool port_was_created_ = false;

  // Whether one of the receivers has indicated that it will respond later and
  // the opener should be expecting that response. Used to determine if we
  // should notify the opener of a message port being closed before an expected
  // response was received. By default this is assumed to be false until one of
  // the receivers notifies us otherwise.
  // Note: this is currently only relevant for messaging using
  // OneTimeMessageHandlers, where the receivers are able to indicate they are
  // going to respond asynchronously.
  bool asynchronous_reply_pending_ = false;

  // Used in IncrementLazyKeepaliveCount
  raw_ptr<ExtensionHost, DanglingUntriaged> background_host_ptr_ = nullptr;
  std::unique_ptr<FrameTracker> frame_tracker_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_MESSAGING_EXTENSION_MESSAGE_PORT_H_
