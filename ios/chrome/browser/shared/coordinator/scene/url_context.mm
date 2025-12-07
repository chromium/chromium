// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/coordinator/scene/url_context.h"

#import "google_apis/gaia/gaia_id.h"

@implementation URLContext

- (instancetype)initWithContext:(UIOpenURLContext*)context
                         gaiaID:(const GaiaId&)gaiaID
                           type:(AccountSwitchType)type {
  self = [super init];
  if (self) {
    _context = context;
    _gaiaID = gaiaID;
    _type = type;
  }
  return self;
}

@end
