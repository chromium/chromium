// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"

#import <string>

#import "base/functional/bind.h"
#import "components/prefs/pref_member.h"
#import "components/prefs/pref_service.h"

@implementation PrefBackedBoolean {
  BooleanPrefMember _pref;
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

- (BOOL)value {
  return _pref.GetValue();
}

- (void)setValue:(BOOL)value {
  _pref.SetValue(value == YES);
}

- (void)stop {
  _pref.Destroy();
}

- (void)notifyObserver {
  [_observer booleanDidChange:self];
}

@end
