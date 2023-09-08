// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_UTILS_FAKE_OBSERVABLE_BOOLEAN_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_UTILS_FAKE_OBSERVABLE_BOOLEAN_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/model/utils/observable_boolean.h"

// ObservableBoolean to be used in tests.
@interface FakeObservableBoolean : NSObject <ObservableBoolean>
@end

// BooleanObserver to be used in tests. It reports the number of changes made.
@interface TestBooleanObserver : NSObject <BooleanObserver>

// Number of changes made since this observer started observing.
@property(nonatomic, assign) int updateCount;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_UTILS_FAKE_OBSERVABLE_BOOLEAN_H_
