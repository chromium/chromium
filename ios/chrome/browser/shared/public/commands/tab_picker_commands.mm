// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/public/commands/tab_picker_commands.h"

#import "base/check.h"

namespace {

// Default maximum number of tabs that can be attached.
const NSUInteger kDefaultMaxTabAttachmentCount = 10;

}  // namespace

@implementation TabPickerParams

- (instancetype)initWithSnackbarPresenter:
    (id<TabPickerSnackbarPresenter>)snackbarPresenter {
  self = [super init];
  if (self) {
    CHECK(snackbarPresenter);
    _snackbarPresenter = snackbarPresenter;
    _maxTabAttachmentCount = kDefaultMaxTabAttachmentCount;
    // TODO(crbug.com/485311221): Support PDFs by default once ready.
    _PDFEnabled = NO;
  }
  return self;
}

- (void)setMaxTabAttachmentCount:(NSUInteger)maxTabAttachmentCount {
  if (maxTabAttachmentCount == 0) {
    _maxTabAttachmentCount = kDefaultMaxTabAttachmentCount;
  } else {
    _maxTabAttachmentCount = maxTabAttachmentCount;
  }
}

@end
