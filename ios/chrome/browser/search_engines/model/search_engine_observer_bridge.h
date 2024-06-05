// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEARCH_ENGINES_MODEL_SEARCH_ENGINE_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_SEARCH_ENGINES_MODEL_SEARCH_ENGINE_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#import "base/memory/raw_ptr.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_observer.h"

// Objective-C equivalent of the TemplateURLServiceObserver class.
@protocol SearchEngineObserving <NSObject>

// Called when the search engine is changed.
- (void)searchEngineChanged;

@optional
// Called from OnTemplateURLServiceShuttingDown.
// Note that the observer will unregister itself at `urlService` destruction, no
// explicit action on owner's part is necessary.
- (void)templateURLServiceShuttingDown:(TemplateURLService*)urlService;

@end

// Observer used to reload the Search Engine collection view once the
// TemplateURLService changes, either on first load or due to a
// policy change.
class SearchEngineObserverBridge : public TemplateURLServiceObserver {
 public:
  SearchEngineObserverBridge(id<SearchEngineObserving> owner,
                             TemplateURLService* urlService);
  ~SearchEngineObserverBridge() override;
  void OnTemplateURLServiceChanged() override;
  void OnTemplateURLServiceShuttingDown() override;

 private:
  __weak id<SearchEngineObserving> owner_;
  raw_ptr<TemplateURLService> templateURLService_;  // weak
};

#endif  // IOS_CHROME_BROWSER_SEARCH_ENGINES_MODEL_SEARCH_ENGINE_OBSERVER_BRIDGE_H_
