// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/u2f/u2f_controller.h"

#include "base/json/string_escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/google/core/common/google_util.h"
#include "crypto/random.h"
#import "ios/chrome/browser/chrome_url_util.h"
#include "ios/chrome/common/x_callback_url.h"
#include "ios/web/public/deprecated/url_verification_constants.h"
#include "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frame_util.h"
#import "ios/web/public/web_state.h"
#include "net/base/url_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kIsU2FKey[] = "isU2F";
const char kTabIDKey[] = "tabID";
const char kRequestUUIDKey[] = "requestUUID";
const char kU2FCallbackURL[] = "u2f-callback";
const char kKeyHandleKey[] = "keyHandle";
const char kSignatureDataKey[] = "signatureData";
const char kClientDataKey[] = "clientData";
const char kRegistrationDataKey[] = "registrationData";
const char kErrorKey[] = "error";
const char kErrorCodeKey[] = "errorCode";
const char kRequestIDKey[] = "requestId";
}

@interface U2FController ()

// Generates the JS string to be injected onto the web page to send FIDO U2F
// requests' results from the U2F callback URL.
- (std::u16string)JSStringFromReponseURL:(const GURL&)URL;

// Checks if the source URL has Google domain or allow-listed test domain.
- (BOOL)shouldAllowSourceURL:(const GURL&)sourceURL;

@end

@implementation U2FController {
  // Maps request UUID NString to URL NSString of the tab making the request.
  NSMutableDictionary* _requestToURLMap;
}

#pragma mark - Public methods

- (instancetype)init {
  self = [super init];
  if (self) {
    _requestToURLMap = [[NSMutableDictionary alloc] init];
  }
  return self;
}

- (GURL)XCallbackFromRequestURL:(const GURL&)requestURL
                      originURL:(const GURL&)originURL
                         tabURL:(const GURL&)tabURL
                          tabID:(NSString*)tabID {
  // Check if origin is secure.
  if (!originURL.SchemeIsCryptographic()) {
    return GURL();
  }

  // Check if the webpage has Google or allowed test domain.
  if (![self shouldAllowSourceURL:originURL]) {
    return GURL();
  }

  // Generate requestUUID and bookkeep it.
  char randomBytes[16];
  crypto::RandBytes(randomBytes, sizeof(randomBytes));
  std::string randomUUID(base::HexEncode(randomBytes, sizeof(randomBytes)));
  NSString* requestUUID = base::SysUTF8ToNSString(randomUUID);
  NSString* tabURLString = base::SysUTF8ToNSString(tabURL.spec());
  [_requestToURLMap setObject:tabURLString forKey:requestUUID];

  // Compose callback string.
  NSString* chromeScheme =
      [[ChromeAppConstants sharedInstance] bundleURLScheme];
  GURL successOrErrorURL(base::StringPrintf(
      "%s://%s/", base::SysNSStringToUTF8(chromeScheme).c_str(),
      kU2FCallbackURL));
  successOrErrorURL = net::AppendQueryParameter(successOrErrorURL, kTabIDKey,
                                                base::SysNSStringToUTF8(tabID));
  successOrErrorURL =
      net::AppendQueryParameter(successOrErrorURL, kRequestUUIDKey, randomUUID);
  successOrErrorURL =
      net::AppendQueryParameter(successOrErrorURL, kIsU2FKey, "1");

  std::map<std::string, std::string> params = {
      {"origin", originURL.spec()}, {"data", requestURL.query()},
  };

  return CreateXCallbackURLWithParameters("u2f-x-callback", "auth",
                                          successOrErrorURL, successOrErrorURL,
                                          GURL(), params);
}

- (void)evaluateU2FResultFromU2FURL:(const GURL&)U2FURL
                           webState:(web::WebState*)webState {
  if (U2FURL.host() != kU2FCallbackURL) {
    // If unexpected callback host is in callback, ignore it.
    return;
  }

  std::string requestUUID;
  if (!net::GetValueForKeyInQuery(U2FURL, std::string(kRequestUUIDKey),
                                  &requestUUID)) {
    // If requestUUID is not in callback, ignore it.
    return;
  }

  NSString* originalTabURL =
      [_requestToURLMap objectForKey:base::SysUTF8ToNSString(requestUUID)];
  if (!originalTabURL) {
    // If no corresponding original URL, ignore it.
    return;
  }

  web::WebFrame* mainFrame = web::GetMainFrame(webState);
  if (!mainFrame) {
    return;
  }

  // If the URLs match and the page URL is trusted, inject the response JS.
  web::URLVerificationTrustLevel trustLevel =
      web::URLVerificationTrustLevel::kNone;
  GURL currentTabURL = webState->GetCurrentURL(&trustLevel);
  if (trustLevel == web::URLVerificationTrustLevel::kAbsolute &&
      GURL(base::SysNSStringToUTF8(originalTabURL)) == currentTabURL) {
    mainFrame->ExecuteJavaScript([self JSStringFromReponseURL:U2FURL]);
  }

  // Remove bookkept URL for returned U2F call.
  [_requestToURLMap removeObjectForKey:base::SysUTF8ToNSString(requestUUID)];
}

