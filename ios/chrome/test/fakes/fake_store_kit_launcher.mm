// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/fakes/fake_store_kit_launcher.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation FakeStoreKitLauncher
@synthesize launchedProductID = _launchedProductID;
@synthesize launchedProductParams = _launchedProductParams;

#pragma mark - StoreKitLauncher
- (void)openAppStore:(NSString*)productID {
  _launchedProductID = [productID copy];
}

- (void)openAppStoreWithParameters:(NSDictionary*)productParameters {
  _launchedProductParams = [productParameters copy];
}
@end
