// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/app_window/app_window_registry.h"

#include <string>
#include <vector>

#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/extension.h"

namespace extensions {

void AppWindowRegistry::Observer::OnAppWindowAdded(AppWindow* app_window) {
}

void AppWindowRegistry::Observer::OnAppWindowRemoved(AppWindow* app_window) {
}

void AppWindowRegistry::Observer::OnAppWindowHidden(AppWindow* app_window) {
}

void AppWindowRegistry::Observer::OnAppWindowShown(AppWindow* app_window,
                                                   bool was_hidden) {}

void AppWindowRegistry::Observer::OnAppWindowActivated(AppWindow* app_window) {
}

AppWindowRegistry::Observer::~Observer() {
}

AppWindowRegistry::AppWindowRegistry(content::BrowserContext* context)
    : context_(context) {
  content::DevToolsAgentHost::AddObserver(this);
}

AppWindowRegistry::~AppWindowRegistry() {
  content::DevToolsAgentHost::RemoveObserver(this);
}

// static
AppWindowRegistry* AppWindowRegistry::Get(content::BrowserContext* context) {
  return Factory::GetForBrowserContext(context, true /* create */);
}

void AppWindowRegistry::AddAppWindow(AppWindow* app_window) {
  BringToFront(app_window);
  for (auto& observer : observers_)
    observer.OnAppWindowAdded(app_window);
}

void AppWindowRegistry::AppWindowActivated(AppWindow* app_window) {
  BringToFront(app_window);
  for (auto& observer : observers_)
    observer.OnAppWindowActivated(app_window);
}

void AppWindowRegistry::AppWindowHidden(AppWindow* app_window) {
  for (auto& observer : observers_)
    observer.OnAppWindowHidden(app_window);
}

void AppWindowRegistry::AppWindowShown(AppWindow* app_window, bool was_hidden) {
  for (auto& observer : observers_)
    observer.OnAppWindowShown(app_window, was_hidden);
}

void AppWindowRegistry::RemoveAppWindow(AppWindow* app_window) {
  const AppWindowList::iterator it =
      std::find(app_windows_.begin(), app_windows_.end(), app_window);
  if (it != app_windows_.end())
    app_windows_.erase(it);
  for (auto& observer : observers_)
    observer.OnAppWindowRemoved(app_window);
}

void AppWindowRegistry::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

bool AppWindowRegistry::HasObserver(const Observer* observer) const {
  return observers_.HasObserver(observer);
}

void AppWindowRegistry::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

AppWindowRegistry::AppWindowList AppWindowRegistry::GetAppWindowsForApp(
    const std::string& app_id) const {
  AppWindowList app_windows;
  for (auto i = app_windows_.cbegin(); i != app_windows_.cend(); ++i) {
    if ((*i)->extension_id() == app_id)
      app_windows.push_back(*i);
  }
  return app_windows;
}

AppWindow* AppWindowRegistry::GetAppWindowForWebContents(
    const content::WebContents* web_contents) const {
  for (AppWindow* window : app_windows_) {
    if (window->web_contents() == web_contents)
      return window;
  }
  return nullptr;
}

AppWindow* AppWindowRegistry::GetAppWindowForNativeWindow(
    gfx::NativeWindow window) const {
  for (auto i = app_windows_.cbegin(); i != app_windows_.cend(); ++i) {
    if ((*i)->GetNativeWindow() == window)
      return *i;
  }

  return NULL;
}

AppWindow* AppWindowRegistry::GetCurrentAppWindowForApp(
    const std::string& app_id) const {
  AppWindow* result = NULL;
  for (auto i = app_windows_.cbegin(); i != app_windows_.cend(); ++i) {
    if ((*i)->extension_id() == app_id) {
      result = *i;
      if (result->GetBaseWindow()->IsActive())
        return result;
    }
  }

  return result;
}

AppWindow* AppWindowRegistry::GetAppWindowForAppAndKey(
    const std::string& app_id,
    const std::string& window_key) const {
  AppWindow* result = NULL;
  for (auto i = app_windows_.cbegin(); i != app_windows_.cend(); ++i) {
    if ((*i)->extension_id() == app_id && (*i)->window_key() == window_key) {
      result = *i;
      if (result->GetBaseWindow()->IsActive())
        return result;
    }
  }
  return result;
}

bool AppWindowRegistry::HadDevToolsAttached(
    content::WebContents* web_contents) const {
  std::string key = GetWindowKeyForWebContents(web_contents);
  return key.empty() ? false : inspected_windows_.count(key) != 0;
}

void AppWindowRegistry::DevToolsAgentHostAttached(
    content::DevToolsAgentHost* agent_host) {
  std::string key = GetWindowKeyForAgentHost(agent_host);
  if (!key.empty())
    inspected_windows_.insert(key);
}

void AppWindowRegistry::DevToolsAgentHostDetached(
    content::DevToolsAgentHost* agent_host) {
  std::string key = GetWindowKeyForAgentHost(agent_host);
  if (!key.empty())
    inspected_windows_.erase(key);
}

void AppWindowRegistry::AddAppWindowToList(AppWindow* app_window) {
  if (base::Contains(app_windows_, app_window))
    return;
  app_windows_.push_back(app_window);
}

void AppWindowRegistry::BringToFront(AppWindow* app_window) {
  const AppWindowList::iterator it =
      std::find(app_windows_.begin(), app_windows_.end(), app_window);
  if (it != app_windows_.end())
    app_windows_.erase(it);
  app_windows_.push_front(app_window);
}

std::string AppWindowRegistry::GetWindowKeyForAgentHost(
    content::DevToolsAgentHost* agent_host) const {
  content::WebContents* web_contents = agent_host->GetWebContents();
  if (!web_contents || web_contents->GetBrowserContext() != context_)
    return std::string();
  return GetWindowKeyForWebContents(web_contents);
}

std::string AppWindowRegistry::GetWindowKeyForWebContents(
    content::WebContents* web_contents) const {
  AppWindow* app_window = GetAppWindowForWebContents(web_contents);
  if (!app_window)
    return std::string();  // Not an AppWindow.

  if (app_window->window_key().empty())
    return web_contents->GetURL().possibly_invalid_spec();

  return base::StringPrintf("%s:%s", app_window->extension_id().c_str(),
                            app_window->window_key().c_str());
}

///////////////////////////////////////////////////////////////////////////////
// Factory boilerplate

// static
AppWindowRegistry* AppWindowRegistry::Factory::GetForBrowserContext(
    content::BrowserContext* context,
    bool create) {
  return static_cast<AppWindowRegistry*>(
      GetInstance()->GetServiceForBrowserContext(context, create));
}

AppWindowRegistry::Factory* AppWindowRegistry::Factory::GetInstance() {
  return base::Singleton<AppWindowRegistry::Factory>::get();
}

AppWindowRegistry::Factory::Factory()
    : BrowserContextKeyedServiceFactory(
          "AppWindowRegistry",
          BrowserContextDependencyManager::GetInstance()) {}

AppWindowRegistry::Factory::~Factory() {}

KeyedService* AppWindowRegistry::Factory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new AppWindowRegistry(context);
}

bool AppWindowRegistry::Factory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool AppWindowRegistry::Factory::ServiceIsNULLWhileTesting() const {
  return false;
}

content::BrowserContext* AppWindowRegistry::Factory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return ExtensionsBrowserClient::Get()->GetOriginalContext(context);
}

}  // namespace extensions
