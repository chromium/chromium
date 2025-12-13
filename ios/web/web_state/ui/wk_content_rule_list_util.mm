// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/wk_content_rule_list_util.h"

#import "base/check.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/web/public/web_client.h"

namespace web {

NSString* CreateLocalBlockingJsonRuleList() {
  NSMutableArray* local_schemes_urls = [@[ @"file://.*" ] mutableCopy];
  WebClient::Schemes schemes;
  GetWebClient()->AddAdditionalSchemes(&schemes);
  GetWebClient()->GetAdditionalWebUISchemes(&(schemes.standard_schemes));
  for (std::string scheme : schemes.standard_schemes) {
    [local_schemes_urls addObject:base::SysUTF8ToNSString(scheme + "://.*")];
  }

  NSDictionary* local_block = @{
    @"trigger" : @{
      @"url-filter" : @"https?://.*",
      @"if-top-url" : local_schemes_urls,
      @"resource-type" : @[
        // These should be all resource types except document.
        // "document" cannot be blocked because it breaks error pages displayed
        // when a WebStatePolicyDecider blocks a navigation.
        @"image", @"style-sheet", @"script", @"font", @"raw", @"svg-document",
        @"media", @"popup", @"ping"
      ],
    },
    @"action" : @{
      @"type" : @"block",
    },
  };

  NSDictionary* allow_crbug = @{
    @"trigger" : @{
      @"url-filter" : @"https://bugs\\.chromium\\.org/.*",
      @"if-top-url" : @[ @"chrome://.*" ],
      @"resource-type" : @[
        // Allow opening crbug from chrome:// urls
        @"popup"
      ],
    },
    @"action" : @{
      @"type" : @"ignore-previous-rules",
    },
  };

  NSData* json_data =
      [NSJSONSerialization dataWithJSONObject:@[ local_block, allow_crbug ]
                                      options:NSJSONWritingPrettyPrinted
                                        error:nil];
  NSString* json_string = [[NSString alloc] initWithData:json_data
                                                encoding:NSUTF8StringEncoding];
  return json_string;
}

NSString* CreateMixedContentAutoUpgradeJsonRuleList() {
  NSDictionary* mixed_content_autoupgrade = @{
    @"trigger" : @{
      @"url-filter" : @"http://.*",
      @"if-top-url" : @[ @"https://.*" ],
      @"resource-type" : @[
        // Only upgrade image and media (i.e. audio and video) per
        // https://www.w3.org/TR/mixed-content/#upgrade-algorithm
        @"image", @"media"
      ],
    },
    @"action" : @{
      @"type" : @"make-https",
    },
  };

  NSData* json_data =
      [NSJSONSerialization dataWithJSONObject:@[ mixed_content_autoupgrade ]
                                      options:NSJSONWritingPrettyPrinted
                                        error:nil];
  NSString* json_string = [[NSString alloc] initWithData:json_data
                                                encoding:NSUTF8StringEncoding];
  return json_string;
}

}  // namespace web
