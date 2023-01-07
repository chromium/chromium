// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/search_engines/extension_search_engine_data_updater.h"

#import "base/strings/sys_string_conversions.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/browser/search_engines/search_engines_util.h"
#import "ios/chrome/common/app_group/app_group_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
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
  NSString* userDefaultsKey =
      base::SysUTF8ToNSString(app_group::kChromeAppGroupSupportsSearchByImage);
  [sharedDefaults setBool:supportsSearchByImage forKey:userDefaultsKey];
}
