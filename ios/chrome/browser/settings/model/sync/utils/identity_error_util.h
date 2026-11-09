// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_MODEL_SYNC_UTILS_IDENTITY_ERROR_UTIL_H_
#define IOS_CHROME_BROWSER_SETTINGS_MODEL_SYNC_UTILS_IDENTITY_ERROR_UTIL_H_

#import <Foundation/Foundation.h>

#import "components/sync/service/sync_service.h"

@class AccountErrorUIInfo;
class AuthenticationService;
@protocol SyncErrorSettingsCommandHandler;
namespace syncer {
enum class IdentityErrorDisplaySurface;
class SyncService;
}  // namespace syncer
enum class SyncState;

// Returns a data object with the needed information to display the account
// error UI. Returns nil if there is no account error to display.
AccountErrorUIInfo* GetAccountErrorUIInfo(syncer::SyncService* sync_service);

// Handles a tap on the account/sync error button by recording surface-specific
// metrics and dispatching the appropriate command to `command_handler`.
// Returns true if the opened dialog should block user interactions on the
// calling surface until dismissed.
bool HandleIdentityErrorButtonTap(
    syncer::SyncService::UserActionableError error_type,
    AuthenticationService* authentication_service,
    id<SyncErrorSettingsCommandHandler> command_handler,
    syncer::IdentityErrorDisplaySurface surface);

#endif  // IOS_CHROME_BROWSER_SETTINGS_MODEL_SYNC_UTILS_IDENTITY_ERROR_UTIL_H_
