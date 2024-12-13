// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/youtube_incognito/ui/youtube_incognito_sheet.h"

@implementation YoutubeIncognitoSheet

- (instancetype)init {
  self = [super init];
  return self;
}

- (void)viewDidLoad {
  self.showDismissBarButton = NO;
  [super viewDidLoad];
}

@end
