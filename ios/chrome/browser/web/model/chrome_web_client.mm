// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/chrome_web_client.h"

#import <UIKit/UIKit.h>

#import <string_view>

#import "base/apple/bundle_locations.h"
#import "base/command_line.h"
#import "base/feature_list.h"
#import "base/files/file_util.h"
#import "base/ios/ios_util.h"
#import "base/ios/ns_error_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/no_destructor.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/ios/browser/autofill_java_script_feature.h"
#import "components/autofill/ios/browser/suggestion_controller_java_script_feature.h"
#import "components/autofill/ios/form_util/form_handlers_java_script_feature.h"
#import "components/dom_distiller/core/url_constants.h"
#import "components/google/core/common/google_util.h"
#import "components/language/ios/browser/language_detection_java_script_feature.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/password_manager/ios/password_manager_java_script_feature.h"
#import "components/strings/grit/components_strings.h"
#import "components/supervised_user/core/browser/supervised_user_interstitial.h"
#import "components/translate/ios/browser/translate_java_script_feature.h"
#import "components/version_info/version_info.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_java_script_feature.h"
#import "ios/chrome/browser/content_settings/model/host_content_settings_map_factory.h"
#import "ios/chrome/browser/flags/chrome_switches.h"
#import "ios/chrome/browser/follow/model/follow_java_script_feature.h"
#import "ios/chrome/browser/https_upgrades/model/https_upgrade_service_factory.h"
#import "ios/chrome/browser/link_to_text/model/link_to_text_java_script_feature.h"
#import "ios/chrome/browser/ntp/model/browser_policy_new_tab_page_rewriter.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/permissions/model/features.h"
#import "ios/chrome/browser/permissions/model/geolocation_api_usage_java_script_feature.h"
#import "ios/chrome/browser/permissions/model/media_api_usage_java_script_feature.h"
#import "ios/chrome/browser/prerender/model/prerender_service.h"
#import "ios/chrome/browser/prerender/model/prerender_service_factory.h"
#import "ios/chrome/browser/reading_list/model/offline_page_tab_helper.h"
#import "ios/chrome/browser/reading_list/model/offline_url_utils.h"
#import "ios/chrome/browser/safe_browsing/model/password_protection_java_script_feature.h"
#import "ios/chrome/browser/safe_browsing/model/safe_browsing_blocking_page.h"
#import "ios/chrome/browser/search_engines/model/search_engine_java_script_feature.h"
#import "ios/chrome/browser/search_engines/model/search_engine_tab_helper_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/elements/windowed_container_view.h"
#import "ios/chrome/browser/ssl/model/ios_ssl_error_handler.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_error.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_error_container.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_interstitial_java_script_feature.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_service_factory.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_url_filter_tab_helper.h"
#import "ios/chrome/browser/web/model/browser_about_rewriter.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_java_script_feature.h"
#import "ios/chrome/browser/web/model/chrome_main_parts.h"
#import "ios/chrome/browser/web/model/error_page_util.h"
#import "ios/chrome/browser/web/model/features.h"
#import "ios/chrome/browser/web/model/font_size/font_size_java_script_feature.h"
#import "ios/chrome/browser/web/model/image_fetch/image_fetch_java_script_feature.h"
#import "ios/chrome/browser/web/model/java_script_console/java_script_console_feature.h"
#import "ios/chrome/browser/web/model/java_script_console/java_script_console_feature_factory.h"
#import "ios/chrome/browser/web/model/print/print_java_script_feature.h"
#import "ios/chrome/browser/web/model/web_performance_metrics/web_performance_metrics_java_script_feature.h"
#import "ios/chrome/browser/web_selection/model/web_selection_java_script_feature.h"
#import "ios/chrome/common/channel_info.h"
#import "ios/components/security_interstitials/https_only_mode/feature.h"
#import "ios/components/security_interstitials/https_only_mode/https_only_mode_blocking_page.h"
#import "ios/components/security_interstitials/https_only_mode/https_only_mode_container.h"
#import "ios/components/security_interstitials/https_only_mode/https_only_mode_controller_client.h"
#import "ios/components/security_interstitials/https_only_mode/https_only_mode_error.h"
#import "ios/components/security_interstitials/https_only_mode/https_upgrade_service.h"
#import "ios/components/security_interstitials/ios_blocking_page_tab_helper.h"
#import "ios/components/security_interstitials/ios_security_interstitial_java_script_feature.h"
#import "ios/components/security_interstitials/lookalikes/lookalike_url_blocking_page.h"
#import "ios/components/security_interstitials/lookalikes/lookalike_url_container.h"
#import "ios/components/security_interstitials/lookalikes/lookalike_url_controller_client.h"
#import "ios/components/security_interstitials/lookalikes/lookalike_url_error.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_error.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_unsafe_resource_container.h"
#import "ios/components/webui/web_ui_url_constants.h"
#import "ios/net/protocol_handler_util.h"
#import "ios/public/provider/chrome/browser/url_rewriters/url_rewriters_api.h"
#import "ios/web/common/features.h"
#import "ios/web/common/user_agent.h"
#import "ios/web/public/find_in_page/crw_find_session.h"
#import "ios/web/public/navigation/browser_url_rewriter.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "net/base/net_errors.h"
#import "net/http/http_util.h"
#import "services/metrics/public/cpp/ukm_source_id.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/resource/resource_bundle.h"
#import "url/gurl.h"

