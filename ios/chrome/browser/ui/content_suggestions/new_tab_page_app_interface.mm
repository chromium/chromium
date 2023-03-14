// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/new_tab_page_app_interface.h"

#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "components/search_engines/template_url.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/flags/system_flags.h"
#import "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_test_utils.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "net/base/mac/url_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using content_suggestions::SearchFieldWidth;

@implementation NewTabPageAppInterface

+ (NSString*)defaultSearchEngine {
  // Get the default Search Engine.
  ChromeBrowserState* browser_state =
      chrome_test_util::GetOriginalBrowserState();
  TemplateURLService* service =
      ios::TemplateURLServiceFactory::GetForBrowserState(browser_state);
  const TemplateURL* default_provider = service->GetDefaultSearchProvider();
  DCHECK(default_provider);
  return base::SysUTF16ToNSString(default_provider->short_name());
}

+ (void)resetSearchEngineTo:(NSString*)defaultSearchEngine {
  std::u16string defaultSearchEngineString =
      base::SysNSStringToUTF16(defaultSearchEngine);
  // Set the search engine back to the default in case the test fails before
  // cleaning it up.
  ChromeBrowserState* browser_state =
      chrome_test_util::GetOriginalBrowserState();
  TemplateURLService* service =
      ios::TemplateURLServiceFactory::GetForBrowserState(browser_state);
  std::vector<TemplateURL*> urls = service->GetTemplateURLs();

  for (auto iter = urls.begin(); iter != urls.end(); ++iter) {
    if (defaultSearchEngineString == (*iter)->short_name()) {
      service->SetUserSelectedDefaultSearchProvider(*iter);
    }
  }
}

+ (CGFloat)searchFieldWidthForCollectionWidth:(CGFloat)collectionWidth
                              traitCollection:
                                  (UITraitCollection*)traitCollection {
  return content_suggestions::SearchFieldWidth(collectionWidth,
                                               traitCollection);
}

+ (UICollectionView*)collectionView {
  return ntp_home::CollectionView();
}

+ (UICollectionView*)contentSuggestionsCollectionView {
  return ntp_home::ContentSuggestionsCollectionView();
}

+ (UIView*)fakeOmnibox {
  return ntp_home::FakeOmnibox();
}

+ (UILabel*)discoverHeaderLabel {
  return ntp_home::DiscoverHeaderLabel();
}

@end
