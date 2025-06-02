// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_MODEL_PLACEHOLDER_SERVICE_H_
#define IOS_CHROME_BROWSER_OMNIBOX_MODEL_PLACEHOLDER_SERVICE_H_

#import <UIKit/UIKit.h>

#import "base/memory/raw_ptr.h"
#import "base/observer_list.h"
#import "base/task/cancelable_task_tracker.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/search_engines/template_url_service_observer.h"

class FaviconLoader;
class TemplateURLService;

#include "base/observer_list_types.h"

// TemplateURLServiceObserver is notified whenever the set of TemplateURLs
// are modified.
class PlaceholderServiceObserver : public base::CheckedObserver {
 public:
  // Notification that the placeholder text might have changed.
  // Relevant for both normal and search-only text.
  virtual void OnPlaceholderTextChanged() = 0;

  // Notification that the search engine icon might have changed.
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

  UIImage* GetCurrentDefaultSearchEngineFavicon();
  NSString* GetCurrentPlaceholderText();
  NSString* GetCurrentSearchOnlyPlaceholderText();

  // TemplateURLServiceObserver
  void OnTemplateURLServiceChanged() override;

 private:
  raw_ptr<FaviconLoader> favicon_loader_;
  raw_ptr<TemplateURLService> template_url_service_;
  base::ObserverList<PlaceholderServiceObserver> model_observers_;

  base::WeakPtrFactory<PlaceholderService> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_OMNIBOX_MODEL_PLACEHOLDER_SERVICE_H_