namespace {
// The tag describing the product name with a placeholder for the version.
const char kProductTagWithPlaceholder[] = "CriOS/%s";

// Returns the safe browsing error page HTML.
NSString* GetSafeBrowsingErrorPageHTML(web::WebState* web_state,
                                       int64_t navigation_id) {
  // Fetch the unsafe resource causing this error page from the WebState's
  // container.
  SafeBrowsingUnsafeResourceContainer* container =
      SafeBrowsingUnsafeResourceContainer::FromWebState(web_state);
  const security_interstitials::UnsafeResource* resource =
      container->GetMainFrameUnsafeResource();

  // Construct the blocking page and associate it with the WebState.
  std::unique_ptr<security_interstitials::IOSSecurityInterstitialPage> page =
      SafeBrowsingBlockingPage::Create(*resource);
  std::string error_page_content = page->GetHtmlContents();
  security_interstitials::IOSBlockingPageTabHelper::FromWebState(web_state)
      ->AssociateBlockingPage(navigation_id, std::move(page));

  return base::SysUTF8ToNSString(error_page_content);
}

// Returns the lookalike error page HTML.
NSString* GetLookalikeUrlErrorPageHtml(web::WebState* web_state,
                                       int64_t navigation_id) {
  // Fetch the lookalike URL info from the WebState's container.
  LookalikeUrlContainer* container =
      LookalikeUrlContainer::FromWebState(web_state);
  std::unique_ptr<LookalikeUrlContainer::LookalikeUrlInfo> lookalike_info =
      container->ReleaseLookalikeUrlInfo();

  // Construct the blocking page and associate it with the WebState.
  std::unique_ptr<security_interstitials::IOSSecurityInterstitialPage> page =
      std::make_unique<LookalikeUrlBlockingPage>(
          web_state, lookalike_info->safe_url, lookalike_info->request_url,
          ukm::ConvertToSourceId(navigation_id,
                                 ukm::SourceIdType::NAVIGATION_ID),
          lookalike_info->match_type,
          std::make_unique<LookalikeUrlControllerClient>(
              web_state, lookalike_info->safe_url, lookalike_info->request_url,
              GetApplicationContext()->GetApplicationLocale()));
  std::string error_page_content = page->GetHtmlContents();
  security_interstitials::IOSBlockingPageTabHelper::FromWebState(web_state)
      ->AssociateBlockingPage(navigation_id, std::move(page));

  return base::SysUTF8ToNSString(error_page_content);
}

// Returns the HTTPS only mode error page HTML.
NSString* GetHttpsOnlyModeErrorPageHtml(web::WebState* web_state,
                                        int64_t navigation_id) {
  // Fetch the HTTP URL from the container.
  HttpsOnlyModeContainer* container =
      HttpsOnlyModeContainer::FromWebState(web_state);
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state->GetBrowserState());
  HttpsUpgradeService* service =
      HttpsUpgradeServiceFactory::GetForProfile(profile);

