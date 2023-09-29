// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/settings_app_interface.h"

#import "base/containers/contains.h"
#import "base/strings/sys_string_conversions.h"
#import "components/browsing_data/core/pref_names.h"
#import "components/metrics/metrics_pref_names.h"
#import "components/metrics/metrics_service.h"
#import "components/prefs/pref_member.h"
#import "components/prefs/pref_service.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/app/main_controller.h"
#import "ios/chrome/browser/content_settings/model/host_content_settings_map_factory.h"
#import "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"

namespace {

std::vector<std::string> listHosts;
std::string portForRewrite;

bool HostToLocalHostRewrite(GURL* url, web::BrowserState* browser_state) {
  DCHECK(url);
  for (const std::string& host : listHosts) {
    if (base::Contains(url->host(), host)) {
      *url = GURL("http://127.0.0.1:" + portForRewrite + "/" + host);
      return true;
    }
  }

  return false;
}

}  // namespace

// Test specific helpers for settings_egtest.mm.
@implementation SettingsAppInterface : NSObject

+ (void)restoreClearBrowsingDataCheckmarksToDefault {
  ChromeBrowserState* browserState =
      chrome_test_util::GetOriginalBrowserState();
  PrefService* preferences = browserState->GetPrefs();
  preferences->SetBoolean(browsing_data::prefs::kDeleteBrowsingHistory, true);
  preferences->SetBoolean(browsing_data::prefs::kDeleteCache, true);
  preferences->SetBoolean(browsing_data::prefs::kDeleteCookies, true);
  preferences->SetBoolean(browsing_data::prefs::kDeletePasswords, false);
  preferences->SetBoolean(browsing_data::prefs::kDeleteFormData, false);
}

+ (BOOL)isMetricsRecordingEnabled {
  return chrome_test_util::IsMetricsRecordingEnabled();
}

+ (BOOL)isMetricsReportingEnabled {
  return chrome_test_util::IsMetricsReportingEnabled();
}

+ (void)setMetricsReportingEnabled:(BOOL)reportingEnabled {
  chrome_test_util::SetBooleanLocalStatePref(
      metrics::prefs::kMetricsReportingEnabled, reportingEnabled);
}

+ (BOOL)isCrashpadEnabled {
  return chrome_test_util::IsCrashpadEnabled();
}

+ (BOOL)isCrashpadReportingEnabled {
  return chrome_test_util::IsCrashpadReportingEnabled();
}

+ (BOOL)settingsRegisteredKeyboardCommands {
  UIViewController* viewController =
      chrome_test_util::GetMainController()
          .browserProviderInterface.mainBrowserProvider.viewController;
  return viewController.presentedViewController.keyCommands != nil;
}

+ (void)overrideSearchEngineURL:(NSString*)searchEngineURL {
  TemplateURLData templateURLData;
  templateURLData.SetURL(base::SysNSStringToUTF8(searchEngineURL));

  auto defaultSearchProvider = std::make_unique<TemplateURL>(templateURLData);
  TemplateURL* defaultSearchProviderPtr = defaultSearchProvider.get();

  TemplateURLService* service =
      ios::TemplateURLServiceFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());
  service->Add(std::move(defaultSearchProvider));
  service->SetUserSelectedDefaultSearchProvider(defaultSearchProviderPtr);
}

+ (void)resetSearchEngine {
  TemplateURLService* service =
      ios::TemplateURLServiceFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());

  TemplateURL* templateURL = service->GetTemplateURLForHost("google.com");
  service->SetUserSelectedDefaultSearchProvider(templateURL);
}

+ (void)addURLRewriterForHosts:(NSArray<NSString*>*)hosts
                        onPort:(NSString*)port {
  listHosts.clear();
  for (NSString* host in hosts) {
    listHosts.push_back(base::SysNSStringToUTF8(host));
  }
  portForRewrite = base::SysNSStringToUTF8(port);

  chrome_test_util::GetCurrentWebState()
      ->GetNavigationManager()
      ->AddTransientURLRewriter(&HostToLocalHostRewrite);
}

@end
