// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/app_window/app_window_geometry_cache.h"

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/extension.h"

namespace {

// The timeout in milliseconds before we'll persist window geometry to the
// StateStore.
const int kSyncTimeoutMilliseconds = 1000;

}  // namespace

namespace extensions {

AppWindowGeometryCache::AppWindowGeometryCache(content::BrowserContext* context,
                                               ExtensionPrefs* prefs)
    : prefs_(prefs),
      sync_delay_(base::TimeDelta::FromMilliseconds(kSyncTimeoutMilliseconds)) {
  extension_registry_observer_.Add(ExtensionRegistry::Get(context));
}

AppWindowGeometryCache::~AppWindowGeometryCache() {}

// static
AppWindowGeometryCache* AppWindowGeometryCache::Get(
    content::BrowserContext* context) {
  return Factory::GetForContext(context, true /* create */);
}

void AppWindowGeometryCache::SaveGeometry(const std::string& extension_id,
                                          const std::string& window_id,
                                          const gfx::Rect& bounds,
                                          const gfx::Rect& screen_bounds,
                                          ui::WindowShowState window_state) {
  ExtensionData& extension_data = cache_[extension_id];

  // If we don't have any unsynced changes and this is a duplicate of what's
  // already in the cache, just ignore it.
  if (extension_data[window_id].bounds == bounds &&
      extension_data[window_id].window_state == window_state &&
      extension_data[window_id].screen_bounds == screen_bounds &&
      !base::Contains(unsynced_extensions_, extension_id))
    return;

  base::Time now = base::Time::Now();

  extension_data[window_id].bounds = bounds;
  extension_data[window_id].screen_bounds = screen_bounds;
  extension_data[window_id].window_state = window_state;
  extension_data[window_id].last_change = now;

  if (extension_data.size() > kMaxCachedWindows) {
    auto oldest = extension_data.end();
    // Too many windows in the cache, find the oldest one to remove.
    for (auto it = extension_data.begin(); it != extension_data.end(); ++it) {
      // Don't expunge the window that was just added.
      if (it->first == window_id)
        continue;

      // If time is in the future, reset it to now to minimize weirdness.
      if (it->second.last_change > now)
        it->second.last_change = now;

      if (oldest == extension_data.end() ||
          it->second.last_change < oldest->second.last_change)
        oldest = it;
    }
    extension_data.erase(oldest);
  }

  unsynced_extensions_.insert(extension_id);

  // We don't use Reset() because the timer may not yet be running.
  // (In that case Stop() is a no-op.)
  sync_timer_.Stop();
  sync_timer_.Start(
      FROM_HERE, sync_delay_, this, &AppWindowGeometryCache::SyncToStorage);
}

void AppWindowGeometryCache::SyncToStorage() {
  std::set<std::string> tosync;
  tosync.swap(unsynced_extensions_);
  for (auto it = tosync.cbegin(), eit = tosync.cend(); it != eit; ++it) {
    const std::string& extension_id = *it;
    const ExtensionData& extension_data = cache_[extension_id];

    std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue);
    for (auto it = extension_data.cbegin(), eit = extension_data.cend();
         it != eit; ++it) {
      std::unique_ptr<base::DictionaryValue> value =
          std::make_unique<base::DictionaryValue>();
      const gfx::Rect& bounds = it->second.bounds;
      const gfx::Rect& screen_bounds = it->second.screen_bounds;
      DCHECK(!bounds.IsEmpty());
      DCHECK(!screen_bounds.IsEmpty());
      DCHECK(it->second.window_state != ui::SHOW_STATE_DEFAULT);
      value->SetInteger("x", bounds.x());
      value->SetInteger("y", bounds.y());
      value->SetInteger("w", bounds.width());
      value->SetInteger("h", bounds.height());
      value->SetInteger("screen_bounds_x", screen_bounds.x());
      value->SetInteger("screen_bounds_y", screen_bounds.y());
      value->SetInteger("screen_bounds_w", screen_bounds.width());
      value->SetInteger("screen_bounds_h", screen_bounds.height());
      value->SetInteger("state", it->second.window_state);
      value->SetString(
          "ts", base::NumberToString(it->second.last_change.ToInternalValue()));
      dict->SetWithoutPathExpansion(it->first, std::move(value));

      for (auto& observer : observers_)
        observer.OnGeometryCacheChanged(extension_id, it->first, bounds);
    }

    prefs_->SetGeometryCache(extension_id, std::move(dict));
  }
}

