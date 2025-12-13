// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_UI_BUNDLED_MODALS_INFOBAR_PASSWORD_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_INFOBARS_UI_BUNDLED_MODALS_INFOBAR_PASSWORD_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/infobars/model/infobar_type.h"
#import "ios/chrome/browser/infobars/ui_bundled/modals/infobar_password_modal_consumer.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller.h"

@class IOSChromePasswordInfobarMetricsRecorder;

@protocol InfobarPasswordTableViewControllerContainerDelegate

// The text used for the save/update credentials button.
- (void)setAcceptButtonText:(NSString*)acceptButtonText;
// The text used for the cancel button.
- (void)setCancelButtonText:(NSString*)cancelButtonText;
// YES if the current set of credentials has already been saved.
- (void)setCurrentCredentialsSaved:(BOOL)currentCredentialsSaved;
// Updates the accept button state.
- (void)updateAcceptButtonEnabled:(BOOL)enabled title:(NSString*)title;

@end

// InfobarPasswordTableViewController represents the content for the Passwords
// InfobarModal.
@interface InfobarPasswordTableViewController
    : LegacyChromeTableViewController <InfobarPasswordModalConsumer>

// Delegate containing the view controller.
@property(nonatomic, weak)
    id<InfobarPasswordTableViewControllerContainerDelegate>
        containerDelegate;
// Used to build and record metrics specific to passwords.
@property(nonatomic, strong)
    IOSChromePasswordInfobarMetricsRecorder* passwordMetricsRecorder;

@property(nonatomic, readonly) NSString* username;
@property(nonatomic, readonly) NSString* unmaskedPassword;

@end

#endif  // IOS_CHROME_BROWSER_INFOBARS_UI_BUNDLED_MODALS_INFOBAR_PASSWORD_TABLE_VIEW_CONTROLLER_H_
