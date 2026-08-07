// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_granular_fill_view_controller.h"

@implementation AtMemoryGranularFillViewController

#pragma mark - AtMemoryGranularFillConsumer

- (void)setTitle:(NSString*)title {
  [super setTitle:title];
}

- (void)setGranularFillItems:(NSArray<AtMemoryGranularFillItem*>*)items {
  // TODO(crbug.com/522340351): Set the granular fill items in the table view.
}

@end
