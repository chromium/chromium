// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DATA_IMPORT_UI_DATA_IMPORT_CREDENTIAL_CONFLICT_MUTATOR_H_
#define IOS_CHROME_BROWSER_DATA_IMPORT_UI_DATA_IMPORT_CREDENTIAL_CONFLICT_MUTATOR_H_

#import <Foundation/Foundation.h>

@class PasswordImportItem;

/// A protocol for the mediator to handle credential import conflicts.
@protocol DataImportCredentialConflictMutator

/// Continues to import the given passwords with identifiers.
- (void)continueToImportPasswords:(NSArray<NSNumber*>*)passwordIdentifiers;

@end

#endif  // IOS_CHROME_BROWSER_DATA_IMPORT_UI_DATA_IMPORT_CREDENTIAL_CONFLICT_MUTATOR_H_
