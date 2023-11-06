// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/search_engines/model/search_engines_app_interface.h"

#import "base/strings/sys_string_conversions.h"
#import "components/search_engines/template_url.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/test/app/chrome_test_util.h"

@implementation SearchEnginesAppInterface

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

+ (void)setSearchEngineTo:(NSString*)defaultSearchEngine {
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

+ (void)addSearchEngineWithName:(NSString*)name
                            URL:(NSString*)URL
                     setDefault:(BOOL)setDefault {
  ChromeBrowserState* browser_state =
      chrome_test_util::GetOriginalBrowserState();
  TemplateURLService* url_service =
      ios::TemplateURLServiceFactory::GetForBrowserState(browser_state);
  TemplateURLData data;
  data.SetShortName(base::SysNSStringToUTF16(name));
  data.SetURL(base::SysNSStringToUTF8(URL));
  url_service->Add(std::make_unique<TemplateURL>(data));
  if (setDefault) {
    [SearchEnginesAppInterface setSearchEngineTo:name];
  }
}

+ (void)removeSearchEngineWithName:(NSString*)name {
  ChromeBrowserState* browser_state =
      chrome_test_util::GetOriginalBrowserState();
  TemplateURLService* url_service =
      ios::TemplateURLServiceFactory::GetForBrowserState(browser_state);
  std::vector<TemplateURL*> urls = url_service->GetTemplateURLs();
  std::u16string utfName = base::SysNSStringToUTF16(name);

  for (auto iter = urls.begin(); iter != urls.end(); ++iter) {
    if (utfName == (*iter)->short_name()) {
      url_service->Remove(*iter);
      return;
    }
  }
}

@end
