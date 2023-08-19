// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_service_worker_message_filter.h"

#include "base/functional/bind.h"
#include "base/memory/singleton.h"
#include "base/unguessable_token.h"
#include "components/keyed_service/content/browser_context_keyed_service_shutdown_notifier_factory.h"
#include "components/keyed_service/core/keyed_service_shutdown_notifier.h"
#include "content/public/browser/service_worker_context.h"
#include "extensions/browser/bad_message.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/events/event_ack_data.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_manager_factory.h"
#include "extensions/common/extension_messages.h"

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
            "ExtensionServiceWorkerMessageFilter") {
    DependsOn(ExtensionRegistryFactory::GetInstance());
    DependsOn(EventRouterFactory::GetInstance());
    DependsOn(ProcessManagerFactory::GetInstance());
  }
  ~ShutdownNotifierFactory() override = default;

  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override {
    return ExtensionsBrowserClient::Get()->GetContextOwnInstance(
        context, /*force_guest_profile=*/true);
  }
};

}  // namespace

ExtensionServiceWorkerMessageFilter::ExtensionServiceWorkerMessageFilter(
    int render_process_id,
    content::BrowserContext* context,
    content::ServiceWorkerContext* service_worker_context)
    : content::BrowserMessageFilter(ExtensionWorkerMsgStart),
      browser_context_(context),
      render_process_id_(render_process_id),
      service_worker_context_(service_worker_context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  shutdown_notifier_subscription_ =
      ShutdownNotifierFactory::GetInstance()->Get(context)->Subscribe(
          base::BindRepeating(
              &ExtensionServiceWorkerMessageFilter::ShutdownOnUIThread,
              base::Unretained(this)));
}

void ExtensionServiceWorkerMessageFilter::ShutdownOnUIThread() {
  browser_context_ = nullptr;
  shutdown_notifier_subscription_ = {};
}

void ExtensionServiceWorkerMessageFilter::OnDestruct() const {
  content::BrowserThread::DeleteOnUIThread::Destruct(this);
}

void ExtensionServiceWorkerMessageFilter::EnsureShutdownNotifierFactoryBuilt() {
  ShutdownNotifierFactory::GetInstance();
}

ExtensionServiceWorkerMessageFilter::~ExtensionServiceWorkerMessageFilter() =
    default;

void ExtensionServiceWorkerMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message,
    content::BrowserThread::ID* thread) {
  if (message.type() == ExtensionHostMsg_EventAckWorker::ID) {
    *thread = content::BrowserThread::UI;
  }
}

bool ExtensionServiceWorkerMessageFilter::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ExtensionServiceWorkerMessageFilter, message)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_EventAckWorker, OnEventAckWorker)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ExtensionServiceWorkerMessageFilter::OnEventAckWorker(
    const ExtensionId& extension_id,
    int64_t service_worker_version_id,
    int worker_thread_id,
    int event_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!browser_context_)
    return;
  const bool worker_stopped =
      !ProcessManager::Get(browser_context_)
           ->HasServiceWorker({extension_id, render_process_id_,
                               service_worker_version_id, worker_thread_id});
  EventRouter::Get(browser_context_)
      ->event_ack_data()
      ->DecrementInflightEvent(
          service_worker_context_, render_process_id_,
          service_worker_version_id, event_id, worker_stopped,
          base::BindOnce(&ExtensionServiceWorkerMessageFilter::
                             DidFailDecrementInflightEvent,
                         this));
}

void ExtensionServiceWorkerMessageFilter::DidFailDecrementInflightEvent() {
  bad_message::ReceivedBadMessage(this, bad_message::ESWMF_BAD_EVENT_ACK);
}

}  // namespace extensions
