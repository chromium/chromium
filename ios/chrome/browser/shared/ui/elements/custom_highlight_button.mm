// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/elements/custom_highlight_button.h"

@implementation CustomHighlightableButton {
  CustomHighlightableButtonHighlightHandler _customHighlightHandler;
}

- (void)setCustomHighlightHandler:
    (CustomHighlightableButtonHighlightHandler)customHighlightHandler {
  _customHighlightHandler = customHighlightHandler;
}

- (void)setCustomHighlighted:(BOOL)customHighlighted {
  if (_customHighlightHandler) {
    _customHighlightHandler(customHighlighted);
  }
}
@end
