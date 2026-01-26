// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/settings_app_interface.h"

#import "base/strings/sys_string_conversions.h"
#import "components/browsing_data/core/pref_names.h"
#import "components/metrics/metrics_pref_names.h"
#import "components/metrics/metrics_service.h"
#import "components/prefs/pref_member.h"
#import "components/prefs/pref_service.h"
#import "components/search_engines/search_engine_choice/search_engine_choice_utils.h"
#import "components/search_engines/template_url_prepopulate_data.h"
#import "components/search_engines/template_url_prepopulate_data_resolver.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/browser/content_settings/model/host_content_settings_map_factory.h"
#import "ios/chrome/browser/search_engines/model/template_url_prepopulate_data_resolver_factory.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/common/crash_report/crash_helper.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#import "third_party/search_engines_data/resources/definitions/prepopulated_engines.h"

// Test specific helpers for settings_egtest.mm.
@implementation SettingsAppInterface {
  std::vector<std::string> _listHosts;
  std::string _portForRewrite;
}

+ (instancetype)sharedInstance {
  static SettingsAppInterface* sharedInstance = nil;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    sharedInstance = [[SettingsAppInterface alloc] init];
  });
  return sharedInstance;
}

+ (void)restoreClearBrowsingDataCheckmarksToDefault {
  ProfileIOS* profile = chrome_test_util::GetOriginalProfile();
  PrefService* preferences = profile->GetPrefs();
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

  // Update the Crashpad reporting state.
  crash_helper::common::SetUserEnabledUploading(reportingEnabled);
}

+ (BOOL)isCrashpadEnabled {
  return chrome_test_util::IsCrashpadEnabled();
}

+ (BOOL)isCrashpadReportingEnabled {
  return chrome_test_util::IsCrashpadReportingEnabled();
}

+ (BOOL)settingsRegisteredKeyboardCommands {
  SceneState* sceneState = chrome_test_util::GetForegroundActiveScene();
  UIViewController* viewController =
      sceneState.browserProviderInterface.mainBrowserProvider.viewController;
  return viewController.presentedViewController.keyCommands != nil;
}

+ (void)overrideSearchEngineWithURL:(NSString*)searchEngineURL {
  TemplateURLData templateURLData;
  templateURLData.SetShortName(u"testSearchEngine");
  templateURLData.SetKeyword(u"testSearchEngine");
  GURL searchableURL(base::SysNSStringToUTF8(searchEngineURL));
  templateURLData.SetURL(searchableURL.possibly_invalid_spec());
  templateURLData.favicon_url = TemplateURL::GenerateFaviconURL(searchableURL);
  templateURLData.last_visited = base::Time::Now();

  auto defaultSearchProvider = std::make_unique<TemplateURL>(templateURLData);
  TemplateURL* defaultSearchProviderPtr = defaultSearchProvider.get();

  TemplateURLService* service = ios::TemplateURLServiceFactory::GetForProfile(
      chrome_test_util::GetOriginalProfile());
  service->Add(std::move(defaultSearchProvider));
  service->SetUserSelectedDefaultSearchProvider(defaultSearchProviderPtr);
}

+ (void)resetSearchEngine {
  ProfileIOS* profile = chrome_test_util::GetOriginalProfile();
  PrefService* prefs = chrome_test_util::GetOriginalProfile()->GetPrefs();
  TemplateURLService* service =
      ios::TemplateURLServiceFactory::GetForProfile(profile);
  TemplateURLPrepopulateData::Resolver* prepopulate_data_resolver =
      ios::TemplateURLPrepopulateDataResolverFactory::GetForProfile(profile);
  std::unique_ptr<TemplateURLData> templateURLData =
      prepopulate_data_resolver->GetEngineFromFullList(
          TemplateURLPrepopulateData::google.id);
  auto templateURL = std::make_unique<TemplateURL>(*templateURLData.get());
  service->SetUserSelectedDefaultSearchProvider(templateURL.get());
  search_engines::WipeSearchEngineChoicePrefs(
      *prefs, search_engines::SearchEngineChoiceWipeReason::kCommandLineFlag);
}

+ (void)addURLRewriterForHosts:(NSArray<NSString*>*)hosts
                        onPort:(NSString*)port {
  SettingsAppInterface* shared = [SettingsAppInterface sharedInstance];
  shared->_listHosts.clear();
  for (NSString* host in hosts) {
    shared->_listHosts.push_back(base::SysNSStringToUTF8(host));
  }
  shared->_portForRewrite = base::SysNSStringToUTF8(port);

  auto hostToLocalHostRewrite = [](GURL* url, web::BrowserState* context) {
    return [SettingsAppInterface hostToLocalHostRewriteForURL:url
                                                      context:context];
  };

  chrome_test_util::GetCurrentWebState()
      ->GetNavigationManager()
      ->AddTransientURLRewriter(hostToLocalHostRewrite);
}

+ (bool)hostToLocalHostRewriteForURL:(GURL*)url
                             context:(web::BrowserState*)context {
  DCHECK(url);
  SettingsAppInterface* shared = [SettingsAppInterface sharedInstance];
  for (const std::string& host : shared->_listHosts) {
    if (url->host().contains(host)) {
      *url = GURL("http://127.0.0.1:" + shared->_portForRewrite + "/" + host);
      return true;
    }
  }

  return false;
}

@end
