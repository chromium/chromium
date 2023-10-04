// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/upgrade/model/test/fake_upgrade_center.h"

@implementation InfoBarManagerHolder

@end

@implementation FakeUpgradeCenter {
  NSMutableDictionary<NSString*, InfoBarManagerHolder*>* _infoBarManagers;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _infoBarManagers = [[NSMutableDictionary alloc] init];
  }
  return self;
}

- (NSDictionary<NSString*, InfoBarManagerHolder*>*)infoBarManagers {
  return [_infoBarManagers copy];
}

- (void)addInfoBarToManager:(infobars::InfoBarManager*)infoBarManager
                   forTabId:(NSString*)tabID {
  InfoBarManagerHolder* infoBarManagerHolder =
      [[InfoBarManagerHolder alloc] init];
  infoBarManagerHolder.infoBarManager = infoBarManager;

  [_infoBarManagers setObject:infoBarManagerHolder forKey:tabID];
}

- (void)tabWillClose:(NSString*)tabID {
  [_infoBarManagers removeObjectForKey:tabID];
}

@end
