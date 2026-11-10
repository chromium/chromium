// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_TEST_GEMINI_ACTUATION_WORKLOG_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_TEST_GEMINI_ACTUATION_WORKLOG_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

// App interface to interact with the Gemini actuation worklog UI and
// `ActorService` from integration tests.
@interface GeminiActuationWorklogAppInterface : NSObject

// Starts the `ActuationWorklogCoordinator` on minimal UI and presents its view
// controller.
+ (void)startActuationWorklogCoordinatorOnMinimalUI;

// Stops the `ActuationWorklogCoordinator` on minimal UI and dismisses its view
// controller.
+ (void)stopActuationWorklogCoordinatorOnMinimalUI;

// Stops observing the current task in the shared `ActuationWorklogCoordinator`.
+ (void)stopObservingWorklogTask;

// Switches the shared `ActuationWorklogCoordinator`'s view controller to
// `ActuationWorklogDisplayModeExpanded`.
+ (void)setWorklogDisplayModeExpanded;

// Returns `YES` if there is an active task in `ActorService`.
+ (BOOL)hasActiveTask;

// Starts an Actor task with credential suggestions for `usernames` and displays
// them in the actuation worklog coordinator. Invokes `completionHandler` with
// the chosen username (or `nil` if the user chooses "Enter password manually"),
// and then executes the serialized `Action` in `nextActionProto` if the user
// confirmed with "Allow" and `nextActionProto` is non-nil.
+ (void)startWorklogTaskWithCredentialSuggestions:(NSArray<NSString*>*)usernames
                                completionHandler:(void (^)(NSString* username))
                                                      completionHandler
                                       nextAction:(NSData*)nextActionProto;

// Enqueues a stop of the active task in `ActorService`. The task stops on a
// later turn of the main run loop.
+ (void)stopActiveTask;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_TEST_GEMINI_ACTUATION_WORKLOG_APP_INTERFACE_H_
