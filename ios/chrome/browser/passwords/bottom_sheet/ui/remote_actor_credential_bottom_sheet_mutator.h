// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_UI_REMOTE_ACTOR_CREDENTIAL_BOTTOM_SHEET_MUTATOR_H_
#define IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_UI_REMOTE_ACTOR_CREDENTIAL_BOTTOM_SHEET_MUTATOR_H_

#import <Foundation/Foundation.h>

// Mutator protocol handling user actions on the remote actor credential bottom
// sheet.
@protocol RemoteActorCredentialBottomSheetMutator <NSObject>

// Called when the user selects the credential at `index` in the list of
// usernames provided to the consumer and confirms sharing.
- (void)didSelectCredentialAtIndex:(NSInteger)index;

// Called when the user clicks the "Cancel" button or dismisses the bottom
// sheet.
- (void)didCancel;

@end

#endif  // IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_UI_REMOTE_ACTOR_CREDENTIAL_BOTTOM_SHEET_MUTATOR_H_
