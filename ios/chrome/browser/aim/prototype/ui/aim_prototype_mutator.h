// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AIM_PROTOTYPE_UI_AIM_PROTOTYPE_MUTATOR_H_
#define IOS_CHROME_BROWSER_AIM_PROTOTYPE_UI_AIM_PROTOTYPE_MUTATOR_H_

#import <Foundation/Foundation.h>

// Mutator for the AIM prototype.
@protocol AIMPrototypeMutator
- (void)sendText:(NSString*)text;
@end

#endif  // IOS_CHROME_BROWSER_AIM_PROTOTYPE_UI_AIM_PROTOTYPE_MUTATOR_H_
