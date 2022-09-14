// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_FAKES_FAKE_PASS_KIT_TAB_HELPER_DELEGATE_H_
#define IOS_CHROME_TEST_FAKES_FAKE_PASS_KIT_TAB_HELPER_DELEGATE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/download/pass_kit_tab_helper_delegate.h"

namespace web {
class WebState;
}  // namespace web

// PassKitTabHelperDelegate which collects all passes into `passes` array.
@interface FakePassKitTabHelperDelegate : NSObject<PassKitTabHelperDelegate>

- (instancetype)initWithWebState:(web::WebState*)webState
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// All passes presented by PassKitTabHelper. nil passes are represented with
// NSNull objects.
@property(nonatomic, readonly) NSArray* passes;
@end

#endif  // IOS_CHROME_TEST_FAKES_FAKE_PASS_KIT_TAB_HELPER_DELEGATE_H_
