// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DATA_IMPORT_UI_DATA_IMPORT_CREDENTIAL_CONFLICT_MUTATOR_H_
#define IOS_CHROME_BROWSER_DATA_IMPORT_UI_DATA_IMPORT_CREDENTIAL_CONFLICT_MUTATOR_H_

#import <Foundation/Foundation.h>

@class PasswordImportItem;

/// A protocol for the mediator to handle credential import conflicts.
@protocol DataImportCredentialConflictMutator

/// Continues to import the passwords and passkeys with given identifiers. The
/// provided identifiers are for the conflicting credentials that should be kept
/// (i.e. stored in the user's account).
- (void)continueToImportPasswords:(NSArray<NSNumber*>*)passwordIdentifiers
                         passkeys:(NSArray<NSNumber*>*)passkeyIdentifiers;

@end

#endif  // IOS_CHROME_BROWSER_DATA_IMPORT_UI_DATA_IMPORT_CREDENTIAL_CONFLICT_MUTATOR_H_
