// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_UI_TTC_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_UI_TTC_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ai_prototyping/ttc/ui/ttc_consumer.h"
#import "ios/chrome/browser/ai_prototyping/ttc/ui/ttc_voice_plate_consumer.h"
#import "ios/chrome/browser/ai_prototyping/ui/ai_prototyping_view_controller_protocol.h"

@protocol TTCMutator;
@protocol TTCVoicePlateMutator;

// View controller displaying the TTC prototype UI.
@interface TTCViewController
    : UIViewController <AIPrototypingViewControllerProtocol,
                        TTCConsumer,
                        TTCVoicePlateConsumer>

// Mutator handling common AI prototyping interactions.
@property(nonatomic, weak) id<AIPrototypingMutator> mutator;

// Mutator handling TTC session journal actions.
@property(nonatomic, weak) id<TTCMutator> ttcMutator;

// Mutator handling voice session lifecycle.
@property(nonatomic, weak) id<TTCVoicePlateMutator> voicePlateMutator;

// Use `initForFeature:` from `AIPrototypingViewControllerProtocol` instead.
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_UI_TTC_VIEW_CONTROLLER_H_
