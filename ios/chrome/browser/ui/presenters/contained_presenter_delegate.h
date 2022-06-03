// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PRESENTERS_CONTAINED_PRESENTER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_PRESENTERS_CONTAINED_PRESENTER_DELEGATE_H_

#import <Foundation/Foundation.h>

@protocol ContainedPresenter;

// Protocol for an object which acts as a delegate for a contained presenter,
// and which is informed about dismissal events.
@protocol ContainedPresenterDelegate <NSObject>

@optional

// Tells the delegate that |presenter| has finished presenting.
- (void)containedPresenterDidPresent:(id<ContainedPresenter>)presenter;

// Tells the delegate that |presenter| has finished dismissing.
- (void)containedPresenterDidDismiss:(id<ContainedPresenter>)presenter;

@end

#endif  // IOS_CHROME_BROWSER_UI_PRESENTERS_CONTAINED_PRESENTER_DELEGATE_H_
