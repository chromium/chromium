// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/messaging/message_service.h"

#include <stdint.h>
#include <limits>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/child_process_host.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/messaging/extension_message_port.h"
#include "extensions/browser/api/messaging/message_port.h"
#include "extensions/browser/api/messaging/messaging_delegate.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_api_frame_id_map.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/extension_web_contents_observer.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/browser/lazy_background_task_queue.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/manifest_handlers/externally_connectable.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "extensions/common/permissions/permissions_data.h"
#include "url/gurl.h"

using content::BrowserContext;
using content::BrowserThread;
using content::SiteInstance;
using content::WebContents;

namespace extensions {

namespace {

const char kReceivingEndDoesntExistError[] =
    "Could not establish connection. Receiving end does not exist.";
#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)
const char kMissingPermissionError[] =
    "Access to native messaging requires nativeMessaging permission.";
const char kProhibitedByPoliciesError[] =
    "Access to the native messaging host was disabled by the system "
    "administrator.";
#endif

enum class IncludeTlsChannelIdBehavior {
  // The TLS channel ID was not requested.
  kNotRequested = 0,

  // The TLS channel ID was requested, but was not included because the target
  // extension did not allow it.
  kRequestedButDenied = 1,

  // The TLS channel ID was requested, but was not found.
  kRequestedButNotFound = 2,

  // The TLS channel ID was requested, allowed, and included in the response.
  kRequestedAndIncluded = 3,

  kMaxValue = kRequestedAndIncluded,
};

void RecordIncludeTlsChannelIdBehavior(IncludeTlsChannelIdBehavior behavior) {
  UMA_HISTOGRAM_ENUMERATION("Extensions.Messaging.IncludeChannelIdBehavior",
                            behavior);
}

}  // namespace

struct MessageService::MessageChannel {
  std::unique_ptr<MessagePort> opener;
  std::unique_ptr<MessagePort> receiver;
};

struct MessageService::OpenChannelParams {
  int source_process_id;
  int source_routing_id;
  std::unique_ptr<base::DictionaryValue> source_tab;
  int source_frame_id;
  std::unique_ptr<MessagePort> receiver;
  PortId receiver_port_id;
  std::string source_extension_id;
  std::string target_extension_id;
  GURL source_url;
  std::string channel_name;
  bool include_tls_channel_id;
  bool requested_tls_channel_id;
  std::string tls_channel_id;
  bool include_guest_process_info;

  // Takes ownership of receiver.
  OpenChannelParams(int source_process_id,
                    int source_routing_id,
                    std::unique_ptr<base::DictionaryValue> source_tab,
                    int source_frame_id,
                    MessagePort* receiver,
                    const PortId& receiver_port_id,
                    const std::string& source_extension_id,
                    const std::string& target_extension_id,
                    const GURL& source_url,
                    const std::string& channel_name,
                    bool include_tls_channel_id,
                    bool requested_tls_channel_id,
                    bool include_guest_process_info)
      : source_process_id(source_process_id),
        source_routing_id(source_routing_id),
        source_frame_id(source_frame_id),
        receiver(receiver),
        receiver_port_id(receiver_port_id),
        source_extension_id(source_extension_id),
        target_extension_id(target_extension_id),
        source_url(source_url),
        channel_name(channel_name),
        include_tls_channel_id(include_tls_channel_id),
        requested_tls_channel_id(requested_tls_channel_id),
        include_guest_process_info(include_guest_process_info) {
    if (source_tab)
      this->source_tab = std::move(source_tab);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(OpenChannelParams);
};

namespace {

static content::RenderProcessHost* GetExtensionProcess(
    BrowserContext* context,
    const std::string& extension_id) {
  scoped_refptr<SiteInstance> site_instance =
      ProcessManager::Get(context)->GetSiteInstanceForURL(
          Extension::GetBaseURLFromExtensionId(extension_id));
  return site_instance->HasProcess() ? site_instance->GetProcess() : NULL;
}

}  // namespace

MessageService::MessageService(BrowserContext* context)
    : messaging_delegate_(ExtensionsAPIClient::Get()->GetMessagingDelegate()),
      lazy_background_task_queue_(LazyBackgroundTaskQueue::Get(context)),
      weak_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_NE(nullptr, messaging_delegate_);
}

MessageService::~MessageService() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  channels_.clear();
}

