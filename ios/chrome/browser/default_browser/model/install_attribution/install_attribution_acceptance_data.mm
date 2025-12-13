// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_browser/model/install_attribution/install_attribution_acceptance_data.h"

@implementation InstallAttributionAcceptanceData

+ (BOOL)supportsSecureCoding {
  return YES;
}

- (void)encodeWithCoder:(NSCoder*)coder {
  [coder encodeObject:self.placementID forKey:@"placementID"];
  [coder encodeObject:self.timestamp forKey:@"timestamp"];
}

- (instancetype)initWithCoder:(NSCoder*)coder {
  if ((self = [super init])) {
    _placementID = [coder decodeObjectOfClass:[NSNumber class]
                                       forKey:@"placementID"];
    _timestamp = [coder decodeObjectOfClass:[NSDate class] forKey:@"timestamp"];
  }
  return self;
}

@end
