// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_UTILS_OBSERVABLE_STRING_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_UTILS_OBSERVABLE_STRING_H_

#import <Foundation/Foundation.h>

@protocol StringObserver;

// Describes the trait an observable string has.
@protocol ObservableString <NSObject>

// The value of this observable string.
@property(nonatomic, copy) NSString* value;

// The observer subscribing to this observable string's changes notifications.
@property(nonatomic, weak) id<StringObserver> observer;

@end

// Observer protocol for string changes.
@protocol StringObserver

// Called when the string changes. Note that all classes conforming to this
// protocol might send this callback even when the value is reset to the same
// value.
- (void)stringDidChange:(id<ObservableString>)observableString;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_UTILS_OBSERVABLE_STRING_H_
