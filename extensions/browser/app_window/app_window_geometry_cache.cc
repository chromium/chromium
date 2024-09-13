// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/app_window/app_window_geometry_cache.h"

#include <stdint.h>

#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/json/values_util.h"
#include "base/not_fatal_until.h"
#include "base/observer_list.h"
#include "base/strings/string_number_conversions.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "ui/base/mojom/window_show_state.mojom.h"

namespace {

// The timeout in milliseconds before we'll persist window geometry to the
// StateStore.
const int kSyncTimeoutMilliseconds = 1000;

}  // namespace

namespace extensions {

AppWindowGeometryCache::AppWindowGeometryCache(content::BrowserContext* context,
                                               ExtensionPrefs* prefs)
    : prefs_(prefs), sync_delay_(base::Milliseconds(kSyncTimeoutMilliseconds)) {
  extension_registry_observation_.Observe(ExtensionRegistry::Get(context));
}

AppWindowGeometryCache::~AppWindowGeometryCache() = default;

// static
AppWindowGeometryCache* AppWindowGeometryCache::Get(
    content::BrowserContext* context) {
  return Factory::GetForContext(context, true /* create */);
}

void AppWindowGeometryCache::SaveGeometry(
    const ExtensionId& extension_id,
    const std::string& window_id,
    const gfx::Rect& bounds,
    const gfx::Rect& screen_bounds,
    ui::mojom::WindowShowState window_state) {
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
  std::set<ExtensionId> tosync;
  tosync.swap(unsynced_extensions_);
  for (auto sync_it = tosync.cbegin(), sync_eit = tosync.cend();
       sync_it != sync_eit; ++sync_it) {
    const ExtensionId& extension_id = *sync_it;
    const ExtensionData& extension_data = cache_[extension_id];

    base::Value::Dict dict;
    for (auto data_it = extension_data.cbegin(),
              data_eit = extension_data.cend();
         data_it != data_eit; ++data_it) {
      base::Value::Dict value;
      const gfx::Rect& bounds = data_it->second.bounds;
      const gfx::Rect& screen_bounds = data_it->second.screen_bounds;
      DCHECK(!bounds.IsEmpty());
      DCHECK(!screen_bounds.IsEmpty());
      DCHECK(data_it->second.window_state !=
             ui::mojom::WindowShowState::kDefault);
      value.Set("x", bounds.x());
      value.Set("y", bounds.y());
      value.Set("w", bounds.width());
      value.Set("h", bounds.height());
      value.Set("screen_bounds_x", screen_bounds.x());
      value.Set("screen_bounds_y", screen_bounds.y());
      value.Set("screen_bounds_w", screen_bounds.width());
      value.Set("screen_bounds_h", screen_bounds.height());
      value.Set("state", static_cast<int>(data_it->second.window_state));
      value.Set("ts", base::TimeToValue(data_it->second.last_change));
      dict.Set(data_it->first, std::move(value));

      for (auto& observer : observers_)
        observer.OnGeometryCacheChanged(extension_id, data_it->first, bounds);
    }

    prefs_->SetGeometryCache(extension_id, std::move(dict));
  }
}

bool AppWindowGeometryCache::GetGeometry(
    const ExtensionId& extension_id,
    const std::string& window_id,
    gfx::Rect* bounds,
    gfx::Rect* screen_bounds,
    ui::mojom::WindowShowState* window_state) {
  std::map<ExtensionId, ExtensionData>::const_iterator extension_data_it =
      cache_.find(extension_id);

  // Not in the map means loading data for the extension didn't finish yet or
  // the cache was not constructed until after the extension was loaded.
  // Attempt to load from sync to address the latter case.
  if (extension_data_it == cache_.end()) {
    LoadGeometryFromStorage(extension_id);
    extension_data_it = cache_.find(extension_id);
    CHECK(extension_data_it != cache_.end(), base::NotFatalUntil::M130);
  }

  auto window_data_it = extension_data_it->second.find(window_id);

  if (window_data_it == extension_data_it->second.end())
    return false;

  const WindowData& window_data = window_data_it->second;

  // Check for and do not return corrupt data.
  if ((bounds && window_data.bounds.IsEmpty()) ||
      (screen_bounds && window_data.screen_bounds.IsEmpty()) ||
      (window_state &&
       window_data.window_state == ui::mojom::WindowShowState::kDefault)) {
    return false;
  }

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
    : window_state(ui::mojom::WindowShowState::kDefault) {}

AppWindowGeometryCache::WindowData::~WindowData() = default;

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
  sync_delay_ = base::Milliseconds(timeout_ms);
}

void AppWindowGeometryCache::LoadGeometryFromStorage(
    const ExtensionId& extension_id) {
  ExtensionData& extension_data = cache_[extension_id];

  const base::Value::Dict* stored_windows =
      prefs_->GetGeometryCache(extension_id);
  if (!stored_windows)
    return;

  for (const auto item : *stored_windows) {
    // If the cache already contains geometry for this window, don't
    // overwrite that information since it is probably the result of an
    // application starting up very quickly.
    const std::string& window_id = item.first;
    if (extension_data.find(window_id) != extension_data.end())
      continue;

    const base::Value::Dict* stored_window = item.second.GetIfDict();
    if (!stored_window)
      continue;

    WindowData& window_data = extension_data[window_id];
    if (std::optional<int> i = stored_window->FindInt("x")) {
      window_data.bounds.set_x(*i);
    }
    if (std::optional<int> i = stored_window->FindInt("y")) {
      window_data.bounds.set_y(*i);
    }
    if (std::optional<int> i = stored_window->FindInt("w")) {
      window_data.bounds.set_width(*i);
    }
    if (std::optional<int> i = stored_window->FindInt("h")) {
      window_data.bounds.set_height(*i);
    }
    if (std::optional<int> i = stored_window->FindInt("screen_bounds_x")) {
      window_data.screen_bounds.set_x(*i);
    }
    if (std::optional<int> i = stored_window->FindInt("screen_bounds_y")) {
      window_data.screen_bounds.set_y(*i);
    }
    if (std::optional<int> i = stored_window->FindInt("screen_bounds_w")) {
      window_data.screen_bounds.set_width(*i);
    }
    if (std::optional<int> i = stored_window->FindInt("screen_bounds_h")) {
      window_data.screen_bounds.set_height(*i);
    }
    if (std::optional<int> i = stored_window->FindInt("state")) {
      window_data.window_state = static_cast<ui::mojom::WindowShowState>(*i);
    }
    if (const std::string* ts_as_string = stored_window->FindString("ts")) {
      int64_t ts;
      if (base::StringToInt64(*ts_as_string, &ts)) {
        window_data.last_change = base::Time::FromInternalValue(ts);
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

AppWindowGeometryCache::Factory::~Factory() = default;

std::unique_ptr<KeyedService>
AppWindowGeometryCache::Factory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<AppWindowGeometryCache>(context,
                                                  ExtensionPrefs::Get(context));
}

content::BrowserContext*
AppWindowGeometryCache::Factory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return ExtensionsBrowserClient::Get()->GetContextRedirectedToOriginal(
      context, /*force_guest_profile=*/true);
}

void AppWindowGeometryCache::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void AppWindowGeometryCache::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace extensions
