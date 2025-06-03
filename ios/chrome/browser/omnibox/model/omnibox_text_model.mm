// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/model/omnibox_text_model.h"

@implementation OmniboxTextModel

- (instancetype)init {
  self = [super init];
  if (self) {
    _focusState = OMNIBOX_FOCUS_NONE;
    _pasteState = OmniboxPasteState::kNone;
  }

  return self;
}

@end
