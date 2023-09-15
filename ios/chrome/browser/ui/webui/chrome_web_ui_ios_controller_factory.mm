// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/webui/chrome_web_ui_ios_controller_factory.h"

#import <Foundation/Foundation.h>

#import "base/functional/bind.h"
#import "base/location.h"
#import "base/no_destructor.h"
#import "components/commerce/core/commerce_constants.h"
#import "components/commerce/ios/browser/commerce_internals_ui.h"
#import "components/optimization_guide/optimization_guide_internals/webui/url_constants.h"
#import "components/version_info/channel.h"
#import "ios/chrome/browser/commerce/model/shopping_service_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/ui/webui/about_ui.h"
#import "ios/chrome/browser/ui/webui/autofill_and_password_manager_internals/autofill_internals_ui_ios.h"
#import "ios/chrome/browser/ui/webui/autofill_and_password_manager_internals/password_manager_internals_ui_ios.h"
#import "ios/chrome/browser/ui/webui/crashes_ui.h"
#import "ios/chrome/browser/ui/webui/download_internals_ui.h"
#import "ios/chrome/browser/ui/webui/flags_ui.h"
#import "ios/chrome/browser/ui/webui/gcm/gcm_internals_ui.h"
#import "ios/chrome/browser/ui/webui/inspect/inspect_ui.h"
#import "ios/chrome/browser/ui/webui/interstitials/interstitial_ui.h"
#import "ios/chrome/browser/ui/webui/local_state/local_state_ui.h"
#import "ios/chrome/browser/ui/webui/management/management_ui.h"
#import "ios/chrome/browser/ui/webui/net_export/net_export_ui.h"
#import "ios/chrome/browser/ui/webui/ntp_tiles_internals_ui.h"
#import "ios/chrome/browser/ui/webui/omaha_ui.h"
#import "ios/chrome/browser/ui/webui/optimization_guide_internals/optimization_guide_internals_ui.h"
#import "ios/chrome/browser/ui/webui/policy/policy_ui.h"
#import "ios/chrome/browser/ui/webui/prefs_internals_ui.h"
#import "ios/chrome/browser/ui/webui/signin_internals_ui_ios.h"
#import "ios/chrome/browser/ui/webui/terms_ui.h"
#import "ios/chrome/browser/ui/webui/translate_internals/translate_internals_ui.h"
#import "ios/chrome/browser/ui/webui/ukm_internals_ui.h"
#import "ios/chrome/browser/ui/webui/user_actions_ui.h"
#import "ios/chrome/browser/ui/webui/userdefaults_internals_ui.h"
#import "ios/chrome/browser/ui/webui/version_ui.h"
#import "ios/chrome/common/channel_info.h"
#import "ios/components/webui/sync_internals/sync_internals_ui.h"
#import "ios/components/webui/web_ui_url_constants.h"
#import "url/gurl.h"

using ::version_info::Channel;
using web::WebUIIOS;
using web::WebUIIOSController;

