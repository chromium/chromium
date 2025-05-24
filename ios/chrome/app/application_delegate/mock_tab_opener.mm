// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/application_delegate/mock_tab_opener.h"

#import "base/ios/block_types.h"
#import "ios/chrome/app/application_mode.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "net/base/apple/url_conversions.h"
#import "ui/base/page_transition_types.h"
#import "url/gurl.h"

@implementation MockTabOpener {
  std::vector<GURL> _URLs;
}

- (void)dismissModalsAndMaybeOpenSelectedTabInMode:
            (ApplicationModeForTabOpening)targetMode
                                 withUrlLoadParams:
                                     (const UrlLoadParams&)urlLoadParams
                                    dismissOmnibox:(BOOL)dismissOmnibox
                                        completion:(ProceduralBlock)completion {
  if (targetMode == ApplicationModeForTabOpening::UNDETERMINED) {
    // Falling back to `NORMAL`.
    targetMode = ApplicationModeForTabOpening::NORMAL;
  }

  _urlLoadParams = urlLoadParams;
  _applicationMode = targetMode;
  _completionBlock = [completion copy];
  _URLs.push_back(urlLoadParams.web_params.url);
}

- (void)dismissModalsAndOpenMultipleTabsWithURLs:(const std::vector<GURL>&)URLs
                                 inIncognitoMode:(BOOL)incognitoMode
                                  dismissOmnibox:(BOOL)dismissOmnibox
                                      completion:(ProceduralBlock)completion {
  _URLs = URLs;
}

- (void)resetURL {
  _urlLoadParams.web_params.url = _urlLoadParams.web_params.url.EmptyGURL();
}

- (void)openTabFromLaunchWithParams:(URLOpenerParams*)params
                 startupInformation:(id<StartupInformation>)startupInformation {
  // Stub.
}

- (BOOL)shouldOpenNTPTabOnActivationOfBrowser:(Browser*)browser {
  // Stub.
  return YES;
}

- (ProceduralBlock)completionBlockForTriggeringAction:
    (TabOpeningPostOpeningAction)action {
  // Stub
  return nil;
}

- (BOOL)URLIsOpenedInRegularMode:(const GURL&)URL {
  return NO;
}

@end
