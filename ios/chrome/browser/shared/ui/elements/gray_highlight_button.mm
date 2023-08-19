// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/elements/gray_highlight_button.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"

@implementation GrayHighlightButton

- (void)setHighlighted:(BOOL)highlighted {
  [super setHighlighted:highlighted];
  if (highlighted) {
    self.backgroundColor = [UIColor colorNamed:kTableViewRowHighlightColor];
  } else {
    self.backgroundColor = [UIColor clearColor];
  }
}

@end
