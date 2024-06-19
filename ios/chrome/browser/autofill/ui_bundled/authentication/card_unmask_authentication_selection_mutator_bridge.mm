// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/authentication/card_unmask_authentication_selection_mutator_bridge.h"
#import "ios/chrome/browser/autofill/ui_bundled/authentication/card_unmask_authentication_selection_mutator_bridge_target.h"

@implementation CardUnmaskAuthenticationSelectionMutatorBridge {
  base::WeakPtr<CardUnmaskAuthenticationSelectionMutatorBridgeTarget> _target;
}

- (instancetype)initWithTarget:
    (base::WeakPtr<CardUnmaskAuthenticationSelectionMutatorBridgeTarget>)
        target {
  self = [super init];
  if (self) {
    _target = target;
  }
  return self;
}

#pragma mark - CardUnmaskAuthenticationSelectionMutator

- (void)didSelectChallengeOption:
    (CardUnmaskChallengeOptionIOS*)challengeOption {
  if (_target) {
    _target->DidSelectChallengeOption(challengeOption);
  }
}

- (void)didAcceptSelection {
  if (_target) {
    _target->DidAcceptSelection();
  }
}

- (void)didCancelSelection {
  if (_target) {
    _target->DidCancelSelection();
  }
}

@end
