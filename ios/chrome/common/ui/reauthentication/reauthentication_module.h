// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_UI_REAUTHENTICATION_REAUTHENTICATION_MODULE_H_
#define IOS_CHROME_COMMON_UI_REAUTHENTICATION_REAUTHENTICATION_MODULE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/common/ui/reauthentication/reauthentication_protocol.h"

namespace base {
class Clock;
}  // namespace base

// A help article on how to set up a passcode.
extern const char kPasscodeArticleURL[];

// This is used before accessing sensitive data or performing sensitive
// operations.
@interface ReauthenticationModule : NSObject <ReauthenticationProtocol>

// The designated initializer. `clock` must not be null.
- (instancetype)initWithClock:(base::Clock*)clock NS_DESIGNATED_INITIALIZER;

// Convenience initializer using base::DefaultClock() as `clock`.
- (instancetype)init;

@end

#endif  // IOS_CHROME_COMMON_UI_REAUTHENTICATION_REAUTHENTICATION_MODULE_H_
