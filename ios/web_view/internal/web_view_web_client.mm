// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/web_view_web_client.h"

#import <dispatch/dispatch.h>

#import <string_view>

#import "base/apple/bundle_locations.h"
#import "base/check.h"
#import "base/functional/bind.h"
#import "base/ios/ns_error_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/ios/browser/autofill_java_script_feature.h"
#import "components/autofill/ios/browser/suggestion_controller_java_script_feature.h"
#import "components/autofill/ios/form_util/form_handlers_java_script_feature.h"
#import "components/language/ios/browser/language_detection_java_script_feature.h"
#import "components/password_manager/ios/password_manager_java_script_feature.h"
#import "components/security_interstitials/core/unsafe_resource.h"
#import "components/ssl_errors/error_info.h"
#import "components/strings/grit/components_strings.h"
#import "components/translate/ios/browser/translate_java_script_feature.h"
#import "ios/components/security_interstitials/https_only_mode/feature.h"
#import "ios/components/security_interstitials/ios_security_interstitial_java_script_feature.h"
#import "ios/components/security_interstitials/lookalikes/lookalike_url_container.h"
#import "ios/components/security_interstitials/lookalikes/lookalike_url_error.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_error.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_unsafe_resource_container.h"
#import "ios/components/webui/web_ui_url_constants.h"
#import "ios/web/common/user_agent.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/security/ssl_status.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "ios/web_view/internal/cwv_lookalike_url_handler_internal.h"
#import "ios/web_view/internal/cwv_ssl_error_handler_internal.h"
#import "ios/web_view/internal/cwv_ssl_status_internal.h"
#import "ios/web_view/internal/cwv_ssl_util.h"
#import "ios/web_view/internal/cwv_web_view_internal.h"
#import "ios/web_view/internal/js_messaging/web_view_scripts_java_script_feature.h"
#import "ios/web_view/internal/safe_browsing/cwv_unsafe_url_handler_internal.h"
#import "ios/web_view/internal/web_view_browser_state.h"
#import "ios/web_view/internal/web_view_message_handler_java_script_feature.h"
#import "ios/web_view/internal/web_view_web_main_parts.h"
#import "ios/web_view/public/cwv_navigation_delegate.h"
#import "ios/web_view/public/cwv_web_view.h"
#import "net/base/apple/url_conversions.h"
#import "net/cert/cert_status_flags.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/resource/resource_bundle.h"

namespace ios_web_view {

WebViewWebClient::WebViewWebClient() = default;

WebViewWebClient::~WebViewWebClient() = default;

std::unique_ptr<web::WebMainParts> WebViewWebClient::CreateWebMainParts() {
  return std::make_unique<WebViewWebMainParts>();
}

void WebViewWebClient::AddAdditionalSchemes(Schemes* schemes) const {
  schemes->standard_schemes.push_back(kChromeUIScheme);
  schemes->secure_schemes.push_back(kChromeUIScheme);
}

bool WebViewWebClient::IsAppSpecificURL(const GURL& url) const {
  return url.SchemeIs(kChromeUIScheme);
}

std::string WebViewWebClient::GetUserAgent(web::UserAgentType type) const {
  if (CWVWebView.customUserAgent) {
    return base::SysNSStringToUTF8(CWVWebView.customUserAgent);
  } else {
    return web::BuildMobileUserAgent(
        base::SysNSStringToUTF8([CWVWebView userAgentProduct]));
  }
}

std::string_view WebViewWebClient::GetDataResource(
    int resource_id,
    ui::ResourceScaleFactor scale_factor) const {
  return ui::ResourceBundle::GetSharedInstance().GetRawDataResourceForScale(
      resource_id, scale_factor);
}

base::RefCountedMemory* WebViewWebClient::GetDataResourceBytes(
    int resource_id) const {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
      resource_id);
}

