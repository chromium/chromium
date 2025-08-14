// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_browser/model/install_attribution/gmo_sko_acceptance_data.h"

@implementation GMOSKOAcceptanceData

- (instancetype)initWithPlacementID:(NSNumber*)placementID
                          timestamp:(NSDate*)timestamp {
  self = [super init];
  if (self) {
    _placementID = [placementID copy];
    _timestamp = [timestamp copy];
  }
  return self;
}

#pragma mark - NSSecureCoding

// This method is required for secure coding. It tells the system this class
// supports secure archiving and unarchiving.
+ (BOOL)supportsSecureCoding {
  return YES;
}

// This method defines how to encode the object's properties into an archive.
- (void)encodeWithCoder:(NSCoder*)coder {
  [coder encodeObject:self.placementID forKey:@"placementID"];
  [coder encodeObject:self.timestamp forKey:@"timestamp"];
}

// This initializer defines how to decode the object from an archive.
- (instancetype)initWithCoder:(NSCoder*)coder {
  NSNumber* placementID = [coder decodeObjectOfClass:[NSNumber class]
                                              forKey:@"placementID"];
  NSDate* timestamp = [coder decodeObjectOfClass:[NSDate class]
                                          forKey:@"timestamp"];
  if (!placementID || !timestamp) {
    return nil;
  }
  return [self initWithPlacementID:placementID timestamp:timestamp];
}

@end