static base::LazyInstance<
    BrowserContextKeyedAPIFactory<MessageService>>::DestructorAtExit g_factory =
    LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<MessageService>*
MessageService::GetFactoryInstance() {
  return g_factory.Pointer();
}

// static
MessageService* MessageService::Get(BrowserContext* context) {
  return BrowserContextKeyedAPIFactory<MessageService>::Get(context);
}

void MessageService::OpenChannelToExtension(
    int source_process_id,
    int source_routing_id,
    const PortId& source_port_id,
    const std::string& source_extension_id,
    const std::string& target_extension_id,
    const GURL& source_url,
    const std::string& channel_name,
    bool include_tls_channel_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(source_port_id.is_opener);

  // Record if the channel requested the channel id. We may not respect the
  // request if the target extension is not externally connectable.
  const bool requested_include_tls_channel_id = include_tls_channel_id;

  content::RenderFrameHost* source =
      content::RenderFrameHost::FromID(source_process_id, source_routing_id);
  if (!source)
    return;
  BrowserContext* context = source->GetProcess()->GetBrowserContext();

  ExtensionRegistry* registry = ExtensionRegistry::Get(context);
  const Extension* target_extension =
      registry->enabled_extensions().GetByID(target_extension_id);
  PortId receiver_port_id(source_port_id.context_id, source_port_id.port_number,
                          false);
  if (!target_extension) {
    DispatchOnDisconnect(
        source, receiver_port_id, kReceivingEndDoesntExistError);
    return;
  }

  bool is_web_connection = false;

  if (source_extension_id != target_extension_id) {
    // It's an external connection. Check the externally_connectable manifest
    // key if it's present. If it's not, we allow connection from any extension
    // but not webpages.
    ExternallyConnectableInfo* externally_connectable =
        static_cast<ExternallyConnectableInfo*>(
            target_extension->GetManifestData(
                manifest_keys::kExternallyConnectable));
    bool is_externally_connectable = false;

    if (externally_connectable) {
      if (source_extension_id.empty()) {
        // No source extension ID so the source was a web page. Check that the
        // URL matches.
        is_web_connection = true;
        is_externally_connectable =
            externally_connectable->matches.MatchesURL(source_url);
        // Only include the TLS channel ID for externally connected web pages.
        include_tls_channel_id &=
            is_externally_connectable &&
            externally_connectable->accepts_tls_channel_id;
      } else {
        // Source extension ID so the source was an extension. Check that the
        // extension matches.
        is_externally_connectable =
            externally_connectable->IdCanConnect(source_extension_id);
      }
    } else {
      // Default behaviour. Any extension, no webpages.
      is_externally_connectable = !source_extension_id.empty();
    }

    if (!is_externally_connectable) {
      // Important: use kReceivingEndDoesntExistError here so that we don't
      // leak information about this extension to callers. This way it's
      // indistinguishable from the extension just not existing.
      DispatchOnDisconnect(
          source, receiver_port_id, kReceivingEndDoesntExistError);
      return;
    }
  }

  WebContents* source_contents = nullptr;
  content::RenderFrameHost* source_render_frame_host =
      content::RenderFrameHost::FromID(source_process_id, source_routing_id);
  if (source_render_frame_host) {
    source_contents =
        WebContents::FromRenderFrameHost(source_render_frame_host);
  }

  int source_frame_id = -1;
  bool include_guest_process_info = false;

  // Get information about the opener's tab, if applicable.
  std::unique_ptr<base::DictionaryValue> source_tab =
      messaging_delegate_->MaybeGetTabInfo(source_contents);

  if (source_tab.get()) {
    DCHECK(source_render_frame_host);
    source_frame_id =
        ExtensionApiFrameIdMap::GetFrameId(source_render_frame_host);
  } else {
    // Check to see if it was a WebView making the request.
    // Sending messages from WebViews to extensions breaks webview isolation,
    // so only allow component extensions to receive messages from WebViews.
    bool is_web_view = !!WebViewGuest::FromWebContents(source_contents);
    if (is_web_view &&
        Manifest::IsComponentLocation(target_extension->location())) {
      include_guest_process_info = true;
    }
  }

  std::unique_ptr<OpenChannelParams> params(new OpenChannelParams(
      source_process_id, source_routing_id, std::move(source_tab),
      source_frame_id, nullptr, receiver_port_id, source_extension_id,
      target_extension_id, source_url, channel_name, include_tls_channel_id,
      requested_include_tls_channel_id, include_guest_process_info));

  pending_incognito_channels_[params->receiver_port_id.GetChannelId()] =
      PendingMessagesQueue();
  if (context->IsOffTheRecord() &&
      !util::IsIncognitoEnabled(target_extension_id, context)) {
    // Give the user a chance to accept an incognito connection from the web if
    // they haven't already, with the conditions:
    // - Only for non-split mode incognito. We don't want the complication of
    //   spinning up an additional process here which might need to do some
    //   setup that we're not expecting.
    // - Only for extensions that can't normally be enabled in incognito, since
    //   that surface (e.g. chrome://extensions) should be the only one for
    //   enabling in incognito. In practice this means platform apps only.
    if (!is_web_connection || IncognitoInfo::IsSplitMode(target_extension) ||
        util::CanBeIncognitoEnabled(target_extension)) {
      OnOpenChannelAllowed(std::move(params), false);
      return;
    }

    // If the target extension isn't even listening for connect/message events,
    // there is no need to go any further and the connection should be
    // rejected without showing a prompt. See http://crbug.com/442497
    EventRouter* event_router = EventRouter::Get(context);
    const char* const events[] = {"runtime.onConnectExternal",
                                  "runtime.onMessageExternal",
                                  "extension.onRequestExternal",
                                  nullptr};
    bool has_event_listener = false;
    for (const char* const* event = events; *event; event++) {
      has_event_listener |=
          event_router->ExtensionHasEventListener(target_extension_id, *event);
    }
    if (!has_event_listener) {
      OnOpenChannelAllowed(std::move(params), false);
      return;
    }

    // This check may show a dialog.
    messaging_delegate_->QueryIncognitoConnectability(
        context, target_extension, source_contents, source_url,
        base::Bind(&MessageService::OnOpenChannelAllowed,
                   weak_factory_.GetWeakPtr(), base::Passed(&params)));
    return;
  }

  OnOpenChannelAllowed(std::move(params), true);
}

