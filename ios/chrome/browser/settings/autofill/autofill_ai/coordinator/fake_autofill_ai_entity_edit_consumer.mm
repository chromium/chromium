// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_ai/coordinator/fake_autofill_ai_entity_edit_consumer.h"

#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_edit_date_item.h"

@implementation FakeAutofillAIEntityEditConsumer

@synthesize mode = _mode;

- (void)updateItem:(TableViewItem*)item {
  // Empty implementation to satisfy protocol.
}

- (void)setLoadingState:(BOOL)loadingState {
  if (loadingState) {
    self.showLoadingStateCalled = YES;
  } else {
    self.hideLoadingStateCalled = YES;
  }
}

- (void)didFinishSavingWithLocalFallback:(BOOL)isLocalFallback {
  if (isLocalFallback) {
    self.didFinishSavingToLocalAsFallbackCalled = YES;
  } else {
    self.didFinishSavingCalled = YES;
  }
}

@end
