// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_UI_CRW_TOUCH_TRACKING_RECOGNIZER_H_
#define IOS_WEB_WEB_STATE_UI_CRW_TOUCH_TRACKING_RECOGNIZER_H_

#import <UIKit/UIGestureRecognizerSubclass.h>

// Methods implemented by the delegate of the CRWTouchTrackingRecognizer.
@protocol CRWTouchTrackingDelegate

// Called with YES when touches began, with NO when touches are ended or
// cancelled.
- (void)touched:(BOOL)touched;

@end

// UIGestureRecognizer subclass that informs delegate about touches using
// simplified interface.
@interface CRWTouchTrackingRecognizer : UIGestureRecognizer

// CRWTouchTrackingRecognizer delegate.
@property(nonatomic, weak) id<CRWTouchTrackingDelegate> touchTrackingDelegate;

// Designated initializer for CRWTouchTrackingRecognizer.
- (id)initWithTouchTrackingDelegate:
    (id<CRWTouchTrackingDelegate>)touchTrackingDelegate;

@end

#endif  // IOS_WEB_WEB_STATE_UI_CRW_TOUCH_TRACKING_RECOGNIZER_H_
