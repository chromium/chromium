// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/wk_content_rule_list_util.h"

#include "base/check.h"
#include "base/strings/sys_string_conversions.h"
#include "ios/web/public/browsing_data/cookie_blocking_mode.h"
#include "ios/web/public/web_client.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

NSString* CreateLocalBlockingJsonRuleList() {
  NSMutableDictionary* local_block = [@{
    @"trigger" : [@{
      @"url-filter" : @"https?://.*",
      @"if-top-url" : [@[ @"file://.*" ] mutableCopy],
      @"resource-type" : @[
        // These should be all resource types except document.
        // "document" cannot be blocked because it breaks error pages displayed
        // when a WebStatePolicyDecider blocks a navigation.
        @"image", @"style-sheet", @"script", @"font", @"raw", @"svg-document",
        @"media", @"popup", @"ping"
      ],
    } mutableCopy],
    @"action" : @{
      @"type" : @"block",
    },
  } mutableCopy];

  WebClient::Schemes schemes;
  GetWebClient()->AddAdditionalSchemes(&schemes);
  GetWebClient()->GetAdditionalWebUISchemes(&(schemes.standard_schemes));
  for (std::string scheme : schemes.standard_schemes) {
    [local_block[@"trigger"][@"if-top-url"]
        addObject:base::SysUTF8ToNSString(scheme + "://.*")];
  }

  NSData* json_data =
      [NSJSONSerialization dataWithJSONObject:@[ local_block ]
                                      options:NSJSONWritingPrettyPrinted
                                        error:nil];
  NSString* json_string = [[NSString alloc] initWithData:json_data
                                                encoding:NSUTF8StringEncoding];
  return json_string;
}

NSString* CreateCookieBlockingJsonRuleList(CookieBlockingMode block_mode) {
  DCHECK(block_mode != CookieBlockingMode::kAllow);
  NSMutableDictionary* overall_block = [@{
    @"trigger" : [@{
      @"url-filter" : @".*",
    } mutableCopy],
    @"action" : @{
      @"type" : @"block-cookies",
    },
  } mutableCopy];

  if (block_mode == CookieBlockingMode::kBlockThirdParty) {
    overall_block[@"trigger"][@"load-type"] = @[ @"third-party" ];
  }

  NSData* json_data =
      [NSJSONSerialization dataWithJSONObject:@[ overall_block ]
                                      options:NSJSONWritingPrettyPrinted
                                        error:nil];
  NSString* json_string = [[NSString alloc] initWithData:json_data
                                                encoding:NSUTF8StringEncoding];
  return json_string;
}

}  // namespace web
