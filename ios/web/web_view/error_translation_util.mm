// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_view/error_translation_util.h"

#import <CFNetwork/CFNetwork.h>
#import <Foundation/Foundation.h>

#import "base/ios/ns_error_util.h"
#import "ios/net/protocol_handler_util.h"
#import "ios/web/public/web_client.h"
#import "net/base/apple/url_conversions.h"
#import "net/base/net_errors.h"
#import "url/gurl.h"

namespace web {

bool GetNetErrorFromIOSErrorCode(NSInteger ios_error_code,
                                 int* net_error_code,
                                 NSURL* url) {
  DCHECK(net_error_code);
  bool translation_success = true;
  switch (ios_error_code) {
    case kCFURLErrorBackgroundSessionInUseByAnotherProcess:
      *net_error_code = net::ERR_ACCESS_DENIED;
      break;
    case kCFURLErrorBackgroundSessionWasDisconnected:
      *net_error_code = net::ERR_CONNECTION_ABORTED;
      break;
    case kCFURLErrorUnknown:
      *net_error_code = net::ERR_FAILED;
      break;
    case kCFURLErrorCancelled:
      *net_error_code = net::ERR_ABORTED;
      break;
    case kCFURLErrorBadURL:
      *net_error_code = net::ERR_INVALID_URL;
      break;
    case kCFURLErrorTimedOut:
      *net_error_code = net::ERR_CONNECTION_TIMED_OUT;
      break;
    case kCFURLErrorUnsupportedURL:
      if (GetWebClient()->IsAppSpecificURL(net::GURLWithNSURL(url))) {
        // Scheme is valid, but URL is not supported.
        *net_error_code = net::ERR_INVALID_URL;
      } else {
        // Scheme is not app-specific and not supported by WebState.
        *net_error_code = net::ERR_UNKNOWN_URL_SCHEME;
      }
      break;
    case kCFURLErrorCannotFindHost:
      *net_error_code = net::ERR_NAME_NOT_RESOLVED;
      break;
    case kCFURLErrorCannotConnectToHost:
      *net_error_code = net::ERR_CONNECTION_FAILED;
      break;
    case kCFURLErrorNetworkConnectionLost:
      // This looks like catch-all code for errors like ERR_CONNECTION_CLOSED,
      // ERR_EMPTY_RESPONSE, ERR_NETWORK_CHANGED or ERR_CONNECTION_RESET.
      // ERR_CONNECTION_CLOSED is too specific for this case, but there is no
      // better cross platform analogue.
      *net_error_code = net::ERR_CONNECTION_CLOSED;
      break;
    case kCFURLErrorDNSLookupFailed:
      *net_error_code = net::ERR_NAME_RESOLUTION_FAILED;
      break;
    case kCFURLErrorHTTPTooManyRedirects:
      *net_error_code = net::ERR_TOO_MANY_REDIRECTS;
      break;
    case kCFURLErrorResourceUnavailable:
      *net_error_code = net::ERR_INSUFFICIENT_RESOURCES;
      break;
    case kCFURLErrorNotConnectedToInternet:
      *net_error_code = net::ERR_INTERNET_DISCONNECTED;
      break;
    case kCFURLErrorRedirectToNonExistentLocation:
      *net_error_code = net::ERR_NAME_NOT_RESOLVED;
      break;
    case kCFURLErrorBadServerResponse:
      *net_error_code = net::ERR_INVALID_RESPONSE;
      break;
    case kCFURLErrorUserCancelledAuthentication:
      *net_error_code = net::ERR_ABORTED;
      break;
    case kCFURLErrorUserAuthenticationRequired:
      *net_error_code = net::ERR_FAILED;
      break;
    case kCFURLErrorZeroByteResource:
      *net_error_code = net::ERR_EMPTY_RESPONSE;
      break;
    case kCFURLErrorCannotDecodeRawData:
      *net_error_code = net::ERR_CONTENT_DECODING_FAILED;
      break;
    case kCFURLErrorCannotDecodeContentData:
      *net_error_code = net::ERR_CONTENT_DECODING_FAILED;
      break;
    case kCFURLErrorCannotParseResponse:
      *net_error_code = net::ERR_INVALID_RESPONSE;
      break;
    case kCFURLErrorInternationalRoamingOff:
      *net_error_code = net::ERR_INTERNET_DISCONNECTED;
      break;
    case kCFURLErrorCallIsActive:
      *net_error_code = net::ERR_CONNECTION_FAILED;
      break;
    case kCFURLErrorDataNotAllowed:
      *net_error_code = net::ERR_INTERNET_DISCONNECTED;
      break;
    case kCFURLErrorRequestBodyStreamExhausted:
      *net_error_code = net::ERR_CONTENT_LENGTH_MISMATCH;
      break;
    case kCFURLErrorFileDoesNotExist:
      *net_error_code = net::ERR_FILE_NOT_FOUND;
      break;
    case kCFURLErrorFileIsDirectory:
      *net_error_code = net::ERR_INVALID_HANDLE;
      break;
    case kCFURLErrorNoPermissionsToReadFile:
      *net_error_code = net::ERR_ACCESS_DENIED;
      break;
    case kCFURLErrorDataLengthExceedsMaximum:
      *net_error_code = net::ERR_FILE_TOO_BIG;
      break;
    case kCFURLErrorSecureConnectionFailed:
      *net_error_code = net::ERR_SSL_PROTOCOL_ERROR;
      break;
    case kCFURLErrorServerCertificateHasBadDate:
      *net_error_code = net::ERR_CERT_DATE_INVALID;
      break;
    case kCFURLErrorServerCertificateUntrusted:
      *net_error_code = net::ERR_CERT_AUTHORITY_INVALID;
      break;
    case kCFURLErrorServerCertificateHasUnknownRoot:
      *net_error_code = net::ERR_CERT_AUTHORITY_INVALID;
      break;
    case kCFURLErrorServerCertificateNotYetValid:
      *net_error_code = net::ERR_CERT_DATE_INVALID;
      break;
    case kCFURLErrorClientCertificateRejected:
      *net_error_code = net::ERR_BAD_SSL_CLIENT_AUTH_CERT;
      break;
    case kCFURLErrorClientCertificateRequired:
      *net_error_code = net::ERR_SSL_CLIENT_AUTH_CERT_NEEDED;
      break;
    default:
      translation_success = false;
      break;
  }
  return translation_success;
}

NSError* NetErrorFromError(NSError* error) {
  DCHECK(error);
  NSError* underlying_error =
      base::ios::GetFinalUnderlyingErrorFromError(error);

  int net_error_code = net::ERR_FAILED;
  if ([underlying_error.domain isEqualToString:NSURLErrorDomain] ||
      [underlying_error.domain
          isEqualToString:static_cast<NSString*>(kCFErrorDomainCFNetwork)]) {
    // Attempt to translate NSURL and CFNetwork error codes into their
    // corresponding net error codes.
    NSString* url_spec = error.userInfo[NSURLErrorFailingURLStringErrorKey];
    NSURL* url = url_spec ? [NSURL URLWithString:url_spec] : nil;
    GetNetErrorFromIOSErrorCode(underlying_error.code, &net_error_code, url);
  }
  return NetErrorFromError(error, net_error_code);
}

NSError* NetErrorFromError(NSError* error, int net_error_code) {
  DCHECK(error);
  NSError* net_error =
      [NSError errorWithDomain:net::kNSErrorDomain
                          code:static_cast<NSInteger>(net_error_code)
                      userInfo:nil];
  return base::ios::ErrorWithAppendedUnderlyingError(error, net_error);
}

}  // namespace web
