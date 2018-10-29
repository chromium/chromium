// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/utils/pref_backed_boolean.h"

#include "base/bind.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation PrefBackedBoolean {
  BooleanPrefMember _pref;
}

@synthesize observer = _observer;

- (id)initWithPrefService:(PrefService*)prefs prefName:(const char*)prefName {
  self = [super init];
  if (self) {
    // Use weak pointer to prevent circular dependency.
    __weak PrefBackedBoolean* weakSelf = self;
    _pref.Init(prefName, prefs, base::BindRepeating(^() {
                 PrefBackedBoolean* strongSelf = weakSelf;
                 if (strongSelf) {
                   [strongSelf.observer booleanDidChange:strongSelf];
                 }
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

@end
