// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UPGRADE_MODEL_TEST_FAKE_UPGRADE_CENTER_H_
#define IOS_CHROME_BROWSER_UPGRADE_MODEL_TEST_FAKE_UPGRADE_CENTER_H_

#import "ios/chrome/browser/upgrade/model/upgrade_center.h"

// A wrappeer to NSObject for InfoBarManager.
@interface InfoBarManagerHolder : NSObject

@property(nonatomic) infobars::InfoBarManager* infoBarManager;

@end

// Fake version of UpgradeCenter that allows access to consumed values.
@interface FakeUpgradeCenter : UpgradeCenter

@property(nonatomic, readonly)
    NSDictionary<NSString*, InfoBarManagerHolder*>* infoBarManagers;

@end

#endif  // IOS_CHROME_BROWSER_UPGRADE_MODEL_TEST_FAKE_UPGRADE_CENTER_H_