  // Construct the blocking page and associate it with the WebState.
  std::unique_ptr<security_interstitials::IOSSecurityInterstitialPage> page =
      std::make_unique<HttpsOnlyModeBlockingPage>(
          web_state, container->http_url(), service,
          std::make_unique<HttpsOnlyModeControllerClient>(
              web_state, container->http_url(),
              GetApplicationContext()->GetApplicationLocale()));

  std::string error_page_content = page->GetHtmlContents();
  security_interstitials::IOSBlockingPageTabHelper::FromWebState(web_state)
      ->AssociateBlockingPage(navigation_id, std::move(page));
  return base::SysUTF8ToNSString(error_page_content);
}

// Returns the Supervised User Error Page Interstitial HTML.
NSString* GetSupervisedUserErrorPageHTML(web::WebState* web_state,
                                         int64_t navigation_id,
                                         const GURL& url) {
  // Fetch the supervised user error info from the WebState's container.
  SupervisedUserErrorContainer* container =
      SupervisedUserErrorContainer::FromWebState(web_state);
  CHECK(container);
  std::unique_ptr<SupervisedUserErrorContainer::SupervisedUserErrorInfo>
      error_info = container->ReleaseSupervisedUserErrorInfo();
  CHECK(error_info);

  std::unique_ptr<supervised_user::SupervisedUserInterstitial> interstitial =
      container->CreateSupervisedUserInterstitial(*error_info);
  std::unique_ptr<security_interstitials::IOSSecurityInterstitialPage> page =
      std::make_unique<SupervisedUserInterstitialBlockingPage>(
          std::move(interstitial), /*controller_client=*/nullptr, container,
          web_state);

  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state->GetBrowserState());
  std::string error_page_content =
      supervised_user::SupervisedUserInterstitial::GetHTMLContents(
          SupervisedUserServiceFactory::GetForProfile(profile),
          profile->GetPrefs(), error_info->filtering_behavior_reason(),
          container->IsRemoteApprovalPendingForUrl(url),
          error_info->is_main_frame(),
          GetApplicationContext()->GetApplicationLocale());

  security_interstitials::IOSBlockingPageTabHelper::FromWebState(web_state)
      ->AssociateBlockingPage(navigation_id, std::move(page));
  return base::SysUTF8ToNSString(error_page_content);
}

// Returns a string describing the product name and version, of the
// form "productname/version". Used as part of the user agent string.
std::string GetMobileProduct() {
  return base::StringPrintf(kProductTagWithPlaceholder,
                            version_info::GetVersionNumber().data());
}

// Returns a string describing the product name and version, of the
// form "productname/version". Used as part of the user agent string.
// The Desktop UserAgent is only using the major version to reduce the surface
// for fingerprinting. The Mobile one is using the full version for legacy
// reasons.
std::string GetDesktopProduct() {
  return base::StringPrintf(kProductTagWithPlaceholder,
                            version_info::GetMajorVersionNumber().c_str());
}

// If `url` is an offline URL, returns the associated online URL. If it is not
// an offline URL then returns `url` as it can be considered as online.
GURL GetOnlineUrl(const GURL& url) {
  GURL online_url = url;
  if (reading_list::IsOfflineEntryURL(url)) {
    online_url = reading_list::EntryURLForOfflineURL(url);
  } else if (reading_list::IsOfflineReloadURL(url)) {
    online_url = reading_list::ReloadURLForOfflineURL(url);
  }
  return online_url;
}

}  // namespace

ChromeWebClient::ChromeWebClient() {}

ChromeWebClient::~ChromeWebClient() {}

std::unique_ptr<web::WebMainParts> ChromeWebClient::CreateWebMainParts() {
  return std::make_unique<IOSChromeMainParts>(
      *base::CommandLine::ForCurrentProcess());
}

void ChromeWebClient::PreWebViewCreation() const {}

void ChromeWebClient::AddAdditionalSchemes(Schemes* schemes) const {
  schemes->standard_schemes.push_back(kChromeUIScheme);
  schemes->secure_schemes.push_back(kChromeUIScheme);
}

std::string ChromeWebClient::GetApplicationLocale() const {
  DCHECK(GetApplicationContext());
  return GetApplicationContext()->GetApplicationLocale();
}

bool ChromeWebClient::IsAppSpecificURL(const GURL& url) const {
  return url.SchemeIs(kChromeUIScheme);
}

