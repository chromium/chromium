// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_PASSWORD_SHARING_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_PASSWORD_SHARING_MEDIATOR_DELEGATE_H_

@class RecipientInfoForIOSDisplay;

namespace password_manager {
enum class FetchFamilyMembersRequestStatus;
}  // namespace password_manager

// Delegate for PasswordSharingMediator.
@protocol PasswordSharingMediatorDelegate

// Called after the recipients fetcher API returned a result.
- (void)onFetchFamilyMembers:
            (NSArray<RecipientInfoForIOSDisplay*>*)familyMembers
                  withStatus:
                      (const password_manager::FetchFamilyMembersRequestStatus&)
                          status;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_PASSWORD_SHARING_MEDIATOR_DELEGATE_H_
