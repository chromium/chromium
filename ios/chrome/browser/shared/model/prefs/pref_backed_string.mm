// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/prefs/pref_backed_string.h"

#import <string>

#import "base/functional/bind.h"
#import "base/strings/sys_string_conversions.h"
#import "components/prefs/pref_member.h"
#import "components/prefs/pref_service.h"

@implementation PrefBackedString {
  StringPrefMember _pref;
}

@synthesize observer = _observer;

- (id)initWithPrefService:(PrefService*)prefs
                 prefName:(std::string_view)prefName {
  self = [super init];
  if (self) {
    __weak __typeof(self) weakSelf = self;
    _pref.Init(std::string(prefName), prefs, base::BindRepeating(^() {
                 [weakSelf notifyObserver];
               }));
  }
  return self;
}

- (NSString*)value {
  return base::SysUTF8ToNSString(_pref.GetValue());
}

- (void)setValue:(NSString*)value {
  _pref.SetValue(base::SysNSStringToUTF8(value));
}

- (void)stop {
  _pref.Destroy();
}

- (void)notifyObserver {
  [_observer stringDidChange:self];
}

@end
