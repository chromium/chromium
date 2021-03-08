// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_MESSAGING_MESSAGE_SERVICE_H_
#define EXTENSIONS_BROWSER_API_MESSAGING_MESSAGE_SERVICE_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "extensions/browser/api/messaging/message_port.h"
#include "extensions/browser/api/messaging/native_message_host.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/lazy_context_id.h"
#include "extensions/browser/lazy_context_task_queue.h"
#include "extensions/common/api/messaging/message.h"
#include "extensions/common/api/messaging/port_id.h"
#include "extensions/common/extension_id.h"

class GURL;

namespace content {
class BrowserContext;
}

namespace extensions {
class ChannelEndpoint;
class Extension;
class ExtensionHost;
class MessagingDelegate;
struct MessagingEndpoint;
struct PortContext;

// This class manages message and event passing between renderer processes.
// It maintains a list of processes that are listening to events and a set of
// open channels.
//
// Messaging works this way:
// - An extension-owned script context (like a background page or a content
//   script) adds an event listener to the "onConnect" event.
// - Another context calls "runtime.connect()" to open a channel to the
// extension process, or an extension context calls "tabs.connect(tabId)" to
// open a channel to the content scripts for the given tab.  The EMS notifies
// the target process/tab, which then calls the onConnect event in every
// context owned by the connecting extension in that process/tab.
// - Once the channel is established, either side can call postMessage to send
// a message to the opposite side of the channel, which may have multiple
// listeners.
//
// Terminology:
// channel: connection between two ports
// port: One sender or receiver tied to one or more RenderFrameHost instances.
class MessageService : public BrowserContextKeyedAPI,
                       public MessagePort::ChannelDelegate {
 public:
  // A messaging channel. Note that the opening port can be the same as the
  // receiver, if an extension background page wants to talk to its tab (for
  // example).
  struct MessageChannel;

  explicit MessageService(content::BrowserContext* context);
  ~MessageService() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<MessageService>* GetFactoryInstance();

  // MessagePort::ChannelDelegate implementation.
  void CloseChannel(const PortId& port_id,
                    const std::string& error_message) override;
  void PostMessage(const PortId& port_id, const Message& message) override;

  // Convenience method to get the MessageService for a browser context.
  static MessageService* Get(content::BrowserContext* context);

  // Given an extension's ID, opens a channel between the given renderer "port"
  // and every listening context owned by that extension. |channel_name| is
  // an optional identifier for use by extension developers. |opener_port| is an
  // optional pre-opened port that should be attached to the opened channel.
  void OpenChannelToExtension(const ChannelEndpoint& source,
                              const PortId& source_port_id,
                              const MessagingEndpoint& source_endpoint,
                              std::unique_ptr<MessagePort> opener_port,
                              const std::string& target_extension_id,
                              const GURL& source_url,
                              const std::string& channel_name);

  // Same as above, but opens a channel to the tab with the given ID.  Messages
  // are restricted to that tab, so if there are multiple tabs in that process,
  // only the targeted tab will receive messages.
  void OpenChannelToTab(const ChannelEndpoint& source,
                        const PortId& source_port_id,
                        int tab_id,
                        int frame_id,
                        const std::string& extension_id,
                        const std::string& channel_name);

  void OpenChannelToNativeApp(const ChannelEndpoint& source,
                              const PortId& source_port_id,
                              const std::string& native_app_name);

  // Marks the given port as opened by |port_context| in the render process
  // with id |process_id|.
  void OpenPort(const PortId& port_id,
                int process_id,
                const PortContext& port_context);

  // Closes the given port in the given |port_context|. If this was the last
  // context or if |force_close| is true, then the other side is closed as well.
  void ClosePort(const PortId& port_id,
                 int process_id,
                 const PortContext& port_context,
                 bool force_close);

  base::WeakPtr<MessagePort::ChannelDelegate> GetChannelDelegate() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  friend class MockMessageService;
  friend class BrowserContextKeyedAPIFactory<MessageService>;
  struct OpenChannelParams;

  // A map of channel ID to its channel object.
  using MessageChannelMap =
      std::map<ChannelId, std::unique_ptr<MessageChannel>>;

  using PendingMessage = std::pair<PortId, Message>;
  using PendingMessagesQueue = std::vector<PendingMessage>;
  // A set of channel IDs waiting to complete opening, and any pending messages
  // queued to be sent on those channels.
  using PendingChannelMap = std::map<ChannelId, PendingMessagesQueue>;

  // A map of channel ID to information about the extension that is waiting
  // for that channel to open. Used for lazy background pages or Service
  // Workers.
  using PendingLazyContextChannelMap = std::map<ChannelId, LazyContextId>;

  // Common implementation for opening a channel configured by |params|.
  //
  // |target_extension| will be non-null if |params->target_extension_id| is
  // non-empty, that is, if the target is an extension, it must exist.
  //
  // |did_enqueue| will be true if the channel opening was delayed while
  // waiting for an event page to start, false otherwise.
  void OpenChannelImpl(content::BrowserContext* browser_context,
                       std::unique_ptr<OpenChannelParams> params,
                       const Extension* target_extension,
                       bool did_enqueue);

  void ClosePortImpl(const PortId& port_id,
                     int process_id,
                     int routing_id,
                     int worker_thread_id,
                     bool force_close,
                     const std::string& error_message);

  void CloseChannelImpl(MessageChannelMap::iterator channel_iter,
                        const PortId& port_id,
                        const std::string& error_message,
                        bool notify_other_port);

  // Have MessageService take ownership of |channel|, and remove any pending
  // channels with the same id.
  void AddChannel(std::unique_ptr<MessageChannel> channel,
                  const PortId& receiver_port_id);

  // If the channel is being opened from an incognito tab the user must allow
  // the connection.
  void OnOpenChannelAllowed(std::unique_ptr<OpenChannelParams> params,
                            bool allowed);

  // Enqueues a message on a pending channel.
  void EnqueuePendingMessage(const PortId& port_id,
                             const ChannelId& channel_id,
                             const Message& message);

  // Enqueues a message on a channel pending on a lazy background page load.
  void EnqueuePendingMessageForLazyBackgroundLoad(const PortId& port_id,
                                                  const ChannelId& channel_id,
                                                  const Message& message);

  // Immediately sends a message to the given port.
  void DispatchMessage(const PortId& port_id,
                       MessageChannel* channel,
                       const Message& message);

  // Potentially registers a pending task with lazy context task queue
  // to open a channel. Returns true if a task was queued.
  // Takes ownership of |params| if true is returned.
  bool MaybeAddPendingLazyContextOpenChannelTask(
      content::BrowserContext* context,
      const Extension* extension,
      std::unique_ptr<OpenChannelParams>* params,
      const PendingMessagesQueue& pending_messages);

  // Callbacks for background task queue tasks. The queue passes in an
  // ExtensionHost to its task callbacks, though some of our callbacks don't
  // use that argument.
  void PendingLazyContextOpenChannel(
      std::unique_ptr<OpenChannelParams> params,
      std::unique_ptr<LazyContextTaskQueue::ContextInfo> context_info);
  void PendingLazyContextClosePort(
      const PortId& port_id,
      int process_id,
      int routing_id,
      int worker_thread_id,
      bool force_close,
      const std::string& error_message,
      std::unique_ptr<LazyContextTaskQueue::ContextInfo> context_info) {
    if (context_info) {
      ClosePortImpl(port_id, process_id, routing_id, worker_thread_id,
                    force_close, error_message);
    }
  }
  void PendingLazyContextPostMessage(
      const PortId& port_id,
      const Message& message,
      std::unique_ptr<LazyContextTaskQueue::ContextInfo> context_info) {
    if (context_info)
      PostMessage(port_id, message);
  }

  void DispatchPendingMessages(const PendingMessagesQueue& queue,
                               const ChannelId& channel_id);

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "MessageService"; }
  static const bool kServiceRedirectedInIncognito = true;
  static const bool kServiceIsCreatedWithBrowserContext = false;
  static const bool kServiceIsNULLWhileTesting = true;

  content::BrowserContext* const context_;

  // Delegate for embedder-specific messaging, e.g. for Chrome tabs.
  // Owned by the ExtensionsAPIClient and guaranteed to outlive |this|.
  MessagingDelegate* messaging_delegate_;

  MessageChannelMap channels_;
  // A set of channel IDs waiting for user permission to cross the border
  // between an incognito page and an app or extension, and any pending messages
  // queued to be sent on those channels.
  PendingChannelMap pending_incognito_channels_;
  PendingLazyContextChannelMap pending_lazy_context_channels_;

  base::WeakPtrFactory<MessageService> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MessageService);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_MESSAGING_MESSAGE_SERVICE_H_
