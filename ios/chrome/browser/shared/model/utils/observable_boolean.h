// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_UTILS_OBSERVABLE_BOOLEAN_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_UTILS_OBSERVABLE_BOOLEAN_H_

#import <Foundation/Foundation.h>

@protocol BooleanObserver;

// Describes the trait an observable boolean has.
@protocol ObservableBoolean <NSObject>

// The value of this observable boolean.
@property(nonatomic, assign) BOOL value;

// The observer subscribing to this observable boolean's changes notifications.
@property(nonatomic, weak) id<BooleanObserver> observer;

@end

// Observer protocol for boolean changes.
@protocol BooleanObserver

// Called when the boolean changes. Note that all classes conforming to this
// protocol might send this callback even when the value is reset to the same
// value. This is the case for ContentSettingBackedBoolean for example.
- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_UTILS_OBSERVABLE_BOOLEAN_H_
