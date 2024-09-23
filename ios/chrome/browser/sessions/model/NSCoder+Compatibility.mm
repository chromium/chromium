// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/model/NSCoder+Compatibility.h"

#import "base/check.h"

namespace {
// Note: `NSNotFound` is equal to `NSIntegerMax` in 32-bit and 64-bit that
// in turn is initialized to `LONG_MAX`. On a 32-bit build, `INT_MAX` and
// `LONG_MAX` have the same value, however, in a 64-bit build, `LONG_MAX`
// is much larger, so we define `NSNotFound32` to `INT_MAX` that has the
// same value in both 32-bit and 64-bit builds.
enum { NSNotFound32 = INT_MAX };
}  // namespace

@implementation NSCoder (Compatibility)

- (NSInteger)cr_decodeIndexForKey:(NSString*)key {
  int32_t index32 = [self decodeInt32ForKey:key];
  DCHECK(0 <= index32 && index32 <= NSNotFound32);
  return index32 == NSNotFound32 ? NSNotFound : static_cast<NSInteger>(index32);
}

- (void)cr_encodeIndex:(NSInteger)index forKey:(NSString*)key {
  DCHECK((0 <= index && index < NSNotFound32) || index == NSNotFound);
  int32_t index32 = index == NSNotFound ? static_cast<int32_t>(NSNotFound32)
                                        : static_cast<int32_t>(index);
  [self encodeInt32:index32 forKey:key];
}

@end
