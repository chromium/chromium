// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/messaging/messaging_api_message_filter.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/types/optional_util.h"
#include "components/keyed_service/content/browser_context_keyed_service_shutdown_notifier_factory.h"
#include "components/keyed_service/core/keyed_service_shutdown_notifier.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/api/messaging/channel_endpoint.h"
#include "extensions/browser/api/messaging/message_service.h"
#include "extensions/browser/bad_message.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_util.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/mojom/message_port.mojom-shared.h"
#include "extensions/common/trace_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_LEGACY_IPC)

using content::BrowserThread;
using content::RenderProcessHost;
using perfetto::protos::pbzero::ChromeTrackEvent;

namespace extensions {

namespace {

class ShutdownNotifierFactory
    : public BrowserContextKeyedServiceShutdownNotifierFactory {
 public:
  ShutdownNotifierFactory(const ShutdownNotifierFactory&) = delete;
  ShutdownNotifierFactory& operator=(const ShutdownNotifierFactory&) = delete;

  static ShutdownNotifierFactory* GetInstance() {
    return base::Singleton<ShutdownNotifierFactory>::get();
  }

 private:
  friend struct base::DefaultSingletonTraits<ShutdownNotifierFactory>;

  ShutdownNotifierFactory()
      : BrowserContextKeyedServiceShutdownNotifierFactory(
            "ExtensionMessageFilter") {
    DependsOn(EventRouterFactory::GetInstance());
  }
  ~ShutdownNotifierFactory() override = default;

  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override {
    return ExtensionsBrowserClient::Get()->GetContextOwnInstance(
        context, /*force_guest_profile=*/true);
  }
};

}  // namespace

MessagingAPIMessageFilter::MessagingAPIMessageFilter(
    int render_process_id,
    content::BrowserContext* context)
    : BrowserMessageFilter(ExtensionMsgStart),
      render_process_id_(render_process_id),
      browser_context_(context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  shutdown_notifier_subscription_ =
      ShutdownNotifierFactory::GetInstance()->Get(context)->Subscribe(
          base::BindRepeating(&MessagingAPIMessageFilter::Shutdown,
                              base::Unretained(this)));
}

MessagingAPIMessageFilter::~MessagingAPIMessageFilter() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void MessagingAPIMessageFilter::Shutdown() {
  browser_context_ = nullptr;
  shutdown_notifier_subscription_ = {};
}

content::RenderProcessHost* MessagingAPIMessageFilter::GetRenderProcessHost() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!browser_context_)
    return nullptr;

  // The IPC might race with RenderProcessHost destruction.  This may only
  // happen in scenarios that are already inherently racey, so returning nullptr
  // (and dropping the IPC) is okay and won't lead to any additional risk of
  // data loss.
  return content::RenderProcessHost::FromID(render_process_id_);
}

void MessagingAPIMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message,
    BrowserThread::ID* thread) {
  switch (message.type()) {
    case ExtensionHostMsg_OpenChannelToExtension::ID:
    case ExtensionHostMsg_OpenChannelToTab::ID:
    case ExtensionHostMsg_OpenChannelToNativeApp::ID:
    case ExtensionHostMsg_OpenMessagePort::ID:
    case ExtensionHostMsg_CloseMessagePort::ID:
    case ExtensionHostMsg_PostMessage::ID:
    case ExtensionHostMsg_ResponsePending::ID:
      *thread = BrowserThread::UI;
      break;
    default:
      break;
  }
}

void MessagingAPIMessageFilter::OnDestruct() const {
  BrowserThread::DeleteOnUIThread::Destruct(this);
}

bool MessagingAPIMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(MessagingAPIMessageFilter, message)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_OpenChannelToExtension,
                        OnOpenChannelToExtension)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_OpenChannelToTab, OnOpenChannelToTab)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_OpenChannelToNativeApp,
                        OnOpenChannelToNativeApp)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_OpenMessagePort, OnOpenMessagePort)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_CloseMessagePort, OnCloseMessagePort)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_PostMessage, OnPostMessage)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_ResponsePending, OnResponsePending)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void MessagingAPIMessageFilter::OnOpenChannelToExtension(
    const PortContext& source_context,
    const ExtensionMsg_ExternalConnectionInfo& info,
    mojom::ChannelType channel_type,
    const std::string& channel_name,
    const PortId& port_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* process = GetRenderProcessHost();
  if (!process)
    return;
  TRACE_EVENT("extensions", "MessageFilter::OnOpenChannelToExtension",
              ChromeTrackEvent::kRenderProcessHost, *process);
  if (source_context.is_for_native_host()) {
    bad_message::ReceivedBadMessage(
        process,
        bad_message::EMF_INVALID_OPEN_CHANNEL_TO_EXTENSION_FROM_NATIVE_HOST);
    return;
  }
  ChannelEndpoint source_endpoint(browser_context_, process->GetID(),
                                  source_context);
  MessageService::Get(browser_context_)
      ->OpenChannelToExtension(source_endpoint, port_id, info, channel_type,
                               channel_name, {}, {});
}

