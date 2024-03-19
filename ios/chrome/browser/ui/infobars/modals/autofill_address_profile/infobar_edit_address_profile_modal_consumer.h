// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_AUTOFILL_ADDRESS_PROFILE_INFOBAR_EDIT_ADDRESS_PROFILE_MODAL_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_AUTOFILL_ADDRESS_PROFILE_INFOBAR_EDIT_ADDRESS_PROFILE_MODAL_CONSUMER_H_

#import <Foundation/Foundation.h>

// Consumer for model to push configurations to the EDITAddressProfile UI.
@protocol InfobarEditAddressProfileModalConsumer <NSObject>

// Informs the consumer if the edit is done for updating the profile.
- (void)setIsEditForUpdate:(BOOL)isEditForUpdate;

// Informs the consumer that the edit is shown for the migration prompt.
- (void)setMigrationPrompt:(BOOL)migrationPrompt;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_AUTOFILL_ADDRESS_PROFILE_INFOBAR_EDIT_ADDRESS_PROFILE_MODAL_CONSUMER_H_
