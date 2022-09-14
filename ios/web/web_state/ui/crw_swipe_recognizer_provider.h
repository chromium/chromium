// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_UI_CRW_SWIPE_RECOGNIZER_PROVIDER_H_
#define IOS_WEB_WEB_STATE_UI_CRW_SWIPE_RECOGNIZER_PROVIDER_H_

#import <UIKit/UIKit.h>

// A protocol implemented by a provider of swipe recognizers for a web view.
@protocol CRWSwipeRecognizerProvider

// Returns set of UIGestureRecognizer objects.
- (NSSet*)swipeRecognizers;

@end

#endif  // IOS_WEB_WEB_STATE_UI_CRW_SWIPE_RECOGNIZER_PROVIDER_H_