void MessageService::OpenChannelToNativeApp(
    int source_process_id,
    int source_routing_id,
    const PortId& source_port_id,
    const std::string& native_app_name) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(source_port_id.is_opener);

  content::RenderFrameHost* source =
      content::RenderFrameHost::FromID(source_process_id, source_routing_id);
  if (!source)
    return;

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(source);
  ExtensionWebContentsObserver* extension_web_contents_observer =
      web_contents ?
          ExtensionWebContentsObserver::GetForWebContents(web_contents) :
          nullptr;
  const Extension* extension =
      extension_web_contents_observer ?
          extension_web_contents_observer->GetExtensionFromFrame(source, true) :
          nullptr;

  bool has_permission = extension &&
                        extension->permissions_data()->HasAPIPermission(
                            APIPermission::kNativeMessaging);

  PortId receiver_port_id(source_port_id.context_id, source_port_id.port_number,
                          false);
  if (!has_permission) {
    DispatchOnDisconnect(source, receiver_port_id, kMissingPermissionError);
    return;
  }

  // Verify that the host is not blocked by policies.
  MessagingDelegate::PolicyPermission policy_permission =
      messaging_delegate_->IsNativeMessagingHostAllowed(
          source->GetProcess()->GetBrowserContext(), native_app_name);
  if (policy_permission == MessagingDelegate::PolicyPermission::DISALLOW) {
    DispatchOnDisconnect(source, receiver_port_id, kProhibitedByPoliciesError);
    return;
  }

  std::unique_ptr<MessageChannel> channel = std::make_unique<MessageChannel>();
  channel->opener.reset(
      new ExtensionMessagePort(weak_factory_.GetWeakPtr(), source_port_id,
                               extension->id(), source, false));
  if (!channel->opener->IsValidPort())
    return;
  channel->opener->OpenPort(source_process_id, source_routing_id);

  std::string error = kReceivingEndDoesntExistError;
  std::unique_ptr<MessagePort> receiver(
      messaging_delegate_->CreateReceiverForNativeApp(
          weak_factory_.GetWeakPtr(), source, extension->id(), receiver_port_id,
          native_app_name,
          policy_permission == MessagingDelegate::PolicyPermission::ALLOW_ALL,
          &error));

  if (!receiver.get()) {
    // Abandon the channel.
    DispatchOnDisconnect(source, receiver_port_id, error);
    return;
  }

  channel->receiver = std::move(receiver);

  // Keep the opener alive until the channel is closed.
  channel->opener->IncrementLazyKeepaliveCount();

  AddChannel(std::move(channel), receiver_port_id);
