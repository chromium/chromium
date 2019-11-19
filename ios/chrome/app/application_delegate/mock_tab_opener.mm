// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/app/application_delegate/mock_tab_opener.h"

#include "base/ios/block_types.h"
#include "base/mac/scoped_block.h"
#include "ios/chrome/app/application_mode.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation MockTabOpener

- (void)dismissModalsAndOpenSelectedTabInMode:
            (ApplicationModeForTabOpening)targetMode
                            withUrlLoadParams:
                                (const UrlLoadParams&)urlLoadParams
                               dismissOmnibox:(BOOL)dismissOmnibox
                                   completion:(ProceduralBlock)completion {
  _urlLoadParams = urlLoadParams;
  _applicationMode = targetMode;
  _completionBlock = [completion copy];
}

- (void)resetURL {
  _urlLoadParams.web_params.url = _urlLoadParams.web_params.url.EmptyGURL();
}

- (void)openTabFromLaunchOptions:(NSDictionary*)launchOptions
              startupInformation:(id<StartupInformation>)startupInformation
                        appState:(AppState*)appState {
  // Stub.
}

- (BOOL)shouldOpenNTPTabOnActivationOfTabModel:(TabModel*)tabModel {
  // Stub.
  return YES;
}

- (ProceduralBlock)completionBlockForTriggeringAction:
    (NTPTabOpeningPostOpeningAction)action {
  // Stub
  return nil;
}

- (BOOL)shouldCompletePaymentRequestOnCurrentTab:
    (id<StartupInformation>)startupInformation {
  // Stub.
  return NO;
}

- (BOOL)URLIsOpenedInRegularMode:(const GURL&)URL {
  return NO;
}

@end
