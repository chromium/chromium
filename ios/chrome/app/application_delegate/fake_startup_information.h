// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_APPLICATION_DELEGATE_FAKE_STARTUP_INFORMATION_H_
#define IOS_CHROME_APP_APPLICATION_DELEGATE_FAKE_STARTUP_INFORMATION_H_

#import <UIKit/UIKit.h>

#import <optional>

#import "ios/chrome/app/application_delegate/startup_information.h"

// Fakes a class adopting the StartupInformation protocol. It only synthetizes
// the properties.
@interface FakeStartupInformation : NSObject <StartupInformation>

@property(nonatomic, assign) BOOL isTerminating;
@property(nonatomic, assign) std::optional<IOSLaunchReason> launchReason;
@property(nonatomic, assign) base::TimeDelta preMainDuration;

@end
#endif  // IOS_CHROME_APP_APPLICATION_DELEGATE_FAKE_STARTUP_INFORMATION_H_
