// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_CONTAINER_MUTATOR_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_CONTAINER_MUTATOR_H_

#import <UIKit/UIKit.h>

// Mutator protocol for communicating actions/events from the Gemini container
// view controller to its mediator.
@protocol GeminiContainerMutator <NSObject>

// Called when the keyboard is shown.
- (void)containerKeyboardDidShowWithDuration:(NSTimeInterval)duration
                                       curve:(UIViewAnimationCurve)curve;

// Called when the dynamic actuation height changes.
- (void)containerDidChangeActuationHeight:(CGFloat)height;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_CONTAINER_MUTATOR_H_
