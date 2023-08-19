// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/whats_new/data_source/whats_new_item.h"

#import "url/gurl.h"

@implementation WhatsNewItem {
  GURL _learnMoreURL;
}

#pragma mark - Properties

- (const GURL&)learnMoreURL {
  return _learnMoreURL;
}

- (void)setLearnMoreURL:(const GURL&)url {
  _learnMoreURL = url;
}

@end
