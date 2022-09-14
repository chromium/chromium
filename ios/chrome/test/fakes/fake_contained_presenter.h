// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_FAKES_FAKE_CONTAINED_PRESENTER_H_
#define IOS_CHROME_TEST_FAKES_FAKE_CONTAINED_PRESENTER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/presenters/contained_presenter.h"

// ContainedPresenter used for testing.
@interface FakeContainedPresenter : NSObject<ContainedPresenter>
// YES if `presentAnimated:` was called with YES.
@property(nonatomic, assign) BOOL lastPresentationWasAnimated;
@end

#endif  // IOS_CHROME_TEST_FAKES_FAKE_CONTAINED_PRESENTER_H_