std::vector<web::JavaScriptFeature*> WebViewWebClient::GetJavaScriptFeatures(
    web::BrowserState* browser_state) const {
  return {
      autofill::AutofillJavaScriptFeature::GetInstance(),
      autofill::FormHandlersJavaScriptFeature::GetInstance(),
      autofill::SuggestionControllerJavaScriptFeature::GetInstance(),
      language::LanguageDetectionJavaScriptFeature::GetInstance(),
      password_manager::PasswordManagerJavaScriptFeature::GetInstance(),
      security_interstitials::IOSSecurityInterstitialJavaScriptFeature::
          GetInstance(),
      translate::TranslateJavaScriptFeature::GetInstance(),
      WebViewMessageHandlerJavaScriptFeature::FromBrowserState(browser_state),
      WebViewScriptsJavaScriptFeature::FromBrowserState(browser_state)};
}

void WebViewWebClient::PrepareErrorPage(
    web::WebState* web_state,
    const GURL& url,
    NSError* error,
    bool is_post,
    bool is_off_the_record,
    const std::optional<net::SSLInfo>& info,
    int64_t navigation_id,
    base::OnceCallback<void(NSString*)> callback) {
  DCHECK(error);

  CWVWebView* web_view = [CWVWebView webViewForWebState:web_state];
  id<CWVNavigationDelegate> navigation_delegate = web_view.navigationDelegate;

  // |final_underlying_error| should be checked first for any specific error
  // cases such as lookalikes and safebrowsing errors. |info| is only non-empty
  // if this is a SSL related error.
  NSError* final_underlying_error =
      base::ios::GetFinalUnderlyingErrorFromError(error);
  if ([final_underlying_error.domain isEqual:kSafeBrowsingErrorDomain] &&
      [navigation_delegate
          respondsToSelector:@selector(webView:handleUnsafeURLWithHandler:)]) {
    DCHECK_EQ(kUnsafeResourceErrorCode, final_underlying_error.code);
    SafeBrowsingUnsafeResourceContainer* container =
        SafeBrowsingUnsafeResourceContainer::FromWebState(web_state);
    const security_interstitials::UnsafeResource* resource =
        container->GetMainFrameUnsafeResource();
    CWVUnsafeURLHandler* handler =
        [[CWVUnsafeURLHandler alloc] initWithWebState:web_state
                                       unsafeResource:*resource
                                         htmlCallback:std::move(callback)];
    [navigation_delegate webView:web_view handleUnsafeURLWithHandler:handler];
  } else if ([final_underlying_error.domain isEqual:kLookalikeUrlErrorDomain] &&
             [navigation_delegate respondsToSelector:@selector
                                  (webView:handleLookalikeURLWithHandler:)]) {
    DCHECK_EQ(kLookalikeUrlErrorCode, final_underlying_error.code);
    LookalikeUrlContainer* container =
        LookalikeUrlContainer::FromWebState(web_state);
    std::unique_ptr<LookalikeUrlContainer::LookalikeUrlInfo> lookalike_info =
        container->ReleaseLookalikeUrlInfo();
    CWVLookalikeURLHandler* handler = [[CWVLookalikeURLHandler alloc]
        initWithWebState:web_state
        lookalikeURLInfo:std::move(lookalike_info)
            htmlCallback:std::move(callback)];
    [navigation_delegate webView:web_view
        handleLookalikeURLWithHandler:handler];
  } else if (info.has_value() &&
             [navigation_delegate respondsToSelector:@selector
                                  (webView:handleSSLErrorWithHandler:)]) {
    CWVSSLErrorHandler* handler = [[CWVSSLErrorHandler alloc]
             initWithWebState:web_state
                          URL:net::NSURLWithGURL(url)
                        error:error
                      SSLInfo:info.value()
        errorPageHTMLCallback:base::CallbackToBlock(std::move(callback))];
    [navigation_delegate webView:web_view handleSSLErrorWithHandler:handler];
  } else {
    std::move(callback).Run(error.localizedDescription);
  }
}

bool WebViewWebClient::EnableLongPressUIContextMenu() const {
  return CWVWebView.chromeContextMenuEnabled;
}

bool WebViewWebClient::EnableWebInspector(
    web::BrowserState* browser_state) const {
  return CWVWebView.webInspectorEnabled;
}

bool WebViewWebClient::IsInsecureFormWarningEnabled(
    web::BrowserState* browser_state) const {
  // ios/web_view doesn't receive variations seeds at runtime, so this will
  // only ever use the default value of the feature.
  return base::FeatureList::IsEnabled(
      security_interstitials::features::kInsecureFormSubmissionInterstitial);
}

}  // namespace ios_web_view
