// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/tab_strip/ui/tab_strip_utils.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"

@implementation TabStripHelper

+ (UIColor*)backgroundColor {
  return [self cellBackgroundColor];
}

+ (UIColor*)cellBackgroundColor {
  return [UIColor colorNamed:kTabStripV3BackgroundColor];
}

+ (UIColor*)newTabButtonSymbolColor {
  return [UIColor colorNamed:kTabStripNewTabButtonColor];
}

@end
