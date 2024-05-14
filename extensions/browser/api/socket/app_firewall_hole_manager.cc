// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/socket/app_firewall_hole_manager.h"

#include <utility>

#include "base/functional/bind.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/extension_id.h"

using content::BrowserContext;

namespace extensions {

namespace {

class AppFirewallHoleManagerFactory : public BrowserContextKeyedServiceFactory {
 public:
  static AppFirewallHoleManager* GetForBrowserContext(BrowserContext* context,
                                                      bool create) {
    return static_cast<AppFirewallHoleManager*>(
        GetInstance()->GetServiceForBrowserContext(context, create));
  }

  static AppFirewallHoleManagerFactory* GetInstance() {
    return base::Singleton<AppFirewallHoleManagerFactory>::get();
  }

  AppFirewallHoleManagerFactory()
      : BrowserContextKeyedServiceFactory(
            "AppFirewallHoleManager",
            BrowserContextDependencyManager::GetInstance()) {
    DependsOn(AppWindowRegistry::Factory::GetInstance());
  }

  ~AppFirewallHoleManagerFactory() override = default;

 private:
  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      BrowserContext* context) const override {
    return new AppFirewallHoleManager(context);
  }

  BrowserContext* GetBrowserContextToUse(
      BrowserContext* context) const override {
    return ExtensionsBrowserClient::Get()->GetContextOwnInstance(
        context, /*force_guest_profile=*/true);
  }
};

bool HasVisibleAppWindows(BrowserContext* context,
                          const ExtensionId& extension_id) {
  AppWindowRegistry* registry = AppWindowRegistry::Get(context);

  for (const AppWindow* window : registry->GetAppWindowsForApp(extension_id)) {
    if (!window->is_hidden()) {
      return true;
    }
  }

  return false;
}

}  // namespace

AppFirewallHole::~AppFirewallHole() {
  if (manager_) {
    manager_->Close(this);
  }
}

AppFirewallHole::AppFirewallHole(
    const base::WeakPtr<AppFirewallHoleManager>& manager,
    chromeos::FirewallHole::PortType type,
    uint16_t port,
    const ExtensionId& extension_id)
    : type_(type),
      port_(port),
      extension_id_(extension_id),
      manager_(manager) {}

void AppFirewallHole::SetVisible(bool app_visible) {
  app_visible_ = app_visible;
  if (app_visible_) {
    if (!firewall_hole_) {
      chromeos::FirewallHole::Open(
          type_, port_, "" /*all interfaces*/,
          base::BindOnce(&AppFirewallHole::OnFirewallHoleOpened,
                         weak_factory_.GetWeakPtr()));
    }
  } else {
    firewall_hole_.reset();
  }
}

void AppFirewallHole::OnFirewallHoleOpened(
    std::unique_ptr<chromeos::FirewallHole> firewall_hole) {
  if (app_visible_) {
    DCHECK(!firewall_hole_);
    firewall_hole_ = std::move(firewall_hole);
  }
}

AppFirewallHoleManager::AppFirewallHoleManager(BrowserContext* context)
    : context_(context) {
  observation_.Observe(AppWindowRegistry::Get(context));
}

AppFirewallHoleManager::~AppFirewallHoleManager() = default;

AppFirewallHoleManager* AppFirewallHoleManager::Get(BrowserContext* context) {
  return AppFirewallHoleManagerFactory::GetForBrowserContext(context, true);
}

std::unique_ptr<AppFirewallHole> AppFirewallHoleManager::Open(
    chromeos::FirewallHole::PortType type,
    uint16_t port,
    const ExtensionId& extension_id) {
  auto hole = base::WrapUnique(new AppFirewallHole(weak_factory_.GetWeakPtr(),
                                                   type, port, extension_id));
  tracked_holes_.emplace(extension_id, hole.get());
  if (HasVisibleAppWindows(context_, extension_id)) {
    hole->SetVisible(true);
  }
  return hole;
}

void AppFirewallHoleManager::Close(AppFirewallHole* hole) {
  auto range = tracked_holes_.equal_range(hole->extension_id());
  for (auto iter = range.first; iter != range.second; ++iter) {
    if (iter->second == hole) {
      tracked_holes_.erase(iter);
      return;
    }
  }
  NOTREACHED_IN_MIGRATION();
}

void AppFirewallHoleManager::OnAppWindowRemoved(AppWindow* app_window) {
  OnAppWindowHidden(app_window);
}

void AppFirewallHoleManager::OnAppWindowHidden(AppWindow* app_window) {
  DCHECK(context_ == app_window->browser_context());
  if (!HasVisibleAppWindows(context_, app_window->extension_id())) {
    const auto& range = tracked_holes_.equal_range(app_window->extension_id());
    for (auto iter = range.first; iter != range.second; ++iter) {
      iter->second->SetVisible(false);
    }
  }
}

void AppFirewallHoleManager::OnAppWindowShown(AppWindow* app_window,
                                              bool was_hidden) {
  const auto& range = tracked_holes_.equal_range(app_window->extension_id());
  for (auto iter = range.first; iter != range.second; ++iter) {
    iter->second->SetVisible(true);
  }
}

// static
void AppFirewallHoleManager::EnsureFactoryBuilt() {
  AppFirewallHoleManagerFactory::GetInstance();
}

}  // namespace extensions
