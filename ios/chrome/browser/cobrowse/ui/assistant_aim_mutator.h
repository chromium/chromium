// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COBROWSE_UI_ASSISTANT_AIM_MUTATOR_H_
#define IOS_CHROME_BROWSER_COBROWSE_UI_ASSISTANT_AIM_MUTATOR_H_

#import <Foundation/Foundation.h>

// Mutator interface for the AIM feature.
@protocol AssistantAIMMutator <NSObject>

// Notifies the mutator that a search was requested with the given `text`.
- (void)assistantAIMViewControllerDidRequestSearchWithText:(NSString*)text;

@end

#endif  // IOS_CHROME_BROWSER_COBROWSE_UI_ASSISTANT_AIM_MUTATOR_H_
