// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SCENE_UI_SCENE_MUTATOR_H_
#define IOS_CHROME_BROWSER_SCENE_UI_SCENE_MUTATOR_H_

#import <Foundation/Foundation.h>

// Protocol to handle user actions in the Scene UI.
@protocol SceneMutator <NSObject>

// Notifies that the New IA Promo IPH was dismissed.
- (void)newIAPromoIPHDismissed;

@end

#endif  // IOS_CHROME_BROWSER_SCENE_UI_SCENE_MUTATOR_H_
