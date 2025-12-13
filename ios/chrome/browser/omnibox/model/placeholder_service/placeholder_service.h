// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_MODEL_PLACEHOLDER_SERVICE_PLACEHOLDER_SERVICE_H_
#define IOS_CHROME_BROWSER_OMNIBOX_MODEL_PLACEHOLDER_SERVICE_PLACEHOLDER_SERVICE_H_

#import <UIKit/UIKit.h>

#import <map>

#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "base/observer_list.h"
#import "base/time/time.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/search_engines/template_url_id.h"
#import "components/search_engines/template_url_service_observer.h"

class FaviconLoader;
class TemplateURL;
class TemplateURLService;

#include "base/observer_list_types.h"

// TemplateURLServiceObserver is notified whenever the set of TemplateURLs
// are modified.
class PlaceholderServiceObserver : public base::CheckedObserver {
 public:
  // Notification that the placeholder text might have changed.
  // Relevant for both normal and search-only text.
  virtual void OnPlaceholderTextChanged() = 0;

  // Notification that the search engine icon might have changed. This is called
  // when an icon is fetched even if it's not the right size icon.
  virtual void OnPlaceholderImageChanged() {}

  // Notification that the placeholder service is shutting down. Observers that
  // might outlive the service can use this to clear out any raw pointers to the
  // service.
  virtual void OnPlaceholderServiceShuttingDown() {}

 protected:
  ~PlaceholderServiceObserver() override = default;
};

// A class that vends the placeholder data for omnibox and fakebox.
class PlaceholderService : public KeyedService,
                           public TemplateURLServiceObserver {
 public:
  explicit PlaceholderService(FaviconLoader* favicon_loader,
                              TemplateURLService* template_url_service);

  PlaceholderService() = delete;
  PlaceholderService(const PlaceholderService&) = delete;
  PlaceholderService& operator=(const PlaceholderService&) = delete;

  ~PlaceholderService() override;

  // Return a weak pointer to the current object.
  base::WeakPtr<PlaceholderService> AsWeakPtr();

  // Observers used to listen for changes to the model.
  // TemplateURLService does NOT delete the observers when deleted.
  void AddObserver(PlaceholderServiceObserver* observer);
  void RemoveObserver(PlaceholderServiceObserver* observer);

  using PlaceholderImageCallback = base::RepeatingCallback<void(UIImage*)>;
  // Requests the icon for the current default search engine at the given
  // `icon_point_size`. If the icon is available synchronously, the callback
  // will be called immediately with the cached icon. Otherwise, the callback
  // will be called with a placeholder icon first, and then updated with the
  // real icon once it's available. The callback will not be updated if the
  // default search engine changes during the fetch.
  void FetchDefaultSearchEngineIcon(CGFloat icon_point_size,
                                    PlaceholderImageCallback callback);

  // Returns the icon for the current default search engine at the given
  // `icon_point_size`. If the icon is unavailable, it will be fetched and
  // `OnPlaceholderImageChanged` will be called once it becomes available.
  UIImage* GetDefaultSearchEngineIcon(CGFloat icon_point_size);

  NSString* GetCurrentPlaceholderText();
  NSString* GetCurrentSearchOnlyPlaceholderText();

  // TemplateURLServiceObserver
  void OnTemplateURLServiceChanged() override;

 private:
  // Retrieves a bundled icon (e.g., Google icon) for the given `template_url`
  // and `icon_point_size`, if available. Returns nil otherwise.
  UIImage* GetBundledIconForTemplateURL(const TemplateURL* template_url,
                                        CGFloat icon_point_size);

  // Called when an icon has been successfully fetched or retrieved for a
  // `template_url_id` at a specific `icon_point_size`. Caches the icon and
  // notifies relevant callbacks.
  void OnIconReceivedForTemplateURL(TemplateURLID template_url_id,
                                    CGFloat icon_point_size,
                                    UIImage* icon);

  // Initiates a fetch for the icon of the given `template_url` at the specified
  // `icon_point_size`.
  void PerformIconFetch(const TemplateURL* template_url,
                        CGFloat icon_point_size);

  raw_ptr<FaviconLoader> favicon_loader_;
  raw_ptr<TemplateURLService> template_url_service_;
  // Current default search engine.
  raw_ptr<const TemplateURL> current_dse_;
  base::ObserverList<PlaceholderServiceObserver> model_observers_;
  // Cache for fetched/bundled icons. Keyed by icon size.
  NSCache<NSNumber*, UIImage*>* icon_cache_;
  // Map of icon sizes to a list of callbacks awaiting an icon of that size.
  // This is cleared when default search engine changes.
  std::map<CGFloat, std::vector<PlaceholderImageCallback>> icon_callbacks_;
  // Map of icon sizes to a timestamp after which a fetch is allowed.
  std::map<CGFloat, base::TimeTicks> fetch_cooldowns_;

  base::WeakPtrFactory<PlaceholderService> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_OMNIBOX_MODEL_PLACEHOLDER_SERVICE_PLACEHOLDER_SERVICE_H_