#else  // !(defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX))
  const char kNativeMessagingNotSupportedError[] =
      "Native Messaging is not supported on this platform.";
  DispatchOnDisconnect(
      source, receiver_port_id, kNativeMessagingNotSupportedError);
#endif  // !(defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX))
}

void MessageService::OpenChannelToTab(int source_process_id,
                                      int source_routing_id,
                                      const PortId& source_port_id,
                                      int tab_id,
                                      int frame_id,
                                      const std::string& extension_id,
                                      const std::string& channel_name) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_GE(frame_id, -1);
  DCHECK(source_port_id.is_opener);

  content::RenderFrameHost* source =
      content::RenderFrameHost::FromID(source_process_id, source_routing_id);
  if (!source)
    return;
  content::BrowserContext* browser_context =
      source->GetProcess()->GetBrowserContext();

  PortId receiver_port_id(source_port_id.context_id, source_port_id.port_number,
                          false);
  content::WebContents* receiver_contents =
      messaging_delegate_->GetWebContentsByTabId(browser_context, tab_id);
  if (!receiver_contents || receiver_contents->GetController().NeedsReload()) {
    // The tab isn't loaded yet. Don't attempt to connect.
    DispatchOnDisconnect(
        source, receiver_port_id, kReceivingEndDoesntExistError);
    return;
  }

  std::unique_ptr<MessagePort> receiver =
      messaging_delegate_->CreateReceiverForTab(weak_factory_.GetWeakPtr(),
                                                extension_id, receiver_port_id,
                                                receiver_contents, frame_id);
  if (!receiver.get()) {
    DispatchOnDisconnect(
        source, receiver_port_id, kReceivingEndDoesntExistError);
    return;
  }

  const Extension* extension = nullptr;
  if (!extension_id.empty()) {
    // Source extension == target extension so the extension must exist, or
    // where did the IPC come from?
    extension = ExtensionRegistry::Get(browser_context)
                    ->enabled_extensions()
                    .GetByID(extension_id);
    DCHECK(extension);
  }

  std::unique_ptr<OpenChannelParams> params(new OpenChannelParams(
      source_process_id, source_routing_id,
      std::unique_ptr<base::DictionaryValue>(),  // Source tab doesn't make
                                                 // sense
                                                 // for opening to tabs.
      -1,  // If there is no tab, then there is no frame either.
      receiver.release(), receiver_port_id, extension_id, extension_id,
      GURL(),  // Source URL doesn't make sense for opening to tabs.
      channel_name, false,
      false,    // Connections to tabs don't get TLS channel IDs.
      false));  // Connections to tabs aren't webview guests.
  OpenChannelImpl(receiver_contents->GetBrowserContext(), std::move(params),
                  extension, false /* did_enqueue */);
}

