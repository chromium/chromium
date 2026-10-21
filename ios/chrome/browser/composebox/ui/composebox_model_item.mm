// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/ui/composebox_model_item.h"

@implementation ComposeboxModelItem

- (instancetype)initWithModelMode:(omnibox::ModelMode)modelMode
                            title:(NSString*)title
                             icon:(UIImage*)icon
                         selected:(BOOL)selected
                         disabled:(BOOL)disabled {
  self = [super init];
  if (self) {
    _modelMode = modelMode;
    _title = [title copy];
    _icon = icon;
    _selected = selected;
    _disabled = disabled;
  }
  return self;
}

@end
