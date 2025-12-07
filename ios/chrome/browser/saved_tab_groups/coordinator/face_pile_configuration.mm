// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/saved_tab_groups/coordinator/face_pile_configuration.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"

@implementation FacePileConfiguration

- (BOOL)isEqual:(id)object {
  if (self == object) {
    return YES;
  }

  if (![object isMemberOfClass:[FacePileConfiguration class]]) {
    return NO;
  }

  FacePileConfiguration* other =
      base::apple::ObjCCast<FacePileConfiguration>(object);
  return (self.groupID == other.groupID) &&
         ([self.backgroundColor isEqual:other.backgroundColor]) &&
         (self.showsEmptyState == other.showsEmptyState) &&
         (self.avatarSize == other.avatarSize);
}

- (NSUInteger)hash {
  NSUInteger result = [base::SysUTF8ToNSString(self.groupID.value()) hash];
  result ^= [self.backgroundColor hash];
  result ^= static_cast<NSUInteger>(self.showsEmptyState);
  result ^= static_cast<NSUInteger>(self.avatarSize);
  return result;
}

@end
