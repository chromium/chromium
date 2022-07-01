// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/offscreen/offscreen_document_manager.h"

#include "base/feature_list.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/offscreen_document_host.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_manager_factory.h"
#include "extensions/common/extension_features.h"
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
  return context;
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
    const GURL& url) {
  DCHECK(base::FeatureList::IsEnabled(
      extensions_features::kExtensionsOffscreenDocuments));
  DCHECK_EQ(url::Origin::Create(url), extension.origin());
  // Currently only a single offscreen document is supported per extension.
  DCHECK_EQ(nullptr, GetOffscreenDocumentForExtension(extension));
  DCHECK(!base::Contains(offscreen_documents_, extension.id()));

  OffscreenDocumentData& data = offscreen_documents_[extension.id()];

  scoped_refptr<content::SiteInstance> site_instance =
      process_manager_->GetSiteInstanceForURL(url);
  data.host = std::make_unique<OffscreenDocumentHost>(extension,
                                                      site_instance.get(), url);
  OffscreenDocumentHost* host = data.host.get();
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

void OffscreenDocumentManager::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  // Close any offscreen document associated with the unloaded extension.
  offscreen_documents_.erase(extension->id());
}

}  // namespace extensions