void MessageService::OpenChannelImpl(BrowserContext* browser_context,
                                     std::unique_ptr<OpenChannelParams> params,
                                     const Extension* target_extension,
                                     bool did_enqueue) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(target_extension != nullptr, !params->target_extension_id.empty());

  content::RenderFrameHost* source =
      content::RenderFrameHost::FromID(params->source_process_id,
                                       params->source_routing_id);
  if (!source)
    return;  // Closed while in flight.

  if (!params->receiver || !params->receiver->IsValidPort()) {
    DispatchOnDisconnect(source, params->receiver_port_id,
                         kReceivingEndDoesntExistError);
    return;
  }

  std::unique_ptr<ExtensionMessagePort> opener(new ExtensionMessagePort(
      weak_factory_.GetWeakPtr(), params->receiver_port_id.GetOppositePortId(),
      params->source_extension_id, source, false));
  if (!opener->IsValidPort())
    return;
  opener->OpenPort(params->source_process_id, params->source_routing_id);
  opener->RevalidatePort();

  params->receiver->RemoveCommonFrames(*opener);
  if (!params->receiver->IsValidPort()) {
    opener->DispatchOnDisconnect(kReceivingEndDoesntExistError);
    return;
  }

  std::unique_ptr<MessageChannel> channel_ptr =
      std::make_unique<MessageChannel>();
  MessageChannel* channel = channel_ptr.get();
  channel->opener = std::move(opener);
  channel->receiver = std::move(params->receiver);
  AddChannel(std::move(channel_ptr), params->receiver_port_id);

  int guest_process_id = content::ChildProcessHost::kInvalidUniqueID;
  int guest_render_frame_routing_id = MSG_ROUTING_NONE;
  if (params->include_guest_process_info) {
    guest_process_id = params->source_process_id;
    guest_render_frame_routing_id = params->source_routing_id;

    DCHECK(WebViewGuest::FromWebContents(
            WebContents::FromRenderFrameHost(source)));
  }

  // Send the connect event to the receiver.  Give it the opener's port ID (the
  // opener has the opposite port ID).
  channel->receiver->DispatchOnConnect(
      params->channel_name, std::move(params->source_tab),
      params->source_frame_id, guest_process_id, guest_render_frame_routing_id,
      params->source_extension_id, params->target_extension_id,
      params->source_url, params->tls_channel_id);

  // Report the event to the event router, if the target is an extension.
  //
  // First, determine what event this will be (runtime.onConnect vs
  // runtime.onMessage etc), and what the event target is (view vs background
  // page etc).
  //
  // Yes - even though this is opening a channel, they may actually be
  // runtime.onRequest/onMessage events because those single-use events are
  // built using the connect framework (see messaging.js).
  //
  // Likewise, if you're wondering about native messaging events, these are
  // only initiated *by* the extension, so aren't really events, just the
  // endpoint of a communication channel.
  if (target_extension) {
    events::HistogramValue histogram_value = events::UNKNOWN;
    bool is_external =
        params->source_extension_id != params->target_extension_id;
    if (params->channel_name == "chrome.runtime.onRequest") {
      histogram_value = is_external ? events::RUNTIME_ON_REQUEST_EXTERNAL
                                    : events::RUNTIME_ON_REQUEST;
    } else if (params->channel_name == "chrome.runtime.onMessage") {
      histogram_value = is_external ? events::RUNTIME_ON_MESSAGE_EXTERNAL
                                    : events::RUNTIME_ON_MESSAGE;
    } else {
      histogram_value = is_external ? events::RUNTIME_ON_CONNECT_EXTERNAL
                                    : events::RUNTIME_ON_CONNECT;
    }
    EventRouter::Get(browser_context)
        ->ReportEvent(histogram_value, target_extension, did_enqueue);
  }

  // Keep both ends of the channel alive until the channel is closed.
  channel->opener->IncrementLazyKeepaliveCount();
  channel->receiver->IncrementLazyKeepaliveCount();
}

void MessageService::AddChannel(std::unique_ptr<MessageChannel> channel,
                                const PortId& receiver_port_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ChannelId channel_id = receiver_port_id.GetChannelId();
  CHECK(channels_.find(channel_id) == channels_.end());
  channels_[channel_id] = std::move(channel);
  pending_lazy_background_page_channels_.erase(channel_id);
}

void MessageService::OpenPort(const PortId& port_id,
                              int process_id,
                              int routing_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!port_id.is_opener);

  ChannelId channel_id = port_id.GetChannelId();
  auto it = channels_.find(channel_id);
  if (it == channels_.end())
    return;

  it->second->receiver->OpenPort(process_id, routing_id);
}

void MessageService::ClosePort(const PortId& port_id,
                               int process_id,
                               int routing_id,
                               bool force_close) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ClosePortImpl(port_id, process_id, routing_id, force_close, std::string());
}

void MessageService::CloseChannel(const PortId& port_id,
                                  const std::string& error_message) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ClosePortImpl(port_id, content::ChildProcessHost::kInvalidUniqueID,
                MSG_ROUTING_NONE, true, error_message);
}