#pragma mark - Helper method

- (std::u16string)JSStringFromReponseURL:(const GURL&)URL {
  std::string requestID;
  if (!net::GetValueForKeyInQuery(URL, kRequestIDKey, &requestID)) {
    return std::u16string();
  }

  std::string JSString;
  std::string quotedRequestID = base::GetQuotedJSONString(requestID);

  std::string errorCode;
  std::string registrationData;
  std::string signatureData;
  if (net::GetValueForKeyInQuery(URL, kErrorKey, &errorCode)) {
    std::string quotedErrorCodeKey = base::GetQuotedJSONString(kErrorCodeKey);
    JSString = base::StringPrintf(
        "u2f.callbackMap_[%s]({%s: %d})", quotedRequestID.c_str(),
        quotedErrorCodeKey.c_str(), std::stoi(errorCode));
  } else if (net::GetValueForKeyInQuery(URL, kRegistrationDataKey,
                                        &registrationData)) {
    std::string clientData;
    std::string quotedRegistrationDataKey =
        base::GetQuotedJSONString(kRegistrationDataKey);
    std::string quotedRegistrationData =
        base::GetQuotedJSONString(registrationData);
    std::string quotedClientDataKey = base::GetQuotedJSONString(kClientDataKey);
    net::GetValueForKeyInQuery(URL, kClientDataKey, &clientData);
    std::string quotedClientData = base::GetQuotedJSONString(clientData);
    JSString = base::StringPrintf(
        "u2f.callbackMap_[%s]({%s: %s, %s: %s})", quotedRequestID.c_str(),
        quotedRegistrationDataKey.c_str(), quotedRegistrationData.c_str(),
        quotedClientDataKey.c_str(), quotedClientData.c_str());
  } else if (net::GetValueForKeyInQuery(URL, kSignatureDataKey,
                                        &signatureData)) {
    std::string keyHandle;
    std::string signatureData;
    std::string clientData;
    std::string quotedKeyHandleKey = base::GetQuotedJSONString(kKeyHandleKey);
    net::GetValueForKeyInQuery(URL, kKeyHandleKey, &keyHandle);
    std::string quotedKeyHandle = base::GetQuotedJSONString(keyHandle);
    std::string quotedSignatureDataKey =
        base::GetQuotedJSONString(kSignatureDataKey);
    net::GetValueForKeyInQuery(URL, kSignatureDataKey, &signatureData);
    std::string quotedSignatureData = base::GetQuotedJSONString(signatureData);
    std::string quotedClientDataKey = base::GetQuotedJSONString(kClientDataKey);
    net::GetValueForKeyInQuery(URL, kClientDataKey, &clientData);
    std::string quotedClientData = base::GetQuotedJSONString(clientData);
    JSString = base::StringPrintf(
        "u2f.callbackMap_[%s]({%s: %s, %s: %s, %s: %s})",
        quotedRequestID.c_str(), quotedKeyHandleKey.c_str(),
        quotedKeyHandle.c_str(), quotedSignatureDataKey.c_str(),
        quotedSignatureData.c_str(), quotedClientDataKey.c_str(),
        quotedClientData.c_str());
  }
  return base::UTF8ToUTF16(JSString);
}

- (BOOL)shouldAllowSourceURL:(const GURL&)sourceURL {
  if (google_util::IsGoogleDomainUrl(
          sourceURL, google_util::ALLOW_SUBDOMAIN,
          google_util::DISALLOW_NON_STANDARD_PORTS)) {
    return YES;
  }

  NSString* sourceDomain = base::SysUTF8ToNSString(sourceURL.host());
  // Convert this condition to checking membership in a set if any new cases
  // need to be added.
  return [sourceDomain isEqualToString:@"u2fdemo.appspot.com"] ||
         [sourceDomain
             isEqualToString:@"chromeiostesting-dot-u2fdemo.appspot.com"];
}

@end
