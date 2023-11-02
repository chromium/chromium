// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_TEST_OBSERVER_H_
#define IOS_WEB_VIEW_TEST_OBSERVER_H_

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

// Observes a KVO compliant property. To use this Observer, create an instance
// and call |setObservedObject:keyPath:|. Then test expected values against
// |lastValue|.
@interface Observer : NSObject

// The last value of performing |keyPath| on |object| after being notified of a
// KVO value change or null if a change has not been observed.
@property(nonatomic, nullable, readonly) id lastValue;

// The previous value of |lastValue| or null if at least two changes have not
// been observed.
@property(nonatomic, nullable, readonly) id previousValue;

// The |keyPath| of |object| being observed.
@property(nonatomic, nullable, readonly) NSString* keyPath;

// The current |object| being observed.
@property(nonatomic, nullable, readonly, weak) NSObject* object;

// Sets the |object| and |keyPath| to observe. The |keyPath| property of
// |object| must exist and be KVO compliant.
- (void)setObservedObject:(NSObject*)object keyPath:(NSString*)keyPath;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_TEST_OBSERVER_H_
