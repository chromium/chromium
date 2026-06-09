// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COBROWSE_UI_ASSISTANT_AIM_MUTATOR_H_
#define IOS_CHROME_BROWSER_COBROWSE_UI_ASSISTANT_AIM_MUTATOR_H_

#import <Foundation/Foundation.h>

// Mutator for the Assistant AIM UI, handled by the mediator.
@protocol AssistantAIMMutator <NSObject>

// Called when the user taps the history button.
- (void)didTapHistory;

// Called when the user selects a specific history task.
- (void)didSelectHistoryTaskWithId:(NSString*)taskId;

// Called when the user taps the start new thread button.
- (void)didTapStartNewThread;

@end

#endif  // IOS_CHROME_BROWSER_COBROWSE_UI_ASSISTANT_AIM_MUTATOR_H_
