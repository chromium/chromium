// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_SHOWCASE_COMMON_PROTOCOL_ALERTER_H_
#define IOS_SHOWCASE_COMMON_PROTOCOL_ALERTER_H_

#import <UIKit/UIKit.h>

// A protocol alerter is a stub object for testing UI components. It can
// be initialized to conform to one or more protocols. When a protocol method
// is called on it, it will log the call to the console. If the alerter has
// has |baseViewController| set, it will display a UIAlert instead of logging.
@interface ProtocolAlerter : NSProxy

// Create a new alerter that responds to all of the selectors in all of the
// protocols in |protocols|.
- (instancetype)initWithProtocols:(NSArray<Protocol*>*)protocols;

// The view controller (if any) that will be used to present alerts.
@property(nonatomic, weak) UIViewController* baseViewController;

// Removes the logging for the method corresponding to |sel|.
- (void)ignoreSelector:(SEL)sel;

@end

#endif  // IOS_SHOWCASE_COMMON_PROTOCOL_ALERTER_H_
