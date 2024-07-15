// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/offscreen/offscreen_document_manager.h"

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/dcheck_is_on.h"
#include "base/not_fatal_until.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/api/offscreen/lifetime_enforcer_factories.h"
#include "extensions/browser/api/offscreen/offscreen_document_lifetime_enforcer.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/offscreen_document_host.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_manager_factory.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace extensions {

namespace {

class OffscreenDocumentManagerFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  OffscreenDocumentManagerFactory();
  OffscreenDocumentManagerFactory(const OffscreenDocumentManagerFactory&) =
      delete;
  OffscreenDocumentManagerFactory& operator=(
      const OffscreenDocumentManagerFactory&) = delete;
  ~OffscreenDocumentManagerFactory() override = default;

  OffscreenDocumentManager* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  // BrowserContextKeyedServiceFactory:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

OffscreenDocumentManagerFactory::OffscreenDocumentManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "OffscreenDocumentManager",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ExtensionRegistryFactory::GetInstance());
  DependsOn(ProcessManagerFactory::GetInstance());
}

OffscreenDocumentManager* OffscreenDocumentManagerFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<OffscreenDocumentManager*>(
      GetServiceForBrowserContext(browser_context, /*create=*/true));
}

content::BrowserContext*
OffscreenDocumentManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Use the `context` passed in; this service has separate instances in
  // on-the-record and incognito.
  return ExtensionsBrowserClient::Get()->GetContextOwnInstance(
      context, /*force_guest_profile=*/true);
}

KeyedService* OffscreenDocumentManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new OffscreenDocumentManager(context);
}

}  // namespace

// OffscreenDocumentManager::OffscreenDocumentData:
OffscreenDocumentManager::OffscreenDocumentData::OffscreenDocumentData() =
    default;
OffscreenDocumentManager::OffscreenDocumentData::~OffscreenDocumentData() =
    default;
OffscreenDocumentManager::OffscreenDocumentData::OffscreenDocumentData(
    OffscreenDocumentData&&) = default;

// OffscreenDocumentManager:
OffscreenDocumentManager::OffscreenDocumentManager(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context),
      process_manager_(ProcessManager::Get(browser_context_)) {
  registry_observation_.Observe(ExtensionRegistry::Get(browser_context_));
}

OffscreenDocumentManager::~OffscreenDocumentManager() = default;

// static
OffscreenDocumentManager* OffscreenDocumentManager::Get(
    content::BrowserContext* browser_context) {
  return static_cast<OffscreenDocumentManagerFactory*>(GetFactory())
      ->GetForBrowserContext(browser_context);
}

// static
BrowserContextKeyedServiceFactory* OffscreenDocumentManager::GetFactory() {
  static base::NoDestructor<OffscreenDocumentManagerFactory> g_factory;
  return g_factory.get();
}

OffscreenDocumentHost* OffscreenDocumentManager::CreateOffscreenDocument(
    const Extension& extension,
    const GURL& url,
    std::set<api::offscreen::Reason> reasons) {
  DCHECK_EQ(url::Origin::Create(url), extension.origin());
  // Currently only a single offscreen document is supported per extension.
  DCHECK_EQ(nullptr, GetOffscreenDocumentForExtension(extension));
  DCHECK(!base::Contains(offscreen_documents_, extension.id()));
  CHECK(!base::Contains(reasons, api::offscreen::Reason::kNone));
#if DCHECK_IS_ON()
  // This should only be for an off-the-record context if the extension is both
  // enabled in incognito *and* runs in split mode. For spanning mode
  // extensions, similar to the background context, offscreen documents only run
  // in the on-the-record context.
  if (browser_context_->IsOffTheRecord()) {
    DCHECK(util::IsIncognitoEnabled(extension.id(), browser_context_));
    DCHECK(IncognitoInfo::IsSplitMode(&extension));
  }
#endif

  OffscreenDocumentData& data = offscreen_documents_[extension.id()];

  scoped_refptr<content::SiteInstance> site_instance =
      process_manager_->GetSiteInstanceForURL(url);
  data.host = std::make_unique<OffscreenDocumentHost>(extension,
                                                      site_instance.get(), url);
  OffscreenDocumentHost* host = data.host.get();

  // The following Unretained()s are safe because this class owns the offscreen
  // document host and lifetime enforcer, so these callbacks can't possibly be
  // called after the manager's destruction.
  auto notify_inactive_callback = base::BindRepeating(
      &OffscreenDocumentManager::OnOffscreenDocumentActivityChanged,
      base::Unretained(this), extension.id());

  for (auto reason : reasons) {
    auto termination_callback = base::BindOnce(
        &OffscreenDocumentManager::CloseOffscreenDocumentForExtensionId,
        base::Unretained(this), extension.id());
    data.enforcers.push_back(LifetimeEnforcerFactories::GetLifetimeEnforcer(
        reason, host, std::move(termination_callback),
        notify_inactive_callback));
  }

  host->SetCloseHandler(
      base::BindOnce(&OffscreenDocumentManager::CloseOffscreenDocument,
                     base::Unretained(this)));
  host->CreateRendererSoon();

  return host;
}

OffscreenDocumentHost*
OffscreenDocumentManager::GetOffscreenDocumentForExtension(
    const Extension& extension) {
  auto iter = offscreen_documents_.find(extension.id());
  if (iter == offscreen_documents_.end())
    return nullptr;
  return iter->second.host.get();
}

void OffscreenDocumentManager::CloseOffscreenDocumentForExtension(
    const Extension& extension) {
  CloseOffscreenDocumentForExtensionId(extension.id());
}

void OffscreenDocumentManager::CloseOffscreenDocumentForExtensionId(
    const ExtensionId& extension_id) {
  DCHECK(base::Contains(offscreen_documents_, extension_id));
  offscreen_documents_.erase(extension_id);
}

void OffscreenDocumentManager::OnOffscreenDocumentActivityChanged(
    const ExtensionId& extension_id) {
  DCHECK(base::Contains(offscreen_documents_, extension_id));
  OffscreenDocumentData& data = offscreen_documents_[extension_id];
  DCHECK(data.host);

  bool any_active = false;
  for (auto& enforcer : data.enforcers) {
    if (enforcer->IsActive()) {
      any_active = true;
      break;
    }
  }

  if (!any_active)
    CloseOffscreenDocumentForExtensionId(extension_id);
}

void OffscreenDocumentManager::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  // Close any offscreen document associated with the unloaded extension.
  offscreen_documents_.erase(extension->id());
}

void OffscreenDocumentManager::Shutdown() {
  // This would normally happen during destruction, but that isn't sufficient -
  // we need to close all the corresponding ExtensionHosts
  // (OffscreenDocumentHosts) prior to the browser context being marked as
  // shut down.
  offscreen_documents_.clear();
}

void OffscreenDocumentManager::CloseOffscreenDocument(
    ExtensionHost* offscreen_document) {
  auto iter = offscreen_documents_.find(offscreen_document->extension_id());
  CHECK(iter != offscreen_documents_.end(), base::NotFatalUntil::M130);
  DCHECK_EQ(iter->second.host.get(), offscreen_document);
  offscreen_documents_.erase(iter);
}

}  // namespace extensions
