// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/webui/chrome_web_ui_ios_controller_factory.h"

#import <Foundation/Foundation.h>

#include "base/bind.h"
#include "base/location.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/system_flags.h"
#include "ios/chrome/browser/ui/webui/about_ui.h"
#include "ios/chrome/browser/ui/webui/autofill_and_password_manager_internals/autofill_internals_ui_ios.h"
#include "ios/chrome/browser/ui/webui/autofill_and_password_manager_internals/password_manager_internals_ui_ios.h"
#include "ios/chrome/browser/ui/webui/crashes_ui.h"
#include "ios/chrome/browser/ui/webui/flags_ui.h"
#include "ios/chrome/browser/ui/webui/gcm/gcm_internals_ui.h"
#include "ios/chrome/browser/ui/webui/inspect/inspect_ui.h"
#include "ios/chrome/browser/ui/webui/net_export/net_export_ui.h"
#include "ios/chrome/browser/ui/webui/ntp_tiles_internals_ui.h"
#include "ios/chrome/browser/ui/webui/omaha_ui.h"
#include "ios/chrome/browser/ui/webui/prefs_internals_ui.h"
#include "ios/chrome/browser/ui/webui/signin_internals_ui_ios.h"
#include "ios/chrome/browser/ui/webui/suggestions_ui.h"
#include "ios/chrome/browser/ui/webui/sync_internals/sync_internals_ui.h"
#include "ios/chrome/browser/ui/webui/terms_ui.h"
#include "ios/chrome/browser/ui/webui/translate_internals/translate_internals_ui.h"
#include "ios/chrome/browser/ui/webui/ukm_internals_ui.h"
#include "ios/chrome/browser/ui/webui/user_actions_ui.h"
#include "ios/chrome/browser/ui/webui/version_ui.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
  return std::make_unique<T>(web_ui);
}

template <class T>
std::unique_ptr<WebUIIOSController> NewWebUIIOSWithHost(WebUIIOS* web_ui,
                                                        const GURL& url) {
  return std::make_unique<T>(web_ui, url.host());
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
    return &NewWebUIIOSWithHost<AboutUI>;
  if (url_host == kChromeUICrashesHost)
    return &NewWebUIIOS<CrashesUI>;
  if (url_host == kChromeUIFlagsHost)
    return &NewWebUIIOS<FlagsUI>;
  if (url_host == kChromeUIGCMInternalsHost)
    return &NewWebUIIOS<GCMInternalsUI>;
  if (url_host == kChromeUIInspectHost)
    return &NewWebUIIOS<InspectUI>;
  if (url_host == kChromeUINetExportHost)
    return &NewWebUIIOS<NetExportUI>;
  if (url_host == kChromeUINTPTilesInternalsHost)
    return &NewWebUIIOS<NTPTilesInternalsUI>;
  if (url_host == kChromeUIOmahaHost)
    return &NewWebUIIOS<OmahaUI>;
  if (url_host == kChromeUIPasswordManagerInternalsHost)
    return &NewWebUIIOS<PasswordManagerInternalsUIIOS>;
  if (url_host == kChromeUIPrefsInternalsHost)
    return &NewWebUIIOS<PrefsInternalsUI>;
  if (url_host == kChromeUISignInInternalsHost)
    return &NewWebUIIOS<SignInInternalsUIIOS>;
  if (url.host_piece() == kChromeUISuggestionsHost)
    return &NewWebUIIOS<suggestions::SuggestionsUI>;
  if (url.host_piece() == kChromeUITranslateInternalsHost)
    return &NewWebUIIOS<TranslateInternalsUI>;
  if (url_host == kChromeUIURLKeyedMetricsHost)
    return &NewWebUIIOS<UkmInternalsUI>;
  if (url_host == kChromeUIUserActionsHost)
    return &NewWebUIIOS<UserActionsUI>;
  if (url_host == kChromeUISyncInternalsHost)
    return &NewWebUIIOS<SyncInternalsUI>;
  if (url_host == kChromeUITermsHost)
    return &NewWebUIIOSWithHost<TermsUI>;
  if (url_host == kChromeUIVersionHost)
    return &NewWebUIIOS<VersionUI>;

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
