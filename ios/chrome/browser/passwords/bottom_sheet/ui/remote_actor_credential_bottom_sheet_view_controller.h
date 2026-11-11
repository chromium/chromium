// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_UI_REMOTE_ACTOR_CREDENTIAL_BOTTOM_SHEET_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_UI_REMOTE_ACTOR_CREDENTIAL_BOTTOM_SHEET_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/passwords/bottom_sheet/ui/remote_actor_credential_bottom_sheet_consumer.h"
#import "ios/chrome/browser/shared/ui/bottom_sheet/table_view_bottom_sheet_view_controller.h"

@protocol RemoteActorCredentialBottomSheetMutator;

// View controller for the remote actor credential bottom sheet triggered by a
// remote actor credential sharing request (e.g. Gemini).
@interface RemoteActorCredentialBottomSheetViewController
    : TableViewBottomSheetViewController <
          RemoteActorCredentialBottomSheetConsumer>

// Mutator handling the user actions on the bottom sheet.
@property(nonatomic, weak) id<RemoteActorCredentialBottomSheetMutator> mutator;

- (instancetype)init NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithConfiguration:(ButtonStackConfiguration*)configuration
    NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_UI_REMOTE_ACTOR_CREDENTIAL_BOTTOM_SHEET_VIEW_CONTROLLER_H_
