// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/download/download_manager_state_view.h"

#import "ios/chrome/browser/ui/icons/chrome_symbol.h"
#import "ios/chrome/browser/ui/icons/download_icon.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Fixed size of the view.
const CGFloat kViewSize = 28;

}  // namespace

@implementation DownloadManagerStateView

#pragma mark - UIView

- (CGSize)intrinsicContentSize {
  return CGSizeMake(kViewSize, kViewSize);
}

#pragma mark - Public

- (void)setState:(DownloadManagerState)state {
  switch (state) {
    case kDownloadManagerStateNotStarted:
      self.image = DefaultSymbolTemplateWithPointSize(
          kDownloadPromptFillSymbol, kSymbolDownloadInfobarPointSize);
      self.tintColor = [UIColor colorNamed:kBlueColor];
      break;
    case kDownloadManagerStateInProgress:
      self.image = DefaultSymbolTemplateWithPointSize(
          kDownloadDocFillSymbol, kSymbolDownloadSmallInfobarPointSize);
      self.tintColor = [UIColor colorNamed:kGrey400Color];
      break;
    case kDownloadManagerStateSucceeded:
    case kDownloadManagerStateFailed:
    case kDownloadManagerStateFailedNotResumable:
      self.image = DefaultSymbolTemplateWithPointSize(
          kDownloadDocFillSymbol, kSymbolDownloadInfobarPointSize);
      self.tintColor = [UIColor colorNamed:kGrey400Color];
      break;
  }
}

@end