std::string ChromeWebClient::GetUserAgent(web::UserAgentType type) const {
  // The user agent should not be requested for app-specific URLs.
  DCHECK_NE(type, web::UserAgentType::NONE);

  // Using desktop user agent overrides a command-line user agent, so that
  // request desktop site can still work when using an overridden UA.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (type != web::UserAgentType::DESKTOP &&
      command_line->HasSwitch(switches::kUserAgent)) {
    std::string user_agent =
        command_line->GetSwitchValueASCII(switches::kUserAgent);
    if (net::HttpUtil::IsValidHeaderValue(user_agent))
      return user_agent;
    LOG(WARNING) << "Ignored invalid value for flag --" << switches::kUserAgent;
  }

  if (type == web::UserAgentType::DESKTOP)
    return web::BuildDesktopUserAgent(GetDesktopProduct());
  return web::BuildMobileUserAgent(GetMobileProduct());
}

std::u16string ChromeWebClient::GetLocalizedString(int message_id) const {
  return l10n_util::GetStringUTF16(message_id);
}

std::string_view ChromeWebClient::GetDataResource(
    int resource_id,
    ui::ResourceScaleFactor scale_factor) const {
  return ui::ResourceBundle::GetSharedInstance().GetRawDataResourceForScale(
      resource_id, scale_factor);
}

base::RefCountedMemory* ChromeWebClient::GetDataResourceBytes(
    int resource_id) const {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
      resource_id);
}

void ChromeWebClient::GetAdditionalWebUISchemes(
    std::vector<std::string>* additional_schemes) {
  additional_schemes->push_back(dom_distiller::kDomDistillerScheme);
}

void ChromeWebClient::PostBrowserURLRewriterCreation(
    web::BrowserURLRewriter* rewriter) {
  rewriter->AddURLRewriter(&WillHandleWebBrowserNewTabPageURLForPolicy);
  rewriter->AddURLRewriter(&WillHandleWebBrowserAboutURL);
  ios::provider::AddURLRewriters(rewriter);
}

std::vector<web::JavaScriptFeature*> ChromeWebClient::GetJavaScriptFeatures(
    web::BrowserState* browser_state) const {
  static base::NoDestructor<PrintJavaScriptFeature> print_feature;
  std::vector<web::JavaScriptFeature*> features;
  if (base::FeatureList::IsEnabled(
          password_manager::features::kPasswordReuseDetectionEnabled)) {
    features.push_back(PasswordProtectionJavaScriptFeature::GetInstance());
  }

  ProfileIOS* profile = ProfileIOS::FromBrowserState(browser_state);
  JavaScriptConsoleFeature* java_script_console_feature =
      JavaScriptConsoleFeatureFactory::GetForProfile(profile);
  features.push_back(java_script_console_feature);

  features.push_back(print_feature.get());

  features.push_back(autofill::AutofillJavaScriptFeature::GetInstance());
  features.push_back(autofill::FormHandlersJavaScriptFeature::GetInstance());
  features.push_back(
      autofill::SuggestionControllerJavaScriptFeature::GetInstance());
  features.push_back(AutofillBottomSheetJavaScriptFeature::GetInstance());
  features.push_back(FontSizeJavaScriptFeature::GetInstance());
  features.push_back(ImageFetchJavaScriptFeature::GetInstance());
  features.push_back(
      password_manager::PasswordManagerJavaScriptFeature::GetInstance());
  features.push_back(LinkToTextJavaScriptFeature::GetInstance());
  features.push_back(WebSelectionJavaScriptFeature::GetInstance());

  SearchEngineJavaScriptFeature::GetInstance()->SetDelegate(
      SearchEngineTabHelperFactory::GetInstance());
  features.push_back(SearchEngineJavaScriptFeature::GetInstance());
  features.push_back(
      security_interstitials::IOSSecurityInterstitialJavaScriptFeature::
          GetInstance());
  features.push_back(
      language::LanguageDetectionJavaScriptFeature::GetInstance());
  features.push_back(translate::TranslateJavaScriptFeature::GetInstance());
  features.push_back(WebPerformanceMetricsJavaScriptFeature::GetInstance());
  features.push_back(FollowJavaScriptFeature::GetInstance());
  features.push_back(ChooseFileJavaScriptFeature::GetInstance());

  features.push_back(
      SupervisedUserInterstitialJavaScriptFeature::GetInstance());

  if (base::FeatureList::IsEnabled(
          kJavaScriptPermissionBasedAPIMetricsEnabled)) {
    if (GeolocationAPIUsageJavaScriptFeature::ShouldOverrideAPI()) {
      features.push_back(GeolocationAPIUsageJavaScriptFeature::GetInstance());
    }
    if (MediaAPIUsageJavaScriptFeature::ShouldOverrideAPI()) {
      features.push_back(MediaAPIUsageJavaScriptFeature::GetInstance());
    }
  }

  return features;
}

