// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/search_engines/model/extension_search_engine_data_updater.h"

#import "base/strings/sys_string_conversions.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/browser/search_engines/model/search_engines_util.h"
#import "ios/chrome/browser/widget_kit/model/features.h"
#import "ios/chrome/common/app_group/app_group_constants.h"

#if BUILDFLAG(ENABLE_WIDGET_KIT_EXTENSION)
#import "ios/chrome/browser/widget_kit/model/model_swift.h"  // nogncheck
#endif

ExtensionSearchEngineDataUpdater::ExtensionSearchEngineDataUpdater(
    TemplateURLService* urlService)
    : templateURLService_(urlService) {
  templateURLService_->AddObserver(this);
  OnTemplateURLServiceChanged();
}

ExtensionSearchEngineDataUpdater::~ExtensionSearchEngineDataUpdater() {
  templateURLService_->RemoveObserver(this);
}

void ExtensionSearchEngineDataUpdater::OnTemplateURLServiceChanged() {
  NSUserDefaults* sharedDefaults = app_group::GetGroupUserDefaults();
  BOOL supportsSearchByImage =
      search_engines::SupportsSearchByImage(templateURLService_);
  NSString* searchByImageUserDefaultsKey =
      base::SysUTF8ToNSString(app_group::kChromeAppGroupSupportsSearchByImage);
  [sharedDefaults setBool:supportsSearchByImage
                   forKey:searchByImageUserDefaultsKey];

  // Update whether or not Google is the default search engine.
  const TemplateURL* defaultURL =
      templateURLService_->GetDefaultSearchProvider();
  const BOOL isGoogleDefaultSearchProvider =
      defaultURL &&
      defaultURL->GetEngineType(templateURLService_->search_terms_data()) ==
          SEARCH_ENGINE_GOOGLE;
  NSString* isGoogleDefaultSearchEngineUserDefaultsKey =
      base::SysUTF8ToNSString(
          app_group::kChromeAppGroupIsGoogleDefaultSearchEngine);
  [sharedDefaults setBool:isGoogleDefaultSearchProvider
                   forKey:isGoogleDefaultSearchEngineUserDefaultsKey];

#if BUILDFLAG(ENABLE_WIDGET_KIT_EXTENSION)
  [WidgetTimelinesUpdater reloadAllTimelines];
#endif
}
