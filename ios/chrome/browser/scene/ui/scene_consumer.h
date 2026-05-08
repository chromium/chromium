// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SCENE_UI_SCENE_CONSUMER_H_
#define IOS_CHROME_BROWSER_SCENE_UI_SCENE_CONSUMER_H_

#import <Foundation/Foundation.h>

// Protocol for the scene consumer.
@protocol SceneConsumer <NSObject>

// Tells the consumer to show the new IA promo.
- (void)showNewIAPromoWithGeminiEligibility:(BOOL)eligible;

@end

#endif  // IOS_CHROME_BROWSER_SCENE_UI_SCENE_CONSUMER_H_
