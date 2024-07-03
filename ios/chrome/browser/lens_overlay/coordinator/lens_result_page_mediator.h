// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_COORDINATOR_LENS_RESULT_PAGE_MEDIATOR_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_COORDINATOR_LENS_RESULT_PAGE_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_result_consumer.h"
#import "ios/web/public/web_state.h"

@protocol LensResultPageConsumer;

/// Mediator that handles lens result page operations.
@interface LensResultPageMediator : NSObject <LensOverlayResultConsumer>

@property(nonatomic, weak) id<LensResultPageConsumer> consumer;

- (instancetype)
     initWithWebStateParams:(const web::WebState::CreateParams&)params
    browserWebStateDelegate:(web::WebStateDelegate*)browserWebStateDelegate
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

/// Releases managed objects.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_COORDINATOR_LENS_RESULT_PAGE_MEDIATOR_H_
