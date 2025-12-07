// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_MODEL_SYNC_UTILS_IDENTITY_ERROR_UTIL_H_
#define IOS_CHROME_BROWSER_SETTINGS_MODEL_SYNC_UTILS_IDENTITY_ERROR_UTIL_H_

#import <Foundation/Foundation.h>

#import "components/sync/service/sync_service.h"

@class AccountErrorUIInfo;
namespace syncer {
class SyncService;
}
enum class SyncState;

// Returns a data object with the needed information to display the account
// error UI. Returns nil if there is no account error to display.
AccountErrorUIInfo* GetAccountErrorUIInfo(syncer::SyncService* sync_service);

#endif  // IOS_CHROME_BROWSER_SETTINGS_MODEL_SYNC_UTILS_IDENTITY_ERROR_UTIL_H_