void MessageService::ClosePortImpl(const PortId& port_id,
                                   int process_id,
                                   int routing_id,
                                   bool force_close,
                                   const std::string& error_message) {
  // Note: The channel might be gone already, if the other side closed first.
  ChannelId channel_id = port_id.GetChannelId();
  auto it = channels_.find(channel_id);
  if (it == channels_.end()) {
    auto pending = pending_lazy_background_page_channels_.find(channel_id);
    if (pending != pending_lazy_background_page_channels_.end()) {
      lazy_background_task_queue_->AddPendingTask(
          pending->second.first, pending->second.second,
          base::BindOnce(&MessageService::PendingLazyBackgroundPageClosePort,
                         weak_factory_.GetWeakPtr(), port_id, process_id,
                         routing_id, force_close, error_message));
    }
    return;
  }

  // The difference between closing a channel and port is that closing a port
  // does not necessarily have to destroy the channel if there are multiple
  // receivers, whereas closing a channel always forces all ports to be closed.
  if (force_close) {
    CloseChannelImpl(it, port_id, error_message, true);
  } else if (port_id.is_opener) {
    it->second->opener->ClosePort(process_id, routing_id);
  } else {
    it->second->receiver->ClosePort(process_id, routing_id);
  }
}

void MessageService::CloseChannelImpl(MessageChannelMap::iterator channel_iter,
                                      const PortId& closing_port_id,
                                      const std::string& error_message,
                                      bool notify_other_port) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::unique_ptr<MessageChannel> channel = std::move(channel_iter->second);
  // Remove from map to make sure that it is impossible for CloseChannelImpl to
  // run twice for the same channel.
  channels_.erase(channel_iter);

  // Notify the other side.
  if (notify_other_port) {
    MessagePort* port = closing_port_id.is_opener ? channel->receiver.get()
                                                  : channel->opener.get();
    port->DispatchOnDisconnect(error_message);
  }

  // Balance the IncrementLazyKeepaliveCount() in OpenChannelImpl.
  channel->opener->DecrementLazyKeepaliveCount();
  channel->receiver->DecrementLazyKeepaliveCount();
}

void MessageService::PostMessage(const PortId& source_port_id,
                                 const Message& message) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ChannelId channel_id = source_port_id.GetChannelId();
  auto iter = channels_.find(channel_id);
  if (iter == channels_.end()) {
    // If this channel is pending, queue up the PostMessage to run once
    // the channel opens.
    EnqueuePendingMessage(source_port_id, channel_id, message);
    return;
  }

  DispatchMessage(source_port_id, iter->second.get(), message);
}

void MessageService::EnqueuePendingMessage(const PortId& source_port_id,
                                           const ChannelId& channel_id,
                                           const Message& message) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto pending_for_incognito = pending_incognito_channels_.find(channel_id);
  if (pending_for_incognito != pending_incognito_channels_.end()) {
    pending_for_incognito->second.push_back(
        PendingMessage(source_port_id, message));
    // A channel should only be holding pending messages because it is in one
    // of these states.
    DCHECK(!base::ContainsKey(pending_tls_channel_id_channels_, channel_id));
    DCHECK(
        !base::ContainsKey(pending_lazy_background_page_channels_, channel_id));
    return;
  }
  auto pending_for_tls_channel_id =
      pending_tls_channel_id_channels_.find(channel_id);
  if (pending_for_tls_channel_id != pending_tls_channel_id_channels_.end()) {
    pending_for_tls_channel_id->second.push_back(
        PendingMessage(source_port_id, message));
    // A channel should only be holding pending messages because it is in one
    // of these states.
    DCHECK(
        !base::ContainsKey(pending_lazy_background_page_channels_, channel_id));
    return;
  }
  EnqueuePendingMessageForLazyBackgroundLoad(source_port_id,
                                             channel_id,
                                             message);
}

void MessageService::EnqueuePendingMessageForLazyBackgroundLoad(
    const PortId& source_port_id,
    const ChannelId& channel_id,
    const Message& message) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto pending = pending_lazy_background_page_channels_.find(channel_id);
  if (pending != pending_lazy_background_page_channels_.end()) {
    lazy_background_task_queue_->AddPendingTask(
        pending->second.first, pending->second.second,
        base::BindOnce(&MessageService::PendingLazyBackgroundPagePostMessage,
                       weak_factory_.GetWeakPtr(), source_port_id, message));
  }
}

