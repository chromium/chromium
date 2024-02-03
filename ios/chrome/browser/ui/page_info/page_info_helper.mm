// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/page_info/page_info_helper.h"

namespace page_info {

// The minimum scale factor of the title label showing the URL of page info.
const CGFloat kPageInfoTitleLabelMinimumScaleFactor = 0.7f;

UILabel* TitleViewLabelForURL(NSString* site_url) {
  UILabel* label_url = [[UILabel alloc] init];
  label_url.lineBreakMode = NSLineBreakByTruncatingHead;
  label_url.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  label_url.text = site_url;
  label_url.adjustsFontSizeToFitWidth = YES;
  label_url.minimumScaleFactor = kPageInfoTitleLabelMinimumScaleFactor;
  return label_url;
}

}  // namespace page_info
