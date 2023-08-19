// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/credential_provider/archivable_credential_util.h"

NSString* RecordIdentifierForData(NSURL* url, NSString* username) {
  NSURLComponents* urlComponents = [NSURLComponents componentsWithURL:url
                                              resolvingAgainstBaseURL:NO];

  // Remove specific parts of URL that are thrown away in the credential
  // manager.
  urlComponents.user = nil;
  urlComponents.password = nil;
  urlComponents.query = nil;
  urlComponents.fragment = nil;

  NSString* strippedURL = urlComponents.string;

  // Replace path with / as well to end up with origin.
  urlComponents.path = @"/";

  NSString* origin = urlComponents.string;

  return
      [NSString stringWithFormat:@"%@||%@||%@", strippedURL, username, origin];
}
