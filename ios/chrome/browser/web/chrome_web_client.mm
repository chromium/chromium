// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/web/chrome_web_client.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/mac/bundle_locations.h"
#include "base/strings/sys_string_conversions.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/strings/grit/components_strings.h"
#include "components/version_info/version_info.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_about_rewriter.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_switches.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/experimental_flags.h"
#include "ios/chrome/browser/ios_chrome_main_parts.h"
#include "ios/chrome/browser/passwords/credential_manager_features.h"
#include "ios/chrome/browser/ssl/ios_ssl_error_handler.h"
#import "ios/chrome/browser/ui/chrome_web_view_factory.h"
#include "ios/chrome/browser/unzip/unzip_service_creator.h"
#import "ios/chrome/browser/web/error_page_util.h"
#include "ios/chrome/grit/ios_resources.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/public/provider/chrome/browser/voice/audio_session_controller.h"
#include "ios/public/provider/chrome/browser/voice/voice_search_provider.h"
#include "ios/web/public/browser_url_rewriter.h"
#include "ios/web/public/service_names.mojom.h"
#include "ios/web/public/user_agent.h"
#include "net/http/http_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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

ChromeWebClient::ChromeWebClient() {}

ChromeWebClient::~ChromeWebClient() {}

std::unique_ptr<web::WebMainParts> ChromeWebClient::CreateWebMainParts() {
  return std::make_unique<IOSChromeMainParts>(
      *base::CommandLine::ForCurrentProcess());
}

void ChromeWebClient::PreWebViewCreation() const {
  // Initialize the audio session to allow a web page's audio to continue
  // playing after the app is backgrounded.
  VoiceSearchProvider* voice_provider =
      ios::GetChromeBrowserProvider()->GetVoiceSearchProvider();
  if (voice_provider) {
    AudioSessionController* audio_controller =
        voice_provider->GetAudioSessionController();
    if (audio_controller) {
      audio_controller->InitializeSessionIfNecessary();
    }
  }
}

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

base::string16 ChromeWebClient::GetPluginNotSupportedText() const {
  return l10n_util::GetStringUTF16(IDS_PLUGIN_NOT_SUPPORTED);
}

std::string ChromeWebClient::GetProduct() const {
  std::string product("CriOS/");
  product += version_info::GetVersionNumber();
  return product;
}

std::string ChromeWebClient::GetUserAgent(web::UserAgentType type) const {
  // The user agent should not be requested for app-specific URLs.
  DCHECK_NE(type, web::UserAgentType::NONE);

  // Using desktop user agent overrides a command-line user agent, so that
  // request desktop site can still work when using an overridden UA.
  if (type == web::UserAgentType::DESKTOP)
    return base::SysNSStringToUTF8(ChromeWebView::kDesktopUserAgent);

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kUserAgent)) {
    std::string user_agent =
        command_line->GetSwitchValueASCII(switches::kUserAgent);
    if (net::HttpUtil::IsValidHeaderValue(user_agent))
      return user_agent;
    LOG(WARNING) << "Ignored invalid value for flag --" << switches::kUserAgent;
  }

  return web::BuildUserAgentFromProduct(GetProduct());
}

base::string16 ChromeWebClient::GetLocalizedString(int message_id) const {
  return l10n_util::GetStringUTF16(message_id);
}

base::StringPiece ChromeWebClient::GetDataResource(
    int resource_id,
    ui::ScaleFactor scale_factor) const {
  return ui::ResourceBundle::GetSharedInstance().GetRawDataResourceForScale(
      resource_id, scale_factor);
}

base::RefCountedMemory* ChromeWebClient::GetDataResourceBytes(
    int resource_id) const {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
      resource_id);
}

std::unique_ptr<base::Value> ChromeWebClient::GetServiceManifestOverlay(
    base::StringPiece name) {
  int identifier = -1;
  if (name == web::mojom::kBrowserServiceName)
    identifier = IDR_CHROME_BROWSER_MANIFEST_OVERLAY;

  if (identifier == -1)
    return nullptr;

  base::StringPiece manifest_contents =
      ui::ResourceBundle::GetSharedInstance().GetRawDataResourceForScale(
          identifier, ui::ScaleFactor::SCALE_FACTOR_NONE);
  return base::JSONReader::Read(manifest_contents);
}

void ChromeWebClient::GetAdditionalWebUISchemes(
    std::vector<std::string>* additional_schemes) {
  additional_schemes->push_back(dom_distiller::kDomDistillerScheme);
}

void ChromeWebClient::PostBrowserURLRewriterCreation(
    web::BrowserURLRewriter* rewriter) {
  rewriter->AddURLRewriter(&WillHandleWebBrowserAboutURL);
}

NSString* ChromeWebClient::GetDocumentStartScriptForAllFrames(
    web::BrowserState* browser_state) const {
  return GetPageScript(@"chrome_bundle_all_frames");
}

NSString* ChromeWebClient::GetDocumentStartScriptForMainFrame(
    web::BrowserState* browser_state) const {
  NSMutableArray* scripts = [NSMutableArray array];
  [scripts addObject:GetPageScript(@"chrome_bundle_main_frame")];

  if (base::FeatureList::IsEnabled(features::kCredentialManager)) {
    [scripts addObject:GetPageScript(@"credential_manager")];
  }

  [scripts addObject:GetPageScript(@"payment_request")];

  return [scripts componentsJoinedByString:@";"];
}

void ChromeWebClient::AllowCertificateError(
    web::WebState* web_state,
    int cert_error,
    const net::SSLInfo& info,
    const GURL& request_url,
    bool overridable,
    const base::Callback<void(bool)>& callback) {
  // TODO(crbug.com/760873): IOSSSLErrorHandler will present an interstitial
  // for the user to decide if it is safe to proceed.
  // Handle the case of web_state not presenting UI to users like prerender tabs
  // or web_state used to fetch offline content in Reading List.
  IOSSSLErrorHandler::HandleSSLError(web_state, cert_error, info, request_url,
                                     overridable, callback);
}

void ChromeWebClient::PrepareErrorPage(NSError* error,
                                       bool is_post,
                                       bool is_off_the_record,
                                       NSString** error_html) {
  DCHECK(error);

  // Return an empty page for the NTP.
  NSString* url_spec = error.userInfo[NSURLErrorFailingURLStringErrorKey];
  GURL url(base::SysNSStringToUTF16(url_spec));
  if (url.GetOrigin() == kChromeUINewTabURL) {
    *error_html = @"";
    return;
  }

  *error_html = GetErrorPage(error, is_post, is_off_the_record);
}

void ChromeWebClient::RegisterServices(StaticServiceMap* services) {
  // The Unzip service is used by the component updater.
  RegisterUnzipService(services);
}
