// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_UI_TTC_VOICE_PLATE_VIEW_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_UI_TTC_VOICE_PLATE_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ai_prototyping/ttc/ui/ttc_voice_plate_consumer.h"

@protocol TTCVoicePlateMutator;

// Reusable compact voice plate capsule view conforming to
// `TTCVoicePlateConsumer`.
@interface TTCVoicePlateView : UIView <TTCVoicePlateConsumer>

// Mutator for dispatching user actions to the coordinator or mediator.
@property(nonatomic, weak) id<TTCVoicePlateMutator> mutator;

// Initializes the voice plate view with designated frame.
- (instancetype)initWithFrame:(CGRect)frame NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_UI_TTC_VOICE_PLATE_VIEW_H_
