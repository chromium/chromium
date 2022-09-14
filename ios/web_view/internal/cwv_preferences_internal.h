// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_CWV_PREFERENCES_INTERNAL_H_
#define IOS_WEB_VIEW_INTERNAL_CWV_PREFERENCES_INTERNAL_H_

#import "ios/web_view/public/cwv_preferences.h"

NS_ASSUME_NONNULL_BEGIN

class PrefService;

@interface CWVPreferences ()

// Takes a PrefService object used to modify settings.
// Caller must ensure |prefService| outlives this instance.
- (instancetype)initWithPrefService:(PrefService*)prefService
    NS_DESIGNATED_INITIALIZER;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_INTERNAL_CWV_PREFERENCES_INTERNAL_H_
