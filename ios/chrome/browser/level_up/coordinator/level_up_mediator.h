// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LEVEL_UP_COORDINATOR_LEVEL_UP_MEDIATOR_H_
#define IOS_CHROME_BROWSER_LEVEL_UP_COORDINATOR_LEVEL_UP_MEDIATOR_H_

#import <Foundation/Foundation.h>

@protocol LevelUpConsumer;

// Mediator for the Level Up feature.
@interface LevelUpMediator : NSObject

// The consumer for this mediator.
@property(nonatomic, weak) id<LevelUpConsumer> consumer;

@end

#endif  // IOS_CHROME_BROWSER_LEVEL_UP_COORDINATOR_LEVEL_UP_MEDIATOR_H_
