// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_COORDINATOR_IDENTITY_DOCS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_COORDINATOR_IDENTITY_DOCS_MEDIATOR_H_

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/coordinator/autofill_ai_base_mediator.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/identity_docs_table_view_controller.h"

@protocol IdentityDocsConsumer;

// Mediator for the Identity Docs settings page.
@interface IdentityDocsMediator : AutofillAIBaseMediator

// Consumer for this mediator.
@property(nonatomic, weak) id<IdentityDocsConsumer> consumer;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_COORDINATOR_IDENTITY_DOCS_MEDIATOR_H_
