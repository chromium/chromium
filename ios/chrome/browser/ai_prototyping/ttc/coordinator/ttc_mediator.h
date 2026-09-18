// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_COORDINATOR_TTC_MEDIATOR_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_COORDINATOR_TTC_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ai_prototyping/ttc/ui/ttc_mutator.h"

@class TTCAudioEngine;
@protocol TTCConsumer;

// Mediator driving the TalkToChrome microphone input debug UI.
@interface TTCMediator : NSObject <TTCMutator>

// Consumer receiving updates from the mediator.
@property(nonatomic, weak) id<TTCConsumer> consumer;

// Initializer injecting the audio engine.
- (instancetype)initWithAudioEngine:(TTCAudioEngine*)audioEngine
    NS_DESIGNATED_INITIALIZER;

// Default initializer creating an internal TTCAudioEngine.
- (instancetype)init;

// Disconnects active state observations and clears references.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_COORDINATOR_TTC_MEDIATOR_H_
