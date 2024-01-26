// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/net/protocol_handler_util.h"

#include <string>

#include "base/base64.h"
#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "base/time/time.h"
#include "ios/net/crn_http_url_response.h"
#import "net/base/apple/url_conversions.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_version.h"
#include "net/url_request/referrer_policy.h"
#include "net/url_request/url_request.h"
#include "url/buildflags.h"
#include "url/gurl.h"

#if !BUILDFLAG(USE_PLATFORM_ICU_ALTERNATIVES)
#include "base/i18n/encoding_detection.h"  // nogncheck
#include "base/i18n/icu_string_conversions.h"  // nogncheck
#endif  // !BUILDFLAG(USE_PLATFORM_ICU_ALTERNATIVES)

namespace {

// "Content-Type" HTTP header.
NSString* const kContentType = @"Content-Type";

}  // namespace

namespace net {

NSString* const kNSErrorDomain = @"org.chromium.net.ErrorDomain";

NSError* GetIOSError(NSInteger ns_error_code,
                     int net_error_code,
                     NSString* url,
                     const base::Time& creation_time) {
  // The error we pass through has the domain NSURLErrorDomain, an IOS error
  // code, and a userInfo dictionary in which we smuggle more detailed info
  // about the error from our network stack. This dictionary contains the
  // failing URL, and a nested error in which we deposit the original error code
  // passed in from the Chrome network stack.
  // The nested error has domain:kNSErrorDomain, code:|original_error_code|,
  // and userInfo:nil; this NSError is keyed in the dictionary with
  // NSUnderlyingErrorKey.
  NSDate* creation_date = creation_time.ToNSDate();
  DCHECK(creation_date);
  NSError* underlying_error =
      [NSError errorWithDomain:kNSErrorDomain code:net_error_code userInfo:nil];
  DCHECK(url);
  NSDictionary* dictionary = @{
      NSURLErrorFailingURLStringErrorKey : url,
      @"CreationDate" : creation_date,
      NSUnderlyingErrorKey : underlying_error,
  };
  return [NSError errorWithDomain:NSURLErrorDomain
                             code:ns_error_code
                         userInfo:dictionary];
}

NSURLResponse* GetNSURLResponseForRequest(URLRequest* request) {
  NSURL* url = NSURLWithGURL(request->url());
  DCHECK(url);

    // Iterate over all the headers and copy them.
    bool has_content_type_header = false;
    NSMutableDictionary* header_fields = [NSMutableDictionary dictionary];
    HttpResponseHeaders* headers = request->response_headers();
    if (headers != nullptr) {
      size_t iter = 0;
      std::string name, value;
      while (headers->EnumerateHeaderLines(&iter, &name, &value)) {
        NSString* key = base::SysUTF8ToNSString(name);
        if (!key) {
          DLOG(ERROR) << "Header name is not in UTF8: " << name;
          // Skip the invalid header.
          continue;
        }
        // Do not copy "Cache-Control" headers as we provide our own controls.
        if ([key caseInsensitiveCompare:@"cache-control"] == NSOrderedSame)
          continue;
        if ([key caseInsensitiveCompare:kContentType] == NSOrderedSame) {
          key = kContentType;
          has_content_type_header = true;
        }

        // Handle bad encoding.
        NSString* v = base::SysUTF8ToNSString(value);
        if (!v) {
          DLOG(ERROR) << "Header \"" << name << "\" is not in UTF8: " << value;
#if BUILDFLAG(USE_PLATFORM_ICU_ALTERNATIVES)
          DCHECK(FALSE) << "ICU support is required, but not included.";
          continue;
#else
          // Infer the encoding, or skip the header if it's not possible.
          std::string encoding;
          if (!base::DetectEncoding(value, &encoding))
            continue;
          std::string value_utf8;
          if (!base::ConvertToUtf8AndNormalize(value, encoding, &value_utf8))
            continue;
          v = base::SysUTF8ToNSString(value_utf8);
          DCHECK(v);
#endif  // !BUILDFLAG(USE_PLATFORM_ICU_ALTERNATIVES)
        }

        // Duplicate keys are appended using a comma separator (RFC 2616).
        NSMutableString* existing = [header_fields objectForKey:key];
        if (existing) {
          [existing appendFormat:@",%@", v];
        } else {
          [header_fields setObject:[NSMutableString stringWithString:v]
                            forKey:key];
        }
      }
    }

    // WebUI does not define a "Content-Type" header. Use the MIME type instead.
    if (!has_content_type_header) {
      std::string mime_type = "";
      request->GetMimeType(&mime_type);
      NSString* type = base::SysUTF8ToNSString(mime_type);
      if ([type length])
        [header_fields setObject:type forKey:kContentType];
    }
    NSString* content_type = [header_fields objectForKey:kContentType];
    if (content_type) {
      NSRange range = [content_type rangeOfString:@","];
      // If there are several "Content-Type" headers, keep only the first one.
      if (range.location != NSNotFound) {
        [header_fields setObject:[content_type substringToIndex:range.location]
                          forKey:kContentType];
      }
    }

    // Use a "no-store" cache control to ensure that the response is not cached
    // by the system. See b/7045043.
    [header_fields setObject:@"no-store" forKey:@"Cache-Control"];

    // Parse the HTTP version.
    NSString* version_string = @"HTTP/1.1";
    if (headers) {
      const HttpVersion& http_version = headers->GetHttpVersion();
      version_string = [NSString stringWithFormat:@"HTTP/%hu.%hu",
                                                  http_version.major_value(),
                                                  http_version.minor_value()];
    }

    return [[CRNHTTPURLResponse alloc] initWithURL:url
                                        statusCode:request->GetResponseCode()
                                       HTTPVersion:version_string
                                      headerFields:header_fields];
}

void CopyHttpHeaders(NSURLRequest* in_request, URLRequest* out_request) {
  DCHECK(out_request->extra_request_headers().IsEmpty());
  NSDictionary* headers = [in_request allHTTPHeaderFields];
  HttpRequestHeaders net_headers;
  NSString* key;
  for (key in headers) {
    if ([key isEqualToString:@"Referer"]) {
      // The referrer must be set through the set_referrer method rather than as
      // a header.
      out_request->SetReferrer(
          base::SysNSStringToUTF8([headers objectForKey:key]));
      // If the referrer is explicitly set, we don't want the network stack to
      // strip it.
      out_request->set_referrer_policy(net::ReferrerPolicy::NEVER_CLEAR);
      continue;
    }
    // Copy over all headers that were set on NSURLRequest
    NSString* value = [headers objectForKey:key];
    net_headers.SetHeader(base::SysNSStringToUTF8(key),
                          base::SysNSStringToUTF8(value));
  }
  // Set default values for some missing headers.
  // The "Accept" header is defined by Webkit on the desktop version.
  net_headers.SetHeaderIfMissing("Accept", "*/*");
  // The custom NSURLProtocol example from Apple adds a default "Content-Type"
  // header for non-empty POST requests. This suggests that this header can be
  // missing, and Chrome network stack does not add it by itself.
  if (out_request->has_upload() && out_request->method() == "POST") {
    DLOG_IF(WARNING, !net_headers.HasHeader(HttpRequestHeaders::kContentType))
        << "Missing \"Content-Type\" header in POST request.";
    net_headers.SetHeaderIfMissing(HttpRequestHeaders::kContentType,
                                   "application/x-www-form-urlencoded");
  }
  out_request->SetExtraRequestHeaders(net_headers);
}

}  // namespace net
