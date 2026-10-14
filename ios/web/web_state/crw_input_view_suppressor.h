// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_CRW_INPUT_VIEW_SUPPRESSOR_H_
#define IOS_WEB_WEB_STATE_CRW_INPUT_VIEW_SUPPRESSOR_H_

#import <UIKit/UIKit.h>

#import "ios/web/common/crw_input_view_provider.h"

// Helper that suppresses input views (e.g. the software keyboard) for an owning
// `UIView` until disconnected.
@interface CRWInputViewSuppressor : NSObject <CRWResponderInputView>

// Initializes the suppressor with the owning `view` (held weakly).
- (instancetype)initWithView:(UIView*)view NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Reloads input views on the active first responder in the owning view's
// hierarchy.
- (void)reloadInputViews;

// Restores any suppressed `inputAssistantItem` groups, reloads input views on
// the active first responder, and disconnects from the owning view.
- (void)disconnect;

@end

#endif  // IOS_WEB_WEB_STATE_CRW_INPUT_VIEW_SUPPRESSOR_H_
