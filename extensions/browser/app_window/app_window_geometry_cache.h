// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_APP_WINDOW_APP_WINDOW_GEOMETRY_CACHE_H_
#define EXTENSIONS_BROWSER_APP_WINDOW_APP_WINDOW_GEOMETRY_CACHE_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "base/scoped_observer.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/rect.h"

namespace extensions {

class ExtensionPrefs;

// A cache for persisted geometry of app windows, both to not have to wait
// for IO when creating a new window, and to not cause IO on every window
// geometry change.
class AppWindowGeometryCache : public KeyedService,
                               public ExtensionRegistryObserver {
 public:
  class Factory : public BrowserContextKeyedServiceFactory {
   public:
    static AppWindowGeometryCache* GetForContext(
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
    content::BrowserContext* GetBrowserContextToUse(
        content::BrowserContext* context) const override;
  };

  class Observer {
   public:
    virtual void OnGeometryCacheChanged(const std::string& extension_id,
                                        const std::string& window_id,
                                        const gfx::Rect& bounds) = 0;

   protected:
    virtual ~Observer() {}
  };

  AppWindowGeometryCache(content::BrowserContext* context,
                         ExtensionPrefs* prefs);

  ~AppWindowGeometryCache() override;

  // Returns the instance for the given browsing context.
  static AppWindowGeometryCache* Get(content::BrowserContext* context);

  // Save the geometry and state associated with |extension_id| and |window_id|.
  void SaveGeometry(const std::string& extension_id,
                    const std::string& window_id,
                    const gfx::Rect& bounds,
                    const gfx::Rect& screen_bounds,
                    ui::WindowShowState state);

  // Get any saved geometry and state associated with |extension_id| and
  // |window_id|. If saved data exists, sets |bounds|, |screen_bounds| and
  // |state| if not NULL and returns true.
  bool GetGeometry(const std::string& extension_id,
                   const std::string& window_id,
                   gfx::Rect* bounds,
                   gfx::Rect* screen_bounds,
                   ui::WindowShowState* state);

  // KeyedService
  void Shutdown() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Maximum number of windows we'll cache the geometry for per app.
  static const size_t kMaxCachedWindows = 100;

 protected:
  friend class AppWindowGeometryCacheTest;

  // For tests, this modifies the timeout delay for saving changes from calls
  // to SaveGeometry. (Note that even if this is set to 0, you still need to
  // run the message loop to see the results of any SyncToStorage call).
  void SetSyncDelayForTests(int timeout_ms);

 private:
  // Data stored for each window.
  struct WindowData {
    WindowData();
    ~WindowData();
    gfx::Rect bounds;
    gfx::Rect screen_bounds;
    ui::WindowShowState window_state;
    base::Time last_change;
  };

  // Data stored for each extension.
  typedef std::map<std::string, WindowData> ExtensionData;

  // ExtensionRegistryObserver implementation.
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;

  void LoadGeometryFromStorage(const std::string& extension_id);
  void SyncToStorage();

  // Preferences storage.
  ExtensionPrefs* prefs_;

  // Cached data.
  std::map<std::string, ExtensionData> cache_;

  // Data that still needs saving.
  std::set<std::string> unsynced_extensions_;

  // The timer used to save the data.
  base::OneShotTimer sync_timer_;

  // The timeout value we'll use for |sync_timer_|.
  base::TimeDelta sync_delay_;

  // Listen to extension load, unloaded notifications.
  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_{this};

  base::ObserverList<Observer>::Unchecked observers_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_APP_WINDOW_APP_WINDOW_GEOMETRY_CACHE_H_
