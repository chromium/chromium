// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/net/nsurlrequest_util.h"

#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"

namespace net {

std::string FormatUrlRequestForLogging(NSURLRequest* request) {
  NSString* urlAbsoluteString = request.URL.absoluteString;
  NSString* mainDocumentURLAbsoluteString =
      request.mainDocumentURL.absoluteString;
  return base::StringPrintf(
      "request: %s request.mainDocURL: %s",
      urlAbsoluteString ? base::SysNSStringToUTF8(urlAbsoluteString).c_str()
                        : "[nil]",
      mainDocumentURLAbsoluteString
          ? base::SysNSStringToUTF8(mainDocumentURLAbsoluteString).c_str()
          : "[nil]");
}

}  // namespace net
