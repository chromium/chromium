// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/net/http_response_headers_util.h"

#import <Foundation/Foundation.h>

#include "base/check.h"
#include "base/format_macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "net/http/http_util.h"

namespace {

// String format used to create the http status line from the status code and
// its localized description.
char const kHttpStatusLineFormat[] = "HTTP %" PRIdNS " DummyStatusDescription";

// Tries to decode `string` as if it was a "utf-8" encoded string incorrectly
// decoded as "latin1" encoded string. Returns the original string if it does
// not appear to be a mis-decoded string.
//
// HTTP headers have no encoding, but NSHTTPURLResponse decode them as if they
// were using "latin1" encoding as the rfc recommends (see [1]). Servers do
// sometime sends data in "utf-8" encoding. Interpreting "utf-8" encoded string
// as "latin1" results in garbled characters as soon as the string contains non
// US-ASCII characters (aka mojibake). Hopefully, this incorrect decoding is
// reversible as "latin1" maps each bytes to the same unicode character (i.e.
// byte '\xab' maps to '\u00ab'). See [2] for example of a web server serving
// "utf-8" encoded string incorrectly decoded.
//
// [1]: https://www.rfc-editor.org/rfc/rfc7230#section-3.2.4.
// [2]: https://crbug.com/1333351
NSString* FixNSStringIncorrectlyDecodedAsLatin1(NSString* string) {
  // Check if the string contains any character above '\u007f'. If not,
  // then the "latin1" and "utf-8" representation are identical, and there
  // is no need to allocate memory for the conversion.
  static NSCharacterSet* non_ascii_characters = [NSCharacterSet
      characterSetWithRange:NSMakeRange(0x0080, 0x10ffff - 0x0080)];

  NSRange range = [string rangeOfCharacterFromSet:non_ascii_characters];
  if (range.location == NSNotFound && range.length == 0)
    return string;

  // Try to save the string as "latin1". Will fail if it does contains
  // characters that falls out of the "latin1" range (i.e. after '\u00ff').
  NSData* data = [string dataUsingEncoding:NSISOLatin1StringEncoding
                      allowLossyConversion:NO];
  if (!data)
    return string;

  // Try to load the saved data as "utf-8". Will fail if the string is
  // not encoded in "utf-8". In that case, it was probably not garbled
  // and the original string needs to be used. This will be the case for
  // strings that are genuinely encoded in "latin1".
  NSString* decoded = [[NSString alloc] initWithData:data
                                            encoding:NSUTF8StringEncoding];
  if (!decoded)
    return string;

  return decoded;
}

}  // anonymous namespace

namespace net {

scoped_refptr<HttpResponseHeaders> CreateHeadersFromNSHTTPURLResponse(
    NSHTTPURLResponse* response) {
  DCHECK(response);
  // Initialize the header with a generated status line.
  scoped_refptr<HttpResponseHeaders> http_headers(new HttpResponseHeaders(
      base::StringPrintf(kHttpStatusLineFormat, response.statusCode)));

  // Iterate through |response|'s headers and add them to |http_headers|.
  [response.allHeaderFields enumerateKeysAndObjectsUsingBlock:^(
                                NSString* name, NSString* value, BOOL*) {
    const std::string header_name = base::SysNSStringToUTF8(name);
    if (!HttpUtil::IsValidHeaderName(header_name))
      return;

    const std::string header_value =
        base::SysNSStringToUTF8(FixNSStringIncorrectlyDecodedAsLatin1(value));
    if (!HttpUtil::IsValidHeaderValue(header_value))
      return;

    http_headers->AddHeader(header_name, header_value);
  }];
  return http_headers;
}

}  // namespace net
