// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEARCH_ENGINES_MODEL_EXTENSION_SEARCH_ENGINE_DATA_UPDATER_H_
#define IOS_CHROME_BROWSER_SEARCH_ENGINES_MODEL_EXTENSION_SEARCH_ENGINE_DATA_UPDATER_H_

#import "base/memory/raw_ptr.h"
#include "components/search_engines/template_url_service_observer.h"

class TemplateURLService;

// Extensions need to know data about the current default search provider. This
// class observes that change and writes the necessary data to `NSUserDefaults`.
class ExtensionSearchEngineDataUpdater : public TemplateURLServiceObserver {
 public:
  explicit ExtensionSearchEngineDataUpdater(TemplateURLService* urlService);
  ~ExtensionSearchEngineDataUpdater() override;

 private:
  // TemplateURLServiceObserver
  void OnTemplateURLServiceChanged() override;

  raw_ptr<TemplateURLService> templateURLService_;  // weak
};
#endif  // IOS_CHROME_BROWSER_SEARCH_ENGINES_MODEL_EXTENSION_SEARCH_ENGINE_DATA_UPDATER_H_