void MessageService::DispatchMessage(const PortId& source_port_id,
                                     MessageChannel* channel,
                                     const Message& message) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Figure out which port the ID corresponds to.
  MessagePort* dest_port = source_port_id.is_opener ? channel->receiver.get()
                                                    : channel->opener.get();

  dest_port->DispatchOnMessage(message);
}

bool MessageService::MaybeAddPendingLazyBackgroundPageOpenChannelTask(
    BrowserContext* context,
    const Extension* extension,
    std::unique_ptr<OpenChannelParams>* params,
    const PendingMessagesQueue& pending_messages) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!BackgroundInfo::HasLazyBackgroundPage(extension))
    return false;

  // If the extension uses spanning incognito mode, make sure we're always
  // using the original profile since that is what the extension process
  // will use.
  if (!IncognitoInfo::IsSplitMode(extension))
    context = ExtensionsBrowserClient::Get()->GetOriginalContext(context);

  if (!lazy_background_task_queue_->ShouldEnqueueTask(context, extension))
    return false;

  ChannelId channel_id = (*params)->receiver_port_id.GetChannelId();
  pending_lazy_background_page_channels_[channel_id] =
      PendingLazyBackgroundPageChannel(context, extension->id());
  int source_id = (*params)->source_process_id;
  lazy_background_task_queue_->AddPendingTask(
      context, extension->id(),
      base::BindOnce(&MessageService::PendingLazyBackgroundPageOpenChannel,
                     weak_factory_.GetWeakPtr(), base::Passed(params),
                     source_id));

  for (const PendingMessage& message : pending_messages) {
    EnqueuePendingMessageForLazyBackgroundLoad(message.first, channel_id,
                                               message.second);
  }
  return true;
}

void MessageService::OnOpenChannelAllowed(
    std::unique_ptr<OpenChannelParams> params,
    bool allowed) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ChannelId channel_id = params->receiver_port_id.GetChannelId();

  auto pending_for_incognito = pending_incognito_channels_.find(channel_id);
  if (pending_for_incognito == pending_incognito_channels_.end()) {
    NOTREACHED();
    return;
  }
  PendingMessagesQueue pending_messages;
  pending_messages.swap(pending_for_incognito->second);
  pending_incognito_channels_.erase(pending_for_incognito);

  // Re-lookup the source process since it may no longer be valid.
  content::RenderFrameHost* source =
      content::RenderFrameHost::FromID(params->source_process_id,
                                       params->source_routing_id);
  if (!source) {
    return;
  }

  if (!allowed) {
    DispatchOnDisconnect(source, params->receiver_port_id,
                         kReceivingEndDoesntExistError);
    return;
  }

  content::RenderProcessHost* source_process = source->GetProcess();
  BrowserContext* context = source_process->GetBrowserContext();

  // Note: we use the source's profile here. If the source is an incognito
  // process, we will use the incognito EPM to find the right extension process,
  // which depends on whether the extension uses spanning or split mode.
  if (content::RenderProcessHost* extension_process =
          GetExtensionProcess(context, params->target_extension_id)) {
    params->receiver.reset(
        new ExtensionMessagePort(
            weak_factory_.GetWeakPtr(), params->receiver_port_id,
            params->target_extension_id, extension_process));
  } else {
    params->receiver.reset();
  }

  // If the target requests the TLS channel id, begin the lookup for it.
  // The target might also be a lazy background page, checked next, but the
  // loading of lazy background pages continues asynchronously, so enqueue
  // messages awaiting TLS channel ID first.
  if (params->include_tls_channel_id) {
    // Transfer pending messages to the next pending channel list.
    pending_tls_channel_id_channels_[channel_id].swap(pending_messages);
    // Capture this reference before params is invalidated by base::Passed().
    const GURL& source_url = params->source_url;
    // Note: use the RenderProcessHost's StoragePartition (which may vary from
    // the BrowserContext's default StoragePartition, as in the case of platform
    // apps). See https://crbug.com/781070.
    property_provider_.GetChannelID(
        source_process->GetStoragePartition(), source_url,
        base::Bind(&MessageService::GotChannelID, weak_factory_.GetWeakPtr(),
                   base::Passed(&params)));
    // Flow continues in MessageService::GotChannelID(), which will also record
    // tls channel ID behavior.
    return;
  }

  {
    // The connection is not including TLS channel ID information. Log the
    // result.
    const auto tls_channel_id_behavior =
        params->requested_tls_channel_id
            ? IncludeTlsChannelIdBehavior::kRequestedButDenied
            : IncludeTlsChannelIdBehavior::kNotRequested;
    RecordIncludeTlsChannelIdBehavior(tls_channel_id_behavior);
  }

  ExtensionRegistry* registry = ExtensionRegistry::Get(context);
  const Extension* target_extension =
      registry->enabled_extensions().GetByID(params->target_extension_id);
  if (!target_extension) {
    DispatchOnDisconnect(source, params->receiver_port_id,
                         kReceivingEndDoesntExistError);
    return;
  }

  // The target might be a lazy background page. In that case, we have to check
  // if it is loaded and ready, and if not, queue up the task and load the
  // page.
  if (!MaybeAddPendingLazyBackgroundPageOpenChannelTask(
          context, target_extension, &params, pending_messages)) {
    OpenChannelImpl(context, std::move(params), target_extension,
                    false /* did_enqueue */);
    DispatchPendingMessages(pending_messages, channel_id);
  }
}

