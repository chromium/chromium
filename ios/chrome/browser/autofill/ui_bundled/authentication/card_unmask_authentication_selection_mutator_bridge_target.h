// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTHENTICATION_CARD_UNMASK_AUTHENTICATION_SELECTION_MUTATOR_BRIDGE_TARGET_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTHENTICATION_CARD_UNMASK_AUTHENTICATION_SELECTION_MUTATOR_BRIDGE_TARGET_H_

#import "ios/chrome/browser/autofill/model/authentication/card_unmask_challenge_option_ios.h"

// The C++ equivalent interface of the Objective-C protocol,
// CardUnmaskAuthenticationSelectionMutator.
class CardUnmaskAuthenticationSelectionMutatorBridgeTarget {
 public:
  virtual void DidSelectChallengeOption(
      CardUnmaskChallengeOptionIOS* challengeOption) = 0;
  virtual void DidAcceptSelection() = 0;
  virtual void DidCancelSelection() = 0;
};

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTHENTICATION_CARD_UNMASK_AUTHENTICATION_SELECTION_MUTATOR_BRIDGE_TARGET_H_
