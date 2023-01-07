// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_NET_CRN_HTTP_URL_RESPONSE_H_
#define IOS_NET_CRN_HTTP_URL_RESPONSE_H_

#import <Foundation/Foundation.h>

// Custom URL response that allows querying the HTTPVersion to enable response
// modifications.
@interface CRNHTTPURLResponse : NSHTTPURLResponse
// Returns the HTTP version string that this object was initialized with.
- (NSString*)cr_HTTPVersion;
@end

#endif  // IOS_NET_CRN_HTTP_URL_RESPONSE_H_