void MessageService::GotChannelID(std::unique_ptr<OpenChannelParams> params,
                                  const std::string& tls_channel_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  {
    const auto tls_channel_id_behavior =
        tls_channel_id.empty()
            ? IncludeTlsChannelIdBehavior::kRequestedButNotFound
            : IncludeTlsChannelIdBehavior::kRequestedAndIncluded;
    RecordIncludeTlsChannelIdBehavior(tls_channel_id_behavior);
  }

  params->tls_channel_id.assign(tls_channel_id);
  ChannelId channel_id = params->receiver_port_id.GetChannelId();

  auto pending_for_tls_channel_id =
      pending_tls_channel_id_channels_.find(channel_id);
  if (pending_for_tls_channel_id == pending_tls_channel_id_channels_.end()) {
    NOTREACHED();
    return;
  }
  PendingMessagesQueue pending_messages;
  pending_messages.swap(pending_for_tls_channel_id->second);
  pending_tls_channel_id_channels_.erase(pending_for_tls_channel_id);

  // Re-lookup the source process since it may no longer be valid.
  content::RenderFrameHost* source =
      content::RenderFrameHost::FromID(params->source_process_id,
                                       params->source_routing_id);
  if (!source) {
    return;
  }

  BrowserContext* context = source->GetProcess()->GetBrowserContext();
  ExtensionRegistry* registry = ExtensionRegistry::Get(context);
  const Extension* target_extension =
      registry->enabled_extensions().GetByID(params->target_extension_id);
  if (!target_extension) {
    DispatchOnDisconnect(source, params->receiver_port_id,
                         kReceivingEndDoesntExistError);
    return;
  }

  if (!MaybeAddPendingLazyBackgroundPageOpenChannelTask(
          context, target_extension, &params, pending_messages)) {
    OpenChannelImpl(context, std::move(params), target_extension,
                    false /* did_enqueue */);
    DispatchPendingMessages(pending_messages, channel_id);
  }
}

void MessageService::PendingLazyBackgroundPageOpenChannel(
    std::unique_ptr<OpenChannelParams> params,
    int source_process_id,
    ExtensionHost* host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!host)
    return;  // TODO(mpcomplete): notify source of disconnect?

  params->receiver.reset(
      new ExtensionMessagePort(
          weak_factory_.GetWeakPtr(), params->receiver_port_id,
          params->target_extension_id, host->render_process_host()));
  OpenChannelImpl(host->browser_context(), std::move(params), host->extension(),
                  true /* did_enqueue */);
}

void MessageService::DispatchOnDisconnect(content::RenderFrameHost* source,
                                          const PortId& port_id,
                                          const std::string& error_message) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ExtensionMessagePort port(weak_factory_.GetWeakPtr(),
                            port_id.GetOppositePortId(), "", source, false);
  if (!port.IsValidPort())
    return;
  port.DispatchOnDisconnect(error_message);
}

void MessageService::DispatchPendingMessages(const PendingMessagesQueue& queue,
                                             const ChannelId& channel_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto channel_iter = channels_.find(channel_id);
  if (channel_iter != channels_.end()) {
    for (const PendingMessage& message : queue) {
      DispatchMessage(message.first, channel_iter->second.get(),
                      message.second);
    }
  }
}

}  // namespace extensions
