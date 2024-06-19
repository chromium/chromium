// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTHENTICATION_CARD_UNMASK_AUTHENTICATION_SELECTION_MUTATOR_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTHENTICATION_CARD_UNMASK_AUTHENTICATION_SELECTION_MUTATOR_H_

@class CardUnmaskChallengeOptionIOS;

@protocol CardUnmaskAuthenticationSelectionMutator <NSObject>

// A challenge option was selected.
- (void)didSelectChallengeOption:(CardUnmaskChallengeOptionIOS*)challengeOption;

// The selection was accepted.
- (void)didAcceptSelection;

// The selection was cancelled.
- (void)didCancelSelection;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTHENTICATION_CARD_UNMASK_AUTHENTICATION_SELECTION_MUTATOR_H_
