// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/coordinator/fake_at_memory_fill_handler.h"

@implementation FakeAtMemoryFillHandler

- (void)fillWithSuggestion:(const autofill::Suggestion&)suggestion {
  self.fillWithSuggestionCalled = YES;
}

@end
