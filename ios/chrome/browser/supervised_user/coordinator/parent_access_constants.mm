// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/supervised_user/coordinator/parent_access_constants.h"

namespace {
// URL of the fetching endpoint.
NSString* const kFamilyLinkURL = @"https://families.google.com/parentaccess";
// Query parameter key for the caller ID.
NSString* const kCallerIdKey = @"callerid";
// Query parameter value for the caller ID, this corresponds
// to the expected value in the PACP server iOS configuration.
NSString* const kCallerIdName = @"qSTnVRdQ";
// Query parameter key for the Javascript message handler.
NSString* const kParentAccessScriptMessageHandlerKey = @"wkhandler";
}  // namespace

NSString* const kParentAccessScriptMessageHandlerName =
    @"ParentAccessScriptMessageHandler";

NSURL* ParentAccessURL() {
  NSURLComponents* family_link_url_components =
      [NSURLComponents componentsWithString:kFamilyLinkURL];
  NSURLQueryItem* caller_id_query_item =
      [NSURLQueryItem queryItemWithName:kCallerIdKey value:kCallerIdName];
  NSURLQueryItem* script_message_hanlder_query_item =
      [NSURLQueryItem queryItemWithName:kParentAccessScriptMessageHandlerKey
                                  value:kParentAccessScriptMessageHandlerName];
  family_link_url_components.queryItems =
      @[ caller_id_query_item, script_message_hanlder_query_item ];
  return family_link_url_components.URL;
}
