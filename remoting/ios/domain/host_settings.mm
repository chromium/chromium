// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "remoting/ios/domain/host_settings.h"

@implementation HostSettings

@synthesize hostId = _hostId;
@synthesize inputMode = _inputMode;
@synthesize shouldResizeHostToFit = _shouldResizeHostToFit;

- (id)initWithCoder:(NSCoder*)coder {
  self = [super init];
  if (self) {
    self.hostId = [coder decodeObjectForKey:@"hostId"];
    NSNumber* mode = [coder decodeObjectForKey:@"inputMode"];
    self.inputMode = (ClientInputMode)[mode intValue];
    self.shouldResizeHostToFit =
        [[coder decodeObjectForKey:@"shouldResizeHostToFit"] boolValue];
  }
  return self;
}

- (void)encodeWithCoder:(NSCoder*)coder {
  [coder encodeObject:self.hostId forKey:@"hostId"];
  NSNumber* mode = [NSNumber numberWithInt:self.inputMode];
  [coder encodeObject:mode forKey:@"inputMode"];
  [coder encodeObject:@(self.shouldResizeHostToFit)
               forKey:@"shouldResizeHostToFit"];
}

- (NSString*)description {
  return [NSString stringWithFormat:@"HostSettings: hostId=%@ inputMode=%d",
                                    _hostId, (int)_inputMode];
}

@end
