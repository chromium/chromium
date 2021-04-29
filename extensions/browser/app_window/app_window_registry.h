// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_APP_WINDOW_APP_WINDOW_REGISTRY_H_
#define EXTENSIONS_BROWSER_APP_WINDOW_APP_WINDOW_REGISTRY_H_

#include <list>
#include <set>
#include <string>

#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/devtools_agent_host_observer.h"
#include "ui/gfx/native_widget_types.h"

namespace content {
class BrowserContext;
class DevToolsAgentHost;
class WebContents;
}

namespace extensions {

class AppWindow;

// The AppWindowRegistry tracks the AppWindows for all platform apps for a
// particular browser context.
class AppWindowRegistry : public KeyedService,
                          public content::DevToolsAgentHostObserver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called just after an app window was added.
    virtual void OnAppWindowAdded(AppWindow* app_window);
    // Called just after an app window was removed.
    virtual void OnAppWindowRemoved(AppWindow* app_window);
    // Called just after an app window was hidden. This is different from
    // window visibility as a minimize does not hide a window, but does make
    // it not visible.
    virtual void OnAppWindowHidden(AppWindow* app_window);
    // Called just after an app window was shown.
    // |was_hidden| will be true if the app window was considered hidden or if
    // it had not been shown before.
    virtual void OnAppWindowShown(AppWindow* app_window, bool was_hidden);
    // Called just after an app window was activated.
    virtual void OnAppWindowActivated(AppWindow* app_window);

   protected:
    ~Observer() override;
  };

  typedef std::list<AppWindow*> AppWindowList;
  typedef AppWindowList::const_iterator const_iterator;
  typedef std::set<std::string> InspectedWindowSet;

  explicit AppWindowRegistry(content::BrowserContext* context);
  ~AppWindowRegistry() override;

  // Returns the instance for the given browser context, or NULL if none. This
  // is a convenience wrapper around
  // AppWindowRegistry::Factory::GetForBrowserContext().
  static AppWindowRegistry* Get(content::BrowserContext* context);

  void AddAppWindow(AppWindow* app_window);
  // Called by |app_window| when it is activated.
  void AppWindowActivated(AppWindow* app_window);
  void AppWindowHidden(AppWindow* app_window);
  void AppWindowShown(AppWindow* app_window, bool was_hidden);
  void RemoveAppWindow(AppWindow* app_window);

  void AddObserver(Observer* observer);
  bool HasObserver(const Observer* observer) const;
  void RemoveObserver(Observer* observer);

  // Returns a set of windows owned by the application identified by app_id.
  AppWindowList GetAppWindowsForApp(const std::string& app_id) const;
  const AppWindowList& app_windows() const { return app_windows_; }

  // Helper functions to find app windows with particular attributes.
  AppWindow* GetAppWindowForWebContents(
      const content::WebContents* web_contents) const;
  AppWindow* GetAppWindowForNativeWindow(gfx::NativeWindow window) const;
  // Returns an app window for the given app, or NULL if no app windows are
  // open. If there is a window for the given app that is active, that one will
  // be returned, otherwise an arbitrary window will be returned.
  AppWindow* GetCurrentAppWindowForApp(const std::string& app_id) const;
  // Returns an app window for the given app and window key, or NULL if no app
  // window with the key are open. If there is a window for the given app and
  // key that is active, that one will be returned, otherwise an arbitrary
  // window will be returned.
  AppWindow* GetAppWindowForAppAndKey(const std::string& app_id,
                                      const std::string& window_key) const;

  // Returns whether a AppWindow's ID was last known to have a DevToolsAgent
  // attached to it, which should be restored during a reload of a corresponding
  // newly created |web_contents|.
  bool HadDevToolsAttached(content::WebContents* web_contents) const;

  class Factory : public BrowserContextKeyedServiceFactory {
   public:
    static AppWindowRegistry* GetForBrowserContext(
        content::BrowserContext* context,
        bool create);

    static Factory* GetInstance();

   private:
    friend struct base::DefaultSingletonTraits<Factory>;

    Factory();
    ~Factory() override;

    // BrowserContextKeyedServiceFactory
    KeyedService* BuildServiceInstanceFor(
        content::BrowserContext* context) const override;
    bool ServiceIsCreatedWithBrowserContext() const override;
    content::BrowserContext* GetBrowserContextToUse(
        content::BrowserContext* context) const override;
  };

 private:
  // Ensures the specified |app_window| is included in |app_windows_|.
  // Otherwise adds |app_window| to the back of |app_windows_|.
  void AddAppWindowToList(AppWindow* app_window);

  // Bring |app_window| to the front of |app_windows_|. If it is not in the
  // list, add it first.
  void BringToFront(AppWindow* app_window);

  // Create a key that identifies an AppWindow across App reloads. If the window
  // was given an id in CreateParams, the key is the extension id, a colon
  // separator, and the AppWindow's |id|. If there is no |id|, the
  // chrome-extension://extension-id/page.html URL will be used. If the
  // WebContents is not for a AppWindow, return an empty string.
  std::string GetWindowKeyForWebContents(
      content::WebContents* web_contents) const;
  std::string GetWindowKeyForAgentHost(
      content::DevToolsAgentHost* agent_host) const;

  // content::DevToolsAgentHostObserver overrides.
  void DevToolsAgentHostAttached(
      content::DevToolsAgentHost* agent_host) override;
  void DevToolsAgentHostDetached(
      content::DevToolsAgentHost* agent_host) override;

  content::BrowserContext* context_;
  AppWindowList app_windows_;
  InspectedWindowSet inspected_windows_;
  base::ObserverList<Observer> observers_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_APP_WINDOW_APP_WINDOW_REGISTRY_H_
