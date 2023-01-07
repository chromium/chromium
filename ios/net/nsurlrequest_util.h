// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_NET_NSURLREQUEST_UTIL_H_
#define IOS_NET_NSURLREQUEST_UTIL_H_

#import <Foundation/Foundation.h>
#include <string>

namespace net {

// Takes an |NSURLRequest| and returns a string in the form:
// request:<url> request.mainDocURL:<mainDocumentURL>.
std::string FormatUrlRequestForLogging(NSURLRequest* request);

}  // namespace net

#endif  // IOS_NET_NSURLREQUEST_UTIL_H_
