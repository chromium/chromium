// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_message_filter.h"

#include "components/keyed_service/content/browser_context_keyed_service_shutdown_notifier_factory.h"
#include "components/keyed_service/core/keyed_service_shutdown_notifier.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_manager_factory.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/manifest_handlers/background_info.h"

using content::BrowserThread;

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
    DependsOn(ProcessManagerFactory::GetInstance());
  }
  ~ShutdownNotifierFactory() override {}

  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override {
    return ExtensionsBrowserClient::Get()->GetContextOwnInstance(
        context, /*force_guest_profile=*/true);
  }
};

}  // namespace

ExtensionMessageFilter::ExtensionMessageFilter(int render_process_id,
                                               content::BrowserContext* context)
    : BrowserMessageFilter(ExtensionMsgStart),
      render_process_id_(render_process_id),
      browser_context_(context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  shutdown_notifier_subscription_ =
      ShutdownNotifierFactory::GetInstance()->Get(context)->Subscribe(
          base::BindRepeating(&ExtensionMessageFilter::ShutdownOnUIThread,
                              base::Unretained(this)));
}

void ExtensionMessageFilter::EnsureShutdownNotifierFactoryBuilt() {
  ShutdownNotifierFactory::GetInstance();
}

ExtensionMessageFilter::~ExtensionMessageFilter() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void ExtensionMessageFilter::ShutdownOnUIThread() {
  browser_context_ = nullptr;
  shutdown_notifier_subscription_ = {};
}

void ExtensionMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message,
    BrowserThread::ID* thread) {
  switch (message.type()) {
    case ExtensionHostMsg_WakeEventPage::ID:
      *thread = BrowserThread::UI;
      break;
    default:
      break;
  }
}

void ExtensionMessageFilter::OnDestruct() const {
  BrowserThread::DeleteOnUIThread::Destruct(this);
}

bool ExtensionMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ExtensionMessageFilter, message)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_WakeEventPage,
                        OnExtensionWakeEventPage)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ExtensionMessageFilter::OnExtensionWakeEventPage(
    int request_id,
    const std::string& extension_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!browser_context_)
    return;

  const Extension* extension = ExtensionRegistry::Get(browser_context_)
                                   ->enabled_extensions()
                                   .GetByID(extension_id);
  if (!extension) {
    // Don't kill the renderer, it might just be some context which hasn't
    // caught up to extension having been uninstalled.
    return;
  }

  ProcessManager* process_manager = ProcessManager::Get(browser_context_);

  if (BackgroundInfo::HasLazyBackgroundPage(extension)) {
    // Wake the event page if it's asleep, or immediately repond with success
    // if it's already awake.
    if (process_manager->IsEventPageSuspended(extension_id)) {
      process_manager->WakeEventPage(
          extension_id,
          base::BindOnce(&ExtensionMessageFilter::SendWakeEventPageResponse,
                         this, request_id));
    } else {
      SendWakeEventPageResponse(request_id, true);
    }
    return;
  }

  if (BackgroundInfo::HasPersistentBackgroundPage(extension)) {
    // No point in trying to wake a persistent background page. If it's open,
    // immediately return and call it a success. If it's closed, fail.
    SendWakeEventPageResponse(request_id,
                              process_manager->GetBackgroundHostForExtension(
                                  extension_id) != nullptr);
    return;
  }

  // The extension has no background page, so there is nothing to wake.
  SendWakeEventPageResponse(request_id, false);
}

void ExtensionMessageFilter::SendWakeEventPageResponse(int request_id,
                                                       bool success) {
  Send(new ExtensionMsg_WakeEventPageResponse(request_id, success));
}

}  // namespace extensions
