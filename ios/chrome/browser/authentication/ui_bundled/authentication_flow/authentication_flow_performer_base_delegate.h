// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_AUTHENTICATION_FLOW_AUTHENTICATION_FLOW_PERFORMER_BASE_DELEGATE_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_AUTHENTICATION_FLOW_AUTHENTICATION_FLOW_PERFORMER_BASE_DELEGATE_H_

#import <Foundation/Foundation.h>

#import "base/functional/callback_forward.h"

class Browser;

// Handles completion of AuthenticationFlowPerformerBase steps.
@protocol AuthenticationFlowPerformerBaseDelegate <NSObject>

// Indicates that switching to a different profile was completed.
// `newProfileBrowser` must be regular. The continuation must be executed with
// `completion`.
- (void)didSwitchToProfileWithNewProfileBrowser:(Browser*)newProfileBrowser
                                     completion:(base::OnceClosure)completion;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_AUTHENTICATION_FLOW_AUTHENTICATION_FLOW_PERFORMER_BASE_DELEGATE_H_
