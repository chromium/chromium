// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_INFOBAR_PASSWORD_MODAL_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_INFOBAR_PASSWORD_MODAL_CONSUMER_H_

#import <Foundation/Foundation.h>

@protocol InfobarPasswordModalConsumer <NSObject>

// The username being displayed in the InfobarModal.
- (void)setUsername:(NSString*)username;
// The masked password being displayed in the InfobarModal.
- (void)setMaskedPassword:(NSString*)maskedPassword;
// The unmasked password for the InfobarModal.
- (void)setUnmaskedPassword:(NSString*)unmaskedPassword;
// The details text message being displayed in the InfobarModal.
- (void)setDetailsTextMessage:(NSString*)detailsTextMessage;
// The URL being displayed in the InfobarModal.
- (void)setURL:(NSString*)URL;
// The text used for the save/update credentials button.
- (void)setSaveButtonText:(NSString*)saveButtonText;
// The text used for the cancel button.
- (void)setCancelButtonText:(NSString*)cancelButtonText;
// YES if the current set of credentials has already been saved.
- (void)setCurrentCredentialsSaved:(BOOL)currentCredentialsSaved;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_INFOBAR_PASSWORD_MODAL_CONSUMER_H_
