// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_TESTING_PROTOCOL_FAKE_H_
#define IOS_TESTING_PROTOCOL_FAKE_H_

#import <UIKit/UIKit.h>

// A protocol fake is a stub object for testing any API that has a dependency
// defined by a protocol, and where the test doesn't require the dependency to
// do anything specific. An instance of ProtocolFake can be initialized to
// conform to one or more protocols. When a protocol method is called on it, it
// will record the number of times each method is called.
// If the `logs` flag is set, it will log the call to the console.
// If `alerts` is set and a baseViewController is provided, it will show an
// alert for the method call. This can be used for testing UI components.
@interface ProtocolFake : NSProxy

// If YES, then method calls are logged to the console. Default is YES.
@property(nonatomic) BOOL logs;
// If YES and `baseViewController` is set, shows an alert view for each
// method. Default is NO.
@property(nonatomic) BOOL alerts;
// The view controller (if any) that will be used to present alerts.
@property(nonatomic, weak) UIViewController* baseViewController;

// Create a new fake that responds to all of the selectors in all of the
// protocols in `protocols`.
- (instancetype)initWithProtocols:(NSArray<Protocol*>*)protocols;

// Returns the number of times `sel` has been called.
- (NSInteger)callCountForSelector:(SEL)sel;

// Removes all actions (counting, logging, and alerting) for the method
// corresponding to `sel`.
- (void)ignoreSelector:(SEL)sel;

@end

#endif  // IOS_TESTING_PROTOCOL_FAKE_H_
