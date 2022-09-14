// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/whats_new/whats_new_table_view_controller.h"
#import "ios/chrome/browser/ui/table_view/table_view_utils.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation WhatsNewTableViewController

- (instancetype)init {
  UITableViewStyle style = ChromeTableViewStyle();
  self = [super initWithStyle:style];
  return self;
}

@end