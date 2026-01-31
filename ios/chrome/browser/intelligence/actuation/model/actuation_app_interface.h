// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_MODEL_ACTUATION_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_MODEL_ACTUATION_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

// Error domain for ActuationAppInterface.
extern NSString* const kActuationAppInterfaceErrorDomain;

// Error codes for ActuationAppInterface.
typedef NS_ENUM(NSInteger, ActuationAppInterfaceErrorCode) {
  ActuationErrorNoProfile = 1,
  ActuationErrorNoService = 2,
  ActuationErrorInvalidProto = 3,
};

// App interface to interact with the ActuationService from integration tests.
@interface ActuationAppInterface : NSObject

// Executes an Action defined by the serialized proto.
// The completion block is called when the action finishes or fails.
+ (void)executeActionWithProto:(NSData*)actionProto
                    completion:(void (^)(NSError* error))completion;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_MODEL_ACTUATION_APP_INTERFACE_H_
