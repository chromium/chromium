// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/content_settings/default_page_mode_table_view_controller.h"

#import "ios/chrome/browser/ui/table_view/cells/table_view_detail_icon_item.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation DefaultPageModeTableViewController

#pragma mark - DefaultPageModeConsumer

- (void)setDefaultPageMode:(DefaultPageMode)mode {
  // TODO(crbug.com/1276922): change the selected cell based on the mode.
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
  // TODO(crbug.com/1276922): Add UserAction recording.
}

- (void)reportBackUserAction {
  // TODO(crbug.com/1276922): Add UserAction recording.
}

@end