namespace {

// A function for creating a new WebUIIOS.
using WebUIIOSFactoryFunction =
    std::unique_ptr<WebUIIOSController> (*)(WebUIIOS* web_ui, const GURL& url);

// Template for defining WebUIIOSFactoryFunction.
template <class T>
std::unique_ptr<WebUIIOSController> NewWebUIIOS(WebUIIOS* web_ui,
                                                const GURL& url) {
  return std::make_unique<T>(web_ui, url.host());
}

template <>
std::unique_ptr<WebUIIOSController> NewWebUIIOS<commerce::CommerceInternalsUI>(
    WebUIIOS* web_ui,
    const GURL& url) {
  ChromeBrowserState* browser_state = ChromeBrowserState::FromWebUIIOS(web_ui);
  return std::make_unique<commerce::CommerceInternalsUI>(
      web_ui, commerce::kChromeUICommerceInternalsHost,
      commerce::ShoppingServiceFactory::GetForBrowserState(browser_state));
}

// Returns a function that can be used to create the right type of WebUIIOS for
// a tab, based on its URL. Returns NULL if the URL doesn't have WebUIIOS
// associated with it.
WebUIIOSFactoryFunction GetWebUIIOSFactoryFunction(const GURL& url) {
  // This will get called a lot to check all URLs, so do a quick check of other
  // schemes to filter out most URLs.
  if (!url.SchemeIs(kChromeUIScheme))
    return nullptr;

  // Please keep this in alphabetical order. If #ifs or special logic is
  // required, add it below in the appropriate section.
  const std::string url_host = url.host();
  if (url_host == kChromeUIAutofillInternalsHost)
    return &NewWebUIIOS<AutofillInternalsUIIOS>;
  if (url_host == kChromeUIChromeURLsHost ||
      url_host == kChromeUIHistogramHost || url_host == kChromeUICreditsHost)
    return &NewWebUIIOS<AboutUI>;
  if (url_host == commerce::kChromeUICommerceInternalsHost) {
    return &NewWebUIIOS<commerce::CommerceInternalsUI>;
  }
  if (url_host == kChromeUICrashesHost)
    return &NewWebUIIOS<CrashesUI>;
  if (url_host == kChromeUIDownloadInternalsHost)
    return &NewWebUIIOS<DownloadInternalsUI>;
  if (url_host == kChromeUIFlagsHost)
    return &NewWebUIIOS<FlagsUI>;
  if (url_host == kChromeUIGCMInternalsHost)
    return &NewWebUIIOS<GCMInternalsUI>;
  if (url_host == kChromeUIInspectHost)
    return &NewWebUIIOS<InspectUI>;
  if (url_host == kChromeUIIntersitialsHost)
    return &NewWebUIIOS<InterstitialUI>;
  if (url_host == kChromeUILocalStateHost)
    return &NewWebUIIOS<LocalStateUI>;
  if (url_host == kChromeUIManagementHost)
    return &NewWebUIIOS<ManagementUI>;
  if (url_host == kChromeUINetExportHost)
    return &NewWebUIIOS<NetExportUI>;
  if (url_host == kChromeUINTPTilesInternalsHost)
    return &NewWebUIIOS<NTPTilesInternalsUI>;
  if (url_host == kChromeUIOmahaHost)
    return &NewWebUIIOS<OmahaUI>;
  if (url_host ==
      optimization_guide_internals::kChromeUIOptimizationGuideInternalsHost) {
    return &NewWebUIIOS<OptimizationGuideInternalsUI>;
  }
  if (url_host == kChromeUIPasswordManagerInternalsHost)
    return &NewWebUIIOS<PasswordManagerInternalsUIIOS>;
  if (url_host == kChromeUIPrefsInternalsHost)
    return &NewWebUIIOS<PrefsInternalsUI>;
  if (url_host == kChromeUISignInInternalsHost)
    return &NewWebUIIOS<SignInInternalsUIIOS>;
  if (url.host_piece() == kChromeUITranslateInternalsHost)
    return &NewWebUIIOS<TranslateInternalsUI>;
  if (url_host == kChromeUIURLKeyedMetricsHost)
    return &NewWebUIIOS<UkmInternalsUI>;
  if (url_host == kChromeUIUserActionsHost)
    return &NewWebUIIOS<UserActionsUI>;
  if (url_host == kChromeUISyncInternalsHost)
    return &NewWebUIIOS<SyncInternalsUI>;
  if (url_host == kChromeUITermsHost)
    return &NewWebUIIOS<TermsUI>;
  if (url_host == kChromeUIVersionHost)
    return &NewWebUIIOS<VersionUI>;
  if (url_host == kChromeUIPolicyHost)
    return &NewWebUIIOS<PolicyUI>;
  if (url_host == kChromeUIUserDefaultsInternalsHost &&
      GetChannel() != Channel::STABLE) {
    return &NewWebUIIOS<UserDefaultsInternalsUI>;
  }

  return nullptr;
}

}  // namespace

NSInteger ChromeWebUIIOSControllerFactory::GetErrorCodeForWebUIURL(
    const GURL& url) const {
  if (url.host() == kChromeUIDinoHost) {
    return NSURLErrorNotConnectedToInternet;
  }
  if (GetWebUIIOSFactoryFunction(url))
    return 0;
  return NSURLErrorUnsupportedURL;
}

std::unique_ptr<WebUIIOSController>
ChromeWebUIIOSControllerFactory::CreateWebUIIOSControllerForURL(
    WebUIIOS* web_ui,
    const GURL& url) const {
  WebUIIOSFactoryFunction function = GetWebUIIOSFactoryFunction(url);
  if (!function)
    return nullptr;

  return (*function)(web_ui, url);
}

// static
ChromeWebUIIOSControllerFactory*
ChromeWebUIIOSControllerFactory::GetInstance() {
  static base::NoDestructor<ChromeWebUIIOSControllerFactory> instance;
  return instance.get();
}

ChromeWebUIIOSControllerFactory::ChromeWebUIIOSControllerFactory() {}

ChromeWebUIIOSControllerFactory::~ChromeWebUIIOSControllerFactory() {}
