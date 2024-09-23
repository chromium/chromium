// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_MODEL_SYNC_UTILS_SYNC_UTIL_H_
#define IOS_CHROME_BROWSER_SETTINGS_MODEL_SYNC_UTILS_SYNC_UTIL_H_

#import <Foundation/Foundation.h>

#import "components/sync/service/sync_service.h"
#import "google_apis/gaia/google_service_auth_error.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

@class AccountErrorUIInfo;
@protocol SyncPresenter;

namespace web {
class WebState;
}

// Gets the top-level description message associated with the sync error state
// of `syncService`. Returns nil if there is no sync error.
NSString* GetSyncErrorDescriptionForSyncService(
    syncer::SyncService* syncService);

// Gets the title of the Sync error info bar.
std::u16string GetSyncErrorInfoBarTitleForProfile(ProfileIOS* profile);

// Gets the string message associated with the sync error state of
// `profile`. The returned error message does not contain any links.
// Returns nil if there is no sync error.
NSString* GetSyncErrorMessageForProfile(ProfileIOS* profile);

// Gets the title of the button to fix the sync error of `profile`.
// Returns nil if there is no sync error or it can't be fixed by a user action.
NSString* GetSyncErrorButtonTitleForProfile(ProfileIOS* profile);

// Returns true if sync settings (or the google services settings when unified
// consent is enabled) should be displayed based on `error`.
bool ShouldShowSyncSettings(syncer::SyncService::UserActionableError error);

// Check for sync errors, and display any that ought to be shown to the user.
// Returns true if an infobar was brought up.
bool DisplaySyncErrors(ProfileIOS* profile,
                       web::WebState* web_state,
                       id<SyncPresenter> presenter);

#endif  // IOS_CHROME_BROWSER_SETTINGS_MODEL_SYNC_UTILS_SYNC_UTIL_H_
