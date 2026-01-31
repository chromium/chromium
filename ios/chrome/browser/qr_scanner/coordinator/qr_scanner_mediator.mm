// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/qr_scanner/coordinator/qr_scanner_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "components/omnibox/browser/omnibox_text_util.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"

@implementation QRScannerMediator {
  raw_ptr<UrlLoadingBrowserAgent> _loader;
}

- (instancetype)initWithLoader:(UrlLoadingBrowserAgent*)loader {
  self = [super init];
  if (self) {
    CHECK(loader);
    _loader = loader;
  }
  return self;
}

#pragma mark - ScannerMutator

- (void)loadScannerQuery:(NSString*)string {
  _loader->LoadURLForQuery(string);
}

@end
