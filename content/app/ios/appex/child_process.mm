// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#include "base/allocator/early_zone_registration_apple.h"

@interface EarlyInitializationObject : NSObject
@end

@implementation EarlyInitializationObject
+ (void)load {
  // Perform malloc zone registration early. See
  // https://developer.apple.com/documentation/objectivec/nsobject-swift.class/load()?language=objc#Discussion
  partition_alloc::EarlyMallocZoneRegistration();
}
@end

#define IOS_INIT_EXPORT __attribute__((visibility("default")))

extern "C" void IOS_INIT_EXPORT ChildProcessStarted() {
  // TODO(crbug.com/419046227): Investigate why this call from swift to this
  // ObjC call is necessarily.
}
