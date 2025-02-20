// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/ui_bundled/auto_deletion/auto_deletion_iph_view_controller.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"

@implementation AutoDeletionIPHViewController

- (instancetype)init {
  return [super init];
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  self.view.translatesAutoresizingMaskIntoConstraints = NO;
}

@end
