// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/web_view_web_client.h"

#include <dispatch/dispatch.h>

#include "base/check.h"
#include "base/mac/bundle_locations.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/post_task.h"
#include "components/ssl_errors/error_info.h"
#include "components/strings/grit/components_strings.h"
#include "ios/components/webui/web_ui_url_constants.h"
#include "ios/web/common/user_agent.h"
#include "ios/web/public/security/ssl_status.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"
#import "ios/web_view/internal/cwv_ssl_status_internal.h"
#import "ios/web_view/internal/cwv_web_view_internal.h"
#include "ios/web_view/internal/web_view_browser_state.h"
#import "ios/web_view/internal/web_view_early_page_script_provider.h"
#import "ios/web_view/internal/web_view_web_main_parts.h"
#import "ios/web_view/public/cwv_navigation_delegate.h"
#import "ios/web_view/public/cwv_web_view.h"
#include "net/cert/cert_status_flags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {
namespace {
// Returns an autoreleased string containing the JavaScript loaded from a
// bundled resource file with the given name (excluding extension).
NSString* GetPageScript(NSString* script_file_name) {
  DCHECK(script_file_name);
  NSString* path =
      [base::mac::FrameworkBundle() pathForResource:script_file_name
                                             ofType:@"js"];
  DCHECK(path) << "Script file not found: "
               << base::SysNSStringToUTF8(script_file_name) << ".js";
  NSError* error = nil;
  NSString* content = [NSString stringWithContentsOfFile:path
                                                encoding:NSUTF8StringEncoding
                                                   error:&error];
  DCHECK(!error) << "Error fetching script: "
                 << base::SysNSStringToUTF8(error.description);
  DCHECK(content);
  return content;
}
}  // namespace

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

base::StringPiece WebViewWebClient::GetDataResource(
    int resource_id,
    ui::ScaleFactor scale_factor) const {
  return ui::ResourceBundle::GetSharedInstance().GetRawDataResourceForScale(
      resource_id, scale_factor);
}

base::RefCountedMemory* WebViewWebClient::GetDataResourceBytes(
    int resource_id) const {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
      resource_id);
}

NSString* WebViewWebClient::GetDocumentStartScriptForAllFrames(
    web::BrowserState* browser_state) const {
  return GetPageScript(@"web_view_all_frames");
}

NSString* WebViewWebClient::GetDocumentStartScriptForMainFrame(
    web::BrowserState* browser_state) const {
  NSMutableArray* scripts = [NSMutableArray array];

  WebViewEarlyPageScriptProvider& provider =
      WebViewEarlyPageScriptProvider::FromBrowserState(browser_state);
  [scripts addObject:provider.GetScript()];

  [scripts addObject:GetPageScript(@"web_view_main_frame")];

  return [scripts componentsJoinedByString:@";"];
}

std::u16string WebViewWebClient::GetPluginNotSupportedText() const {
  return l10n_util::GetStringUTF16(IDS_PLUGIN_NOT_SUPPORTED);
}

void WebViewWebClient::AllowCertificateError(
    web::WebState* web_state,
    int net_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    bool overridable,
    int64_t navigation_id,
    base::OnceCallback<void(bool)> callback) {
  CWVWebView* web_view = [CWVWebView webViewForWebState:web_state];

  SEL selector = @selector
      (webView:didFailNavigationWithSSLError:overridable:decisionHandler:);
  if ([web_view.navigationDelegate respondsToSelector:selector]) {
    CWVCertStatus cert_status =
        CWVCertStatusFromNetCertStatus(ssl_info.cert_status);
    ssl_errors::ErrorInfo error_info = ssl_errors::ErrorInfo::CreateError(
        ssl_errors::ErrorInfo::NetErrorToErrorType(net_error),
        ssl_info.cert.get(), request_url);
    NSString* error_description =
        base::SysUTF16ToNSString(error_info.short_description());
    NSError* error =
        [NSError errorWithDomain:NSURLErrorDomain
                            code:NSURLErrorSecureConnectionFailed
                        userInfo:@{
                          NSLocalizedDescriptionKey : error_description,
                          CWVCertStatusKey : @(cert_status),
                        }];

    __block base::OnceCallback<void(bool)> local_callback = std::move(callback);
    void (^decisionHandler)(CWVSSLErrorDecision) =
        ^(CWVSSLErrorDecision decision) {
          switch (decision) {
            case CWVSSLErrorDecisionOverrideErrorAndReload: {
              std::move(local_callback).Run(true);
              break;
            }
            case CWVSSLErrorDecisionDoNothing: {
              std::move(local_callback).Run(false);
              break;
            }
          }
        };

    [web_view.navigationDelegate webView:web_view
           didFailNavigationWithSSLError:error
                             overridable:overridable
                         decisionHandler:decisionHandler];
  } else {
    std::move(callback).Run(false);
  }
}

bool WebViewWebClient::EnableLongPressAndForceTouchHandling() const {
  return CWVWebView.chromeLongPressAndForceTouchHandlingEnabled;
}

}  // namespace ios_web_view
