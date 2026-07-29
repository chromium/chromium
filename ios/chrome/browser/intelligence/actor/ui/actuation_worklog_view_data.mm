// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_data.h"

#import "base/check.h"

@implementation ActuationWorklogItem

- (instancetype)initWithTitle:(NSString*)title
                     subtitle:(NSString*)subtitle
                         icon:(UIImage*)icon
                        style:(ActuationWorklogItemStyle)style
                       active:(BOOL)active {
  CHECK(title);

  self = [super init];
  if (self) {
    _title = [title copy];
    _subtitle = [subtitle copy];
    _icon = icon;
    _style = style;
    _active = active;
  }
  return self;
}

@end

@implementation ActuationWorklogChip

- (instancetype)initWithText:(NSString*)text icon:(UIImage*)icon {
  CHECK(text);

  self = [super init];
  if (self) {
    _text = [text copy];
    _icon = icon;
  }
  return self;
}

@end
