// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/assistant/ui/assistant_sheet_view_controller.h"

#import "ios/chrome/browser/assistant/ui/assistant_sheet_view.h"

@implementation AssistantSheetViewController

- (void)loadView {
  self.view = [[AssistantSheetView alloc] init];
}

- (void)viewDidLoad {
  [super viewDidLoad];
  // TODO(crbug.com/469050167): Implement.
}

@end
