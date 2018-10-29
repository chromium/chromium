// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_FORM_OBSERVER_HELPER_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_FORM_OBSERVER_HELPER_H_

#import <Foundation/Foundation.h>

@protocol FormActivityObserver;
class WebStateList;

// Convenience object to observe updates in forms. It abstracts all the logic
// to observe a web state list and its web states.
@interface FormObserverHelper : NSObject

// The delegate interested in the form activity.
@property(nonatomic, weak) id<FormActivityObserver> delegate;

// Returns a fresh object observing the active web state for the passed list.
- (instancetype)initWithWebStateList:(WebStateList*)webStateList;

// Not available. Use |initWithWebStateList:|.
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_FORM_OBSERVER_HELPER_H_
