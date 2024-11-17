// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/content/navigation/content_navigation_context.h"

#import <optional>

#import "base/notreached.h"
#import "content/public/browser/navigation_handle.h"
#import "net/base/net_errors.h"

namespace web {

std::optional<NSInteger> GetIOSErrorForNetError(int net_error_code) {
  switch (net_error_code) {
    case net::ERR_ACCESS_DENIED:
      // TODO(crbug.com/40257932): no analog for
      // kCFURLErrorBackgroundSessionInUseByAnotherProcess.
      return kCFURLErrorNoPermissionsToReadFile;
    case net::ERR_CONNECTION_ABORTED:
      return kCFURLErrorBackgroundSessionWasDisconnected;
    case net::ERR_FAILED:
      return kCFURLErrorUserAuthenticationRequired;
    case net::ERR_ABORTED:
      // TODO(crbug.com/40257932) no analog for
      // kCFURLErrorUserCancelledAuthentication.
      return kCFURLErrorCancelled;
    case net::ERR_INVALID_URL:
      return kCFURLErrorBadURL;
    case net::ERR_CONNECTION_TIMED_OUT:
      return kCFURLErrorTimedOut;
    case net::ERR_UNKNOWN_URL_SCHEME:
      return kCFURLErrorUnsupportedURL;
    case net::ERR_NAME_NOT_RESOLVED:
      // TODO(crbug.com/40257932) no analog for
      // kCFURLErrorRedirectToNonExistentLocation.
      return kCFURLErrorCannotFindHost;
    // Note, we do no not handle mapping any iOS error code back to
    // ERR_SOCKET_NOT_CONNECTED so we will piggy-back on this code here.
    case net::ERR_SOCKET_NOT_CONNECTED:
    case net::ERR_CONNECTION_FAILED:
      // TODO(crbug.com/40257932) no analog for kCFURLErrorCallIsActive.
      return kCFURLErrorCannotConnectToHost;
    case net::ERR_CONNECTION_CLOSED:
      return kCFURLErrorNetworkConnectionLost;
    case net::ERR_NAME_RESOLUTION_FAILED:
      return kCFURLErrorDNSLookupFailed;
    case net::ERR_TOO_MANY_REDIRECTS:
      return kCFURLErrorHTTPTooManyRedirects;
    case net::ERR_INSUFFICIENT_RESOURCES:
      return kCFURLErrorResourceUnavailable;
    case net::ERR_INTERNET_DISCONNECTED:
      // TODO(crbug.com/40257932): no analog for
      // kCFURLErrorInternationalRoamingOff.
      // TODO(crbug.com/40257932): no analog for kCFURLErrorDataNotAllowed.
      return kCFURLErrorNotConnectedToInternet;
    case net::ERR_INVALID_RESPONSE:
      // TODO(crbug.com/40257932) no analog for kCFURLErrorCannotParseResponse.
      return kCFURLErrorBadServerResponse;
    case net::ERR_EMPTY_RESPONSE:
      return kCFURLErrorZeroByteResource;
    case net::ERR_CONTENT_DECODING_FAILED:
      // TODO(crbug.com/40257932): no analog for kCFURLErrorCannotDecodeRawData.
      return kCFURLErrorCannotDecodeContentData;
    case net::ERR_CONTENT_LENGTH_MISMATCH:
      return kCFURLErrorRequestBodyStreamExhausted;
    case net::ERR_FILE_NOT_FOUND:
      return kCFURLErrorFileDoesNotExist;
    case net::ERR_INVALID_HANDLE:
      return kCFURLErrorFileIsDirectory;
    case net::ERR_FILE_TOO_BIG:
      return kCFURLErrorDataLengthExceedsMaximum;
    case net::ERR_SSL_PROTOCOL_ERROR:
      return kCFURLErrorSecureConnectionFailed;
    case net::ERR_CERT_DATE_INVALID:
      // TODO(crbug.com/40257932): no analog for
      // kCFURLErrorServerCertificateNotYetValid.
      return kCFURLErrorServerCertificateHasBadDate;
    case net::ERR_CERT_AUTHORITY_INVALID:
      // TODO(crbug.com/40257932): no analog for
      // kCFURLErrorServerCertificateHasUnknownRoot.
      return kCFURLErrorServerCertificateUntrusted;
    case net::ERR_BAD_SSL_CLIENT_AUTH_CERT:
      return kCFURLErrorClientCertificateRejected;
    case net::ERR_SSL_CLIENT_AUTH_CERT_NEEDED:
      return kCFURLErrorClientCertificateRequired;
    default:
      break;
  }
  return std::nullopt;
}

NavigationContext* ContentNavigationContext::GetOrCreate(
    content::NavigationHandle* handle,
    WebState* web_state) {
  auto* context = ContentNavigationContext::GetForNavigationHandle(*handle);
  if (!context) {
    ContentNavigationContext::CreateForNavigationHandle(*handle, web_state);
    context = ContentNavigationContext::GetForNavigationHandle(*handle);
  }
  return context;
}

ContentNavigationContext::~ContentNavigationContext() = default;

WebState* ContentNavigationContext::GetWebState() {
  return web_state_;
}

int64_t ContentNavigationContext::GetNavigationId() const {
  return handle_->GetNavigationId();
}

const GURL& ContentNavigationContext::GetUrl() const {
  return handle_->GetURL();
}

bool ContentNavigationContext::HasUserGesture() const {
  return handle_->HasUserGesture();
}

ui::PageTransition ContentNavigationContext::GetPageTransition() const {
  return handle_->GetPageTransition();
}

bool ContentNavigationContext::IsSameDocument() const {
  return handle_->IsSameDocument();
}

bool ContentNavigationContext::HasCommitted() const {
  return handle_->HasCommitted();
}

bool ContentNavigationContext::IsDownload() const {
  return handle_->IsDownload();
}

bool ContentNavigationContext::IsPost() const {
  return handle_->IsPost();
}

NSError* ContentNavigationContext::GetError() const {
  net::Error net_error_code = handle_->GetNetErrorCode();
  if (net_error_code == net::OK) {
    error_ = nil;
  } else {
    auto ios_error_code = GetIOSErrorForNetError(net_error_code);
    CHECK(ios_error_code);
    error_ = [[NSError alloc]
        initWithDomain:NSURLErrorDomain
                  code:*ios_error_code
              userInfo:@{
                @"url" : [NSString stringWithUTF8String:GetUrl().spec().c_str()]
              }];
  }
  return error_;
}

net::HttpResponseHeaders* ContentNavigationContext::GetResponseHeaders() const {
  return const_cast<net::HttpResponseHeaders*>(handle_->GetResponseHeaders());
}

bool ContentNavigationContext::IsRendererInitiated() const {
  return handle_->IsRendererInitiated();
}

HttpsUpgradeType ContentNavigationContext::GetFailedHttpsUpgradeType() const {
  // TODO(crbug.com/40257932): Determine an analog.
  return HttpsUpgradeType::kNone;
}

ContentNavigationContext::ContentNavigationContext(
    content::NavigationHandle& handle,
    WebState* web_state)
    : handle_(handle), web_state_(web_state) {}

NAVIGATION_HANDLE_USER_DATA_KEY_IMPL(ContentNavigationContext);

}  // namespace web