bool AppWindowGeometryCache::GetGeometry(const std::string& extension_id,
                                         const std::string& window_id,
                                         gfx::Rect* bounds,
                                         gfx::Rect* screen_bounds,
                                         ui::WindowShowState* window_state) {
  std::map<std::string, ExtensionData>::const_iterator extension_data_it =
      cache_.find(extension_id);

  // Not in the map means loading data for the extension didn't finish yet or
  // the cache was not constructed until after the extension was loaded.
  // Attempt to load from sync to address the latter case.
  if (extension_data_it == cache_.end()) {
    LoadGeometryFromStorage(extension_id);
    extension_data_it = cache_.find(extension_id);
    DCHECK(extension_data_it != cache_.end());
  }

  auto window_data_it = extension_data_it->second.find(window_id);

  if (window_data_it == extension_data_it->second.end())
    return false;

  const WindowData& window_data = window_data_it->second;

  // Check for and do not return corrupt data.
  if ((bounds && window_data.bounds.IsEmpty()) ||
      (screen_bounds && window_data.screen_bounds.IsEmpty()) ||
      (window_state && window_data.window_state == ui::SHOW_STATE_DEFAULT))
    return false;

  if (bounds)
    *bounds = window_data.bounds;
  if (screen_bounds)
    *screen_bounds = window_data.screen_bounds;
  if (window_state)
    *window_state = window_data.window_state;
  return true;
}

void AppWindowGeometryCache::Shutdown() { SyncToStorage(); }

AppWindowGeometryCache::WindowData::WindowData()
    : window_state(ui::SHOW_STATE_DEFAULT) {}

AppWindowGeometryCache::WindowData::~WindowData() {}

void AppWindowGeometryCache::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  LoadGeometryFromStorage(extension->id());
}

void AppWindowGeometryCache::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  SyncToStorage();
  cache_.erase(extension->id());
}

void AppWindowGeometryCache::SetSyncDelayForTests(int timeout_ms) {
  sync_delay_ = base::TimeDelta::FromMilliseconds(timeout_ms);
}

void AppWindowGeometryCache::LoadGeometryFromStorage(
    const std::string& extension_id) {
  ExtensionData& extension_data = cache_[extension_id];

  const base::DictionaryValue* stored_windows =
      prefs_->GetGeometryCache(extension_id);
  if (!stored_windows)
    return;

  for (base::DictionaryValue::Iterator it(*stored_windows); !it.IsAtEnd();
       it.Advance()) {
    // If the cache already contains geometry for this window, don't
    // overwrite that information since it is probably the result of an
    // application starting up very quickly.
    const std::string& window_id = it.key();
    auto cached_window = extension_data.find(window_id);
    if (cached_window == extension_data.end()) {
      const base::DictionaryValue* stored_window;
      if (it.value().GetAsDictionary(&stored_window)) {
        WindowData& window_data = extension_data[it.key()];

        int i;
        if (stored_window->GetInteger("x", &i))
          window_data.bounds.set_x(i);
        if (stored_window->GetInteger("y", &i))
          window_data.bounds.set_y(i);
        if (stored_window->GetInteger("w", &i))
          window_data.bounds.set_width(i);
        if (stored_window->GetInteger("h", &i))
          window_data.bounds.set_height(i);
        if (stored_window->GetInteger("screen_bounds_x", &i))
          window_data.screen_bounds.set_x(i);
        if (stored_window->GetInteger("screen_bounds_y", &i))
          window_data.screen_bounds.set_y(i);
        if (stored_window->GetInteger("screen_bounds_w", &i))
          window_data.screen_bounds.set_width(i);
        if (stored_window->GetInteger("screen_bounds_h", &i))
          window_data.screen_bounds.set_height(i);
        if (stored_window->GetInteger("state", &i)) {
          window_data.window_state = static_cast<ui::WindowShowState>(i);
        }
        std::string ts_as_string;
        if (stored_window->GetString("ts", &ts_as_string)) {
          int64_t ts;
          if (base::StringToInt64(ts_as_string, &ts)) {
            window_data.last_change = base::Time::FromInternalValue(ts);
          }
        }
      }
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// Factory boilerplate

// static
AppWindowGeometryCache* AppWindowGeometryCache::Factory::GetForContext(
    content::BrowserContext* context,
    bool create) {
  return static_cast<AppWindowGeometryCache*>(
      GetInstance()->GetServiceForBrowserContext(context, create));
}

AppWindowGeometryCache::Factory*
AppWindowGeometryCache::Factory::GetInstance() {
  return base::Singleton<AppWindowGeometryCache::Factory>::get();
}

AppWindowGeometryCache::Factory::Factory()
    : BrowserContextKeyedServiceFactory(
          "AppWindowGeometryCache",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ExtensionPrefsFactory::GetInstance());
}

AppWindowGeometryCache::Factory::~Factory() {}

KeyedService* AppWindowGeometryCache::Factory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new AppWindowGeometryCache(context, ExtensionPrefs::Get(context));
}

bool AppWindowGeometryCache::Factory::ServiceIsNULLWhileTesting() const {
  return false;
}

content::BrowserContext*
AppWindowGeometryCache::Factory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return ExtensionsBrowserClient::Get()->GetOriginalContext(context);
}

void AppWindowGeometryCache::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void AppWindowGeometryCache::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace extensions
