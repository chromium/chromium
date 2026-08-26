// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_COORDINATOR_AUTOFILL_AI_PRIVATE_INFERENCE_NOTICE_MUTATOR_H_
#define IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_COORDINATOR_AUTOFILL_AI_PRIVATE_INFERENCE_NOTICE_MUTATOR_H_

#import <Foundation/Foundation.h>

// Mutator protocol for communicating actions to the
// AutofillAIPrivateInferenceNoticeMediator.
@protocol AutofillAIPrivateInferenceNoticeMutator <NSObject>

// Marks the notice as shown in profile preferences.
- (void)markNoticeShown;

// Handles the user acknowledging the notice.
- (void)didAcknowledgeNotice;

// Handles the user tapping the settings action.
- (void)didTapSettings;

// Handles the notice being dismissed.
- (void)didDismissNotice;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_COORDINATOR_AUTOFILL_AI_PRIVATE_INFERENCE_NOTICE_MUTATOR_H_
