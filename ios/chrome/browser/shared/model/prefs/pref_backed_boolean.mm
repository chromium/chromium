// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"

#import "base/functional/bind.h"
#import "components/prefs/pref_member.h"
#import "components/prefs/pref_service.h"

@implementation PrefBackedBoolean {
  BooleanPrefMember _pref;

  // Record whether -stop was called.
  BOOL _stopped;
}

@synthesize observer = _observer;

- (id)initWithPrefService:(PrefService*)prefs prefName:(const char*)prefName {
  self = [super init];
  if (self) {
    // Use weak pointer to prevent circular dependency.
    __weak PrefBackedBoolean* weakSelf = self;
    _pref.Init(prefName, prefs, base::BindRepeating(^() {
                 [weakSelf notifyObserver];
               }));
  }
  return self;
}

- (void)dealloc {
  CHECK(_stopped, base::NotFatalUntil::M140)
      << "-stop should be called on PrefBackedBoolean";
}

- (BOOL)value {
  return _pref.GetValue();
}

- (void)setValue:(BOOL)value {
  _pref.SetValue(value == YES);
}

- (void)stop {
  _stopped = YES;
  _pref.Destroy();
}

- (void)notifyObserver {
  [_observer booleanDidChange:self];
}

@end
