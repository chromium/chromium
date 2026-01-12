// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/apple/url_conversions.h"

#import <Foundation/Foundation.h>
#include <stddef.h>
#include <stdint.h>

#include "base/check_op.h"
#include "base/strings/sys_string_conversions.h"
#include "url/gurl.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  @autoreleasepool {
    NSString* string = [[NSString alloc] initWithBytes:data
                                                length:size
                                              encoding:NSUTF8StringEncoding];
    if (!string) {
      return 0;
    }

    NSURL* url = [NSURL URLWithString:string];
    if (url) {
      GURL gurl_from_nsurl = net::GURLWithNSURL(url);
      GURL gurl_from_string = GURL(base::SysNSStringToUTF8(string));
      // A CHECK comparing gurl_from_nsurl gurl_from_string would not be
      // useful because there are simply too many differences.
      (void)(gurl_from_nsurl == gurl_from_string);
    }
  }
  return 0;
}