void ChromeWebClient::PrepareErrorPage(
    web::WebState* web_state,
    const GURL& url,
    NSError* error,
    bool is_post,
    bool is_off_the_record,
    const std::optional<net::SSLInfo>& ssl_info,
    int64_t navigation_id,
    base::OnceCallback<void(NSString*)> callback) {
  OfflinePageTabHelper* offline_page_tab_helper =
      OfflinePageTabHelper::FromWebState(web_state);
  // WebState that are not attached to a tab may not have an
  // OfflinePageTabHelper.
  if (offline_page_tab_helper &&
      (offline_page_tab_helper->CanHandleErrorLoadingURL(url))) {
    // An offline version of the page will be displayed to replace this error
    // page. Loading an error page here can cause a race between the
    // navigation to load the error page and the navigation to display the
    // offline version of the page. If the latter navigation interrupts the
    // former and causes it to fail, this can incorrectly appear to be a
    // navigation back to the previous committed URL. To avoid this race,
    // return a nil error page here to avoid an error page load. See
    // crbug.com/980912.
    std::move(callback).Run(nil);
    return;
  }
  DCHECK(error);
  NSError* final_underlying_error =
      base::ios::GetFinalUnderlyingErrorFromError(error);
  if ([final_underlying_error.domain
          isEqualToString:kSafeBrowsingErrorDomain]) {
    // Only kUnsafeResourceErrorCode is supported.
    DCHECK_EQ(kUnsafeResourceErrorCode, final_underlying_error.code);
    std::move(callback).Run(
        GetSafeBrowsingErrorPageHTML(web_state, navigation_id));
  } else if ([final_underlying_error.domain
                 isEqualToString:kLookalikeUrlErrorDomain]) {
    // Only kLookalikeUrlErrorCode is supported.
    DCHECK_EQ(kLookalikeUrlErrorCode, final_underlying_error.code);
    std::move(callback).Run(
        GetLookalikeUrlErrorPageHtml(web_state, navigation_id));
  } else if ([final_underlying_error.domain
                 isEqualToString:kSupervisedUserInterstitialErrorDomain]) {
    CHECK_EQ(kSupervisedUserInterstitialErrorCode, final_underlying_error.code);
    std::move(callback).Run(
        GetSupervisedUserErrorPageHTML(web_state, navigation_id, url));
  } else if ([final_underlying_error.domain
                 isEqualToString:kHttpsOnlyModeErrorDomain]) {
    // Only kHttpsOnlyModeErrorCode is supported.
    DCHECK_EQ(kHttpsOnlyModeErrorCode, final_underlying_error.code);
    std::move(callback).Run(
        GetHttpsOnlyModeErrorPageHtml(web_state, navigation_id));
  } else if (ssl_info.has_value()) {
    IOSSSLErrorHandler::HandleSSLError(
        web_state, net::MapCertStatusToNetError(ssl_info.value().cert_status),
        ssl_info.value(), url, ssl_info.value().is_fatal_cert_error,
        navigation_id, std::move(callback));
  } else {
    std::move(callback).Run(
        GetErrorPage(url, error, is_post, is_off_the_record));
  }
}

UIView* ChromeWebClient::GetWindowedContainer() {
  if (!windowed_container_) {
    windowed_container_ = [[WindowedContainerView alloc] init];
  }
  return windowed_container_;
}

bool ChromeWebClient::EnableFullscreenAPI() const {
  // Only use the Fullscreen API on iOS 16.4+, which fixes serious crashes in
  // earlier versions. Also, only enable on iPad to match expectations of the
  // iOS web ecosystem.
  return base::ios::IsRunningOnOrLater(16, 4, 0) &&
         ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET;
}

