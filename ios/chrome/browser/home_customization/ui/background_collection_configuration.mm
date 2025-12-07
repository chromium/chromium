// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/ui/background_collection_configuration.h"

@implementation BackgroundCollectionConfiguration

- (instancetype)init {
  self = [super init];
  if (self) {
    _configurationOrder = [NSMutableArray array];
    _configurations = [NSMutableDictionary dictionary];
  }
  return self;
}

@end
