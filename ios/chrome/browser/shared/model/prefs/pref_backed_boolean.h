// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_PREFS_PREF_BACKED_BOOLEAN_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_PREFS_PREF_BACKED_BOOLEAN_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/model/utils/observable_boolean.h"

class PrefService;

// An observable boolean backed by a pref from a PrefService.
@interface PrefBackedBoolean : NSObject <ObservableBoolean>

// Returns a PrefBackedBoolean backed by `prefName` from `prefs`.
- (instancetype)initWithPrefService:(PrefService*)prefs
                           prefName:(const char*)prefName
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Stop observing the pref. Can be called before -dealloc to ensure
// that the pref is no longer observed, even if the object survives
// the PrefService (e.g. if the reference is captured by a block).
- (void)stop;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_PREFS_PREF_BACKED_BOOLEAN_H_
