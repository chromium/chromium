// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/bwg_link_opening_handler.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/bwg_metrics.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "url/gurl.h"

@implementation BWGLinkOpeningHandler {
  // The URL loading agent for opening URLs.
  raw_ptr<UrlLoadingBrowserAgent, DanglingUntriaged> _URLLoadingAgent;
}

#pragma mark - BWGLinkOpeningDelegate

- (instancetype)initWithURLLoader:(UrlLoadingBrowserAgent*)URLLoadingAgent {
  self = [super self];
  if (self) {
    _URLLoadingAgent = URLLoadingAgent;
  }
  return self;
}

- (void)openURLInNewTab:(NSString*)URL {
  UrlLoadParams params =
      UrlLoadParams::InNewTab(GURL(base::SysNSStringToUTF8(URL)));
  params.append_to = OpenPosition::kCurrentTab;
  _URLLoadingAgent->Load(params);
  RecordURLOpened();
}

@end