void MessagingAPIMessageFilter::OnOpenChannelToNativeApp(
    const PortContext& source_context,
    const std::string& native_app_name,
    const PortId& port_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* process = GetRenderProcessHost();
  if (!process)
    return;
  TRACE_EVENT("extensions", "MessageFilter::OnOpenChannelToNativeApp",
              ChromeTrackEvent::kRenderProcessHost, *process);
  if (source_context.is_for_native_host()) {
    bad_message::ReceivedBadMessage(
        process,
        bad_message::EMF_INVALID_OPEN_CHANNEL_TO_NATIVE_APP_FROM_NATIVE_HOST);
    return;
  }
  ChannelEndpoint source_endpoint(browser_context_, process->GetID(),
                                  source_context);
  MessageService::Get(browser_context_)
      ->OpenChannelToNativeApp(source_endpoint, port_id, native_app_name, {},
                               {});
}

void MessagingAPIMessageFilter::OnOpenChannelToTab(
    const PortContext& source_context,
    const ExtensionMsg_TabTargetConnectionInfo& info,
    mojom::ChannelType channel_type,
    const std::string& channel_name,
    const PortId& port_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* process = GetRenderProcessHost();
  if (!process)
    return;
  TRACE_EVENT("extensions", "MessageFilter::OnOpenChannelToTab",
              ChromeTrackEvent::kRenderProcessHost, *process);
  if (source_context.is_for_native_host()) {
    bad_message::ReceivedBadMessage(
        process, bad_message::EMF_NON_EXTENSION_SENDER_NATIVE_HOST);
    return;
  }
  ChannelEndpoint source_endpoint(browser_context_, process->GetID(),
                                  source_context);
  MessageService::Get(browser_context_)
      ->OpenChannelToTab(source_endpoint, port_id, info.tab_id, info.frame_id,
                         info.document_id, channel_type, channel_name, {}, {});
}

void MessagingAPIMessageFilter::OnOpenMessagePort(const PortContext& source,
                                                  const PortId& port_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* process = GetRenderProcessHost();
  if (!process) {
    return;
  }
  TRACE_EVENT("extensions", "MessageFilter::OnOpenMessagePort",
              ChromeTrackEvent::kRenderProcessHost, *process);
  MessageService::Get(browser_context_)->OpenPort(process, port_id, source);
}

void MessagingAPIMessageFilter::OnCloseMessagePort(
    const PortContext& port_context,
    const PortId& port_id,
    bool force_close) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* process = GetRenderProcessHost();
  if (!process) {
    return;
  }
  TRACE_EVENT("extensions", "MessageFilter::OnCloseMessagePort",
              ChromeTrackEvent::kRenderProcessHost, *process);

  MessageService::Get(browser_context_)
      ->ClosePort(process, port_id, port_context, force_close);
}

void MessagingAPIMessageFilter::OnPostMessage(const PortId& port_id,
                                              const Message& message) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!browser_context_)
    return;

  MessageService::Get(browser_context_)->PostMessage(port_id, message);
}

void MessagingAPIMessageFilter::OnResponsePending(
    const PortContext& port_context,
    const PortId& port_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* process = GetRenderProcessHost();
  if (!process) {
    return;
  }
  TRACE_EVENT("extensions", "MessageFilter::OnResponsePending",
              ChromeTrackEvent::kRenderProcessHost, *process);

  MessageService::Get(browser_context_)
      ->NotifyResponsePending(process, port_id, port_context);
}

// static
void MessagingAPIMessageFilter::EnsureAssociatedFactoryBuilt() {
  ShutdownNotifierFactory::GetInstance();
}

}  // namespace extensions

#endif
