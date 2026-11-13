// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_TEST_TAB_CONTEXT_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_SYNC_TEST_TAB_CONTEXT_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

// App-side helper for TabContext Sync and JavaScript bridge EarlGrey tests.
@interface TabContextAppInterface : NSObject

// Injects an unencrypted ephemeral `EncryptedTabContextContainerSpecifics`
// entity with `containerID` and `accessToken` into the fake sync server.
+ (void)injectEphemeralContainerWithID:(NSString*)containerID
                           accessToken:(NSString*)accessToken;

// Calls `CreateContainerPossiblyUnencryptedEphemeral()` on the profile's
// `TabContextSyncService` and returns the lowercase container UUID, or `nil`
// on failure.
+ (NSString*)createEphemeralContainer;

// Uploads `pageContext` with `entryID` to `containerID` via the profile's
// `TabContextSyncService` and waits for completion. Returns `YES` on
// `UploadOutcome::kSucceeded`.
+ (BOOL)uploadPageContext:(NSString*)pageContext
                  entryID:(NSString*)entryID
              containerID:(NSString*)containerID;

// Returns the `encrypted_content` bytes of the `ENCRYPTED_TAB_CONTEXT_ITEM`
// entity committed to the fake sync server for `containerID` and `entryID`,
// or `nil` if not found.
+ (NSData*)encryptedContentForServerItemWithContainerID:(NSString*)containerID
                                                entryID:(NSString*)entryID;

@end

#endif  // IOS_CHROME_BROWSER_SYNC_TEST_TAB_CONTEXT_APP_INTERFACE_H_
