// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/tab_groups/ui/tab_group_indicator_view.h"

#import "ios/chrome/browser/ui/toolbar/tab_groups/ui/tab_group_indicator_constants.h"

@implementation TabGroupIndicatorView {
  // Stores the tab group informations.
  NSString* _groupTitle;
  UIColor* _groupColor;

  // Tracks if the view is available.
  BOOL _available;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    self.accessibilityIdentifier = kTabGroupIndicatorViewIdentifier;
    self.backgroundColor = UIColor.redColor;
  }
  return self;
}

#pragma mark - TabGroupIndicatorConsumer

- (void)setTabGroupTitle:(NSString*)groupTitle groupColor:(UIColor*)groupColor {
  if (groupTitle == _groupTitle && groupColor == _groupColor) {
    [self updateVisibility];
    return;
  }

  [self setGroupTitle:groupTitle];
  [self setGroupColor:groupColor];
  [self updateVisibility];
}

#pragma mark - Private

// Updates the view's visibility.
- (void)updateVisibility {
  self.hidden = _groupTitle == nil || !_available;
}

#pragma mark - Setters

- (void)setAvailable:(BOOL)available {
  _available = available;
  [self updateVisibility];
}

- (void)setGroupTitle:(NSString*)title {
  _groupTitle = title;
  // TODO(crbug.com/5832033): Implement this.
}

- (void)setGroupColor:(UIColor*)color {
  _groupColor = color;
  // TODO(crbug.com/5832033): Implement this.
}

@end
