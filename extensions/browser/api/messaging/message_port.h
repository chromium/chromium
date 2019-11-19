// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_MESSAGING_MESSAGE_PORT_H_
#define EXTENSIONS_BROWSER_API_MESSAGING_MESSAGE_PORT_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/values.h"

class GURL;

namespace content {
class RenderFrameHost;
}

namespace extensions {
struct Message;
struct MessagingEndpoint;
struct PortId;
struct PortContext;

// One side of the communication handled by extensions::MessageService.
class MessagePort {
 public:
  // Delegate handling the channel between the port and its host.
  class ChannelDelegate {
   public:
    // Closes the message channel associated with the given port, and notifies
    // the other side.
    virtual void CloseChannel(const PortId& port_id,
                              const std::string& error_message) = 0;

    // Enqueues a message on a pending channel, or sends a message to the given
    // port if the channel isn't pending.
    virtual void PostMessage(const PortId& port_id, const Message& message) = 0;
  };

  virtual ~MessagePort();

  // Called right before a channel is created for this MessagePort and |port|.
  // This allows us to ensure that the ports have no RenderFrameHost instances
  // in common.
  virtual void RemoveCommonFrames(const MessagePort& port);

  // Checks whether the given RenderFrameHost is associated with this port.
  virtual bool HasFrame(content::RenderFrameHost* rfh) const;

  // Called right before a port is connected to a channel. If false, the port
  // is not used and the channel is closed.
  virtual bool IsValidPort() = 0;

  // Triggers the check of whether the port is still valid. If the port is
  // determined to be invalid, the channel will be closed.
  virtual void RevalidatePort();

  // Notifies the port that the channel has been opened.
  virtual void DispatchOnConnect(
      const std::string& channel_name,
      std::unique_ptr<base::DictionaryValue> source_tab,
      int source_frame_id,
      int guest_process_id,
      int guest_render_frame_routing_id,
      const MessagingEndpoint& source_endpoint,
      const std::string& target_extension_id,
      const GURL& source_url);

  // Notifies the port that the channel has been closed. If |error_message| is
  // non-empty, it indicates an error occurred while opening the connection.
  virtual void DispatchOnDisconnect(const std::string& error_message);

  // Dispatches a message to this end of the communication.
  virtual void DispatchOnMessage(const Message& message) = 0;

  // Marks the port as opened by the specific frame or service worker.
  virtual void OpenPort(int process_id, const PortContext& port_context);

  // Closes the port for the given frame or service worker.
  virtual void ClosePort(int process_id, int routing_id, int worker_thread_id);

  // MessagePorts that target extensions will need to adjust their keepalive
  // counts for their lazy background page.
  virtual void IncrementLazyKeepaliveCount();
  virtual void DecrementLazyKeepaliveCount();

 protected:
  MessagePort();

 private:
  DISALLOW_COPY_AND_ASSIGN(MessagePort);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_MESSAGING_MESSAGE_PORT_H_
