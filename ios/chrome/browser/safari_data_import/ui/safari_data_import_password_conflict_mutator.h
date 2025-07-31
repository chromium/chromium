// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_UI_SAFARI_DATA_IMPORT_PASSWORD_CONFLICT_MUTATOR_H_
#define IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_UI_SAFARI_DATA_IMPORT_PASSWORD_CONFLICT_MUTATOR_H_

#import <Foundation/Foundation.h>

@class PasswordImportItem;

/// A protocol for the mediator to handle password import conflicts.
@protocol SafariDataImportPasswordConflictMutator

/// Continues to import the given passwords with identifiers.
- (void)continueToImportPasswords:(NSArray<NSNumber*>*)passwordIdentifiers;

@end

#endif  // IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_UI_SAFARI_DATA_IMPORT_PASSWORD_CONFLICT_MUTATOR_H_
