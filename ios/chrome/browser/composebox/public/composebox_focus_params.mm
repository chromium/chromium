// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/public/composebox_focus_params.h"

@implementation ComposeboxFocusParams

- (instancetype)initWithEntrypoint:(ComposeboxEntrypoint)entrypoint {
  self = [super init];
  if (self) {
    _entrypoint = entrypoint;
  }
  return self;
}

@end
