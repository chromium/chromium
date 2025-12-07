// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_UTILS_FAKE_OBSERVABLE_STRING_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_UTILS_FAKE_OBSERVABLE_STRING_H_

#import "ios/chrome/browser/shared/model/utils/observable_string.h"

// ObservableString to be used in tests.
@interface FakeObservableString : NSObject <ObservableString>
@end

// StringObserver to be used in tests. It reports the number of changes made.
@interface TestStringObserver : NSObject <StringObserver>

// Number of changes made since this observer started observing.
@property(nonatomic, assign) int updateCount;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_UTILS_FAKE_OBSERVABLE_STRING_H_
