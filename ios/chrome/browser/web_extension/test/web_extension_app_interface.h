// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_EXTENSION_TEST_WEB_EXTENSION_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_WEB_EXTENSION_TEST_WEB_EXTENSION_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

// App interface for WebExtension EarlGrey tests.
@interface WebExtensionAppInterface : NSObject

// Returns whether the `ExtensionService` is ready for the original profile.
+ (BOOL)isExtensionServiceReady;

// Returns whether the `ExtensionService` has pending callbacks waiting for it
// to become ready.
+ (BOOL)isExtensionServiceWaiting;

// Returns whether `RunWhenReady` was ever called on the `ExtensionService`.
+ (BOOL)wasExtensionServiceWaitedUpon;

// Sets whether the `ExtensionService` (when using `FakeExtensionService`)
// is ready.
+ (void)setExtensionServiceReady:(BOOL)ready;

// Returns whether the original profile is at the `kPrepareUI` stage.
+ (BOOL)isProfileAtPrepareUIStage;

// Returns whether the original profile is at or beyond the `kUIReady` stage.
+ (BOOL)isProfileUIReady;

// Returns whether the original profile is at the `kFinal` stage.
+ (BOOL)isProfileAtFinalStage;

@end

#endif  // IOS_CHROME_BROWSER_WEB_EXTENSION_TEST_WEB_EXTENSION_APP_INTERFACE_H_
