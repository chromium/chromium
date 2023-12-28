// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_MODEL_WEB_STATE_CONTAINER_VIEW_PROVIDER_H_
#define IOS_CHROME_BROWSER_WEB_MODEL_WEB_STATE_CONTAINER_VIEW_PROVIDER_H_

#import <UIKit/UIKit.h>

// An object conforming to this protocol can provide information about the
// view that a WebState will be displayed in.
@protocol WebStateContainerViewProvider <NSObject>

// The container view for the web state.
- (UIView*)containerView;

// The location in the web state that should be used to display dialogs.
- (CGPoint)dialogLocation;

@end

#endif  // IOS_CHROME_BROWSER_WEB_MODEL_WEB_STATE_CONTAINER_VIEW_PROVIDER_H_