bool ChromeWebClient::EnableLongPressUIContextMenu() const {
  return true;
}

bool ChromeWebClient::EnableWebInspector(
    web::BrowserState* browser_state) const {
  if (!web::features::IsWebInspectorSupportEnabled()) {
    return false;
  }

  ProfileIOS* profile = ProfileIOS::FromBrowserState(browser_state);
  return profile->GetPrefs()->GetBoolean(prefs::kWebInspectorEnabled);
}

web::UserAgentType ChromeWebClient::GetDefaultUserAgent(
    web::WebState* web_state,
    const GURL& url) const {
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state->GetBrowserState());
  HostContentSettingsMap* settings_map =
      ios::HostContentSettingsMapFactory::GetForProfile(profile);

  bool use_desktop_agent = ShouldLoadUrlInDesktopMode(url, settings_map);
  return use_desktop_agent ? web::UserAgentType::DESKTOP
                           : web::UserAgentType::MOBILE;
}

void ChromeWebClient::LogDefaultUserAgent(web::WebState* web_state,
                                          const GURL& url) const {
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state->GetBrowserState());
  HostContentSettingsMap* settings_map =
      ios::HostContentSettingsMapFactory::GetForProfile(profile);
  bool use_desktop_agent = ShouldLoadUrlInDesktopMode(url, settings_map);
  base::UmaHistogramBoolean("IOS.PageLoad.DefaultModeMobile",
                            !use_desktop_agent);
}

void ChromeWebClient::CleanupNativeRestoreURLs(web::WebState* web_state) const {
  web::NavigationManager* navigationManager = web_state->GetNavigationManager();
  for (int i = 0; i < web_state->GetNavigationItemCount(); i++) {
    // The WKWebView URL underneath the NTP is about://newtab/, which has no
    // title. When restoring the NTP, be sure to re-add the title below.
    web::NavigationItem* item = navigationManager->GetItemAtIndex(i);
    NewTabPageTabHelper::UpdateItem(item);

    // The WKWebView URL underneath a forced-offline page is chrome://offline,
    // which has an embedded entry URL. Apply that entryURL to the virtualURL
    // here.
    if (item->GetVirtualURL().host() == kChromeUIOfflineHost) {
      item->SetVirtualURL(
          reading_list::EntryURLForOfflineURL(item->GetVirtualURL()));
    }
  }
}

void ChromeWebClient::WillDisplayMediaCapturePermissionPrompt(
    web::WebState* web_state) const {
  // When a prendered page displays a prompt, cancel the prerender.
  PrerenderService* prerender_service = PrerenderServiceFactory::GetForProfile(
      ProfileIOS::FromBrowserState(web_state->GetBrowserState()));
  if (prerender_service &&
      prerender_service->IsWebStatePrerendered(web_state)) {
    prerender_service->CancelPrerender();
  }
}

bool ChromeWebClient::IsPointingToSameDocument(const GURL& url1,
                                               const GURL& url2) const {
  GURL url_to_compare1 = GetOnlineUrl(url1);
  GURL url_to_compare2 = GetOnlineUrl(url2);
  return url_to_compare1 == url_to_compare2;
}

bool ChromeWebClient::IsBrowserLockdownModeEnabled() {
  return GetApplicationContext()->GetLocalState()->GetBoolean(
      prefs::kBrowserLockdownModeEnabled);
}

void ChromeWebClient::SetOSLockdownModeEnabled(bool enabled) {
  GetApplicationContext()->GetLocalState()->SetBoolean(
      prefs::kOSLockdownModeEnabled, enabled);
}

bool ChromeWebClient::IsInsecureFormWarningEnabled(
    web::BrowserState* browser_state) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(browser_state);
  if (!profile->GetPrefs()->GetBoolean(prefs::kInsecureFormWarningsEnabled) &&
      profile->GetPrefs()->IsManagedPreference(
          prefs::kInsecureFormWarningsEnabled)) {
    return false;
  }
  return base::FeatureList::IsEnabled(
      security_interstitials::features::kInsecureFormSubmissionInterstitial);
}
