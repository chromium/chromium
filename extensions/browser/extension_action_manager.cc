// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_action_manager.h"

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_icon_image.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/constants.h"
#include "ui/gfx/image/image_skia.h"

namespace extensions {

namespace {

// BrowserContextKeyedServiceFactory for ExtensionActionManager.
class ExtensionActionManagerFactory : public BrowserContextKeyedServiceFactory {
 public:
  // BrowserContextKeyedServiceFactory implementation:
  static ExtensionActionManager* GetForBrowserContext(
      content::BrowserContext* context) {
    return static_cast<ExtensionActionManager*>(
        GetInstance()->GetServiceForBrowserContext(context, true));
  }

  static ExtensionActionManagerFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<ExtensionActionManagerFactory>;

  ExtensionActionManagerFactory()
      : BrowserContextKeyedServiceFactory(
            "ExtensionActionManager",
            BrowserContextDependencyManager::GetInstance()) {}

  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* browser_context) const override {
    return new ExtensionActionManager(browser_context);
  }

  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override {
    return ExtensionsBrowserClient::Get()->GetContextRedirectedToOriginal(
        context, /*force_guest_profile=*/true);
  }
};

ExtensionActionManagerFactory* ExtensionActionManagerFactory::GetInstance() {
  return base::Singleton<ExtensionActionManagerFactory>::get();
}

}  // namespace

ExtensionActionManager::ExtensionActionManager(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
  CHECK(!browser_context_->IsOffTheRecord())
      << "Don't instantiate this with an off-the-record context.";
  extension_registry_observation_.Observe(
      ExtensionRegistry::Get(browser_context_));
}

ExtensionActionManager::~ExtensionActionManager() {
  // Don't assert that the ExtensionAction maps are empty because Extensions
  // are sometimes (only in tests?) not unloaded before the associated context
  // is destroyed.
}

ExtensionActionManager* ExtensionActionManager::Get(
    content::BrowserContext* context) {
  return ExtensionActionManagerFactory::GetForBrowserContext(context);
}

void ExtensionActionManager::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  actions_.erase(extension->id());
}

ExtensionAction* ExtensionActionManager::GetExtensionAction(
    const Extension& extension) const {
  auto iter = actions_.find(extension.id());
  if (iter != actions_.end()) {
    return iter->second.get();
  }

  const ActionInfo* action_info =
      ActionInfo::GetExtensionActionInfo(&extension);
  if (!action_info) {
    return nullptr;
  }

  // Only create action info for enabled extensions.
  // This avoids bugs where actions are recreated just after being removed
  // in response to OnExtensionUnloaded().
  if (!ExtensionRegistry::Get(browser_context_)
           ->enabled_extensions()
           .Contains(extension.id())) {
    return nullptr;
  }

  auto action = std::make_unique<ExtensionAction>(extension, *action_info);

  if (action->default_icon()) {
    action->SetDefaultIconImage(std::make_unique<IconImage>(
        browser_context_, &extension, *action->default_icon(),
        ExtensionAction::ActionIconSize(),
        ExtensionAction::FallbackIcon().AsImageSkia(), nullptr));
  }

  ExtensionAction* raw_action = action.get();
  actions_[extension.id()] = std::move(action);
  return raw_action;
}

// static
void ExtensionActionManager::EnsureFactoryBuilt() {
  ExtensionActionManagerFactory::GetInstance();
}

}  // namespace extensions
