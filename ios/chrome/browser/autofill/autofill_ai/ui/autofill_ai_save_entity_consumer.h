// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_SAVE_ENTITY_CONSUMER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_SAVE_ENTITY_CONSUMER_H_

#import <Foundation/Foundation.h>

#import <optional>

#import "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"

@class CrURL;

// Consumer for the Autofill AI entity save and update UI.
@protocol AutofillAISaveEntityConsumer <NSObject>

// Sets the entities to be displayed and managed by the consumer.
- (void)setNewEntity:(autofill::EntityInstance)newEntity
            oldEntity:(std::optional<autofill::EntityInstance>)oldEntity
    saveIsSynchronous:(BOOL)saveIsSynchronous;

// Sets the footer text (with "BEGIN_LINK"/"END_LINK" delimiters) and the
// corresponding tappable link URLs.
- (void)setFooterText:(NSString*)footerText urls:(NSArray<CrURL*>*)urls;

// Sets whether an explicit decline button ("No thanks") should be displayed in
// the button stack instead of the navigation bar cancel button.
- (void)setShowsExplicitDeclineButton:(BOOL)showsExplicitDeclineButton;

// Updates the UI to show the progress of the entity upload via the loading
// state.
- (void)showLoadingState;

// Updates the UI to show the confirmation state after successful save/update.
- (void)showConfirmationState;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_SAVE_ENTITY_CONSUMER_H_
