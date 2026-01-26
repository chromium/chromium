// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_MODEL_IMPORT_STATS_H_
#define IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_MODEL_IMPORT_STATS_H_

#import <Foundation/Foundation.h>

// Holds counts of different credential exchange data types received from the
// exporter password manager app needed for logging metrics.
@interface ImportStats : NSObject

// Counts of credentials defined in the spec:
// https://fidoalliance.org/specs/cx/cxf-v1.0-ps-20250814.html#sctn-credential-data-types
@property(nonatomic, assign) NSInteger addressCount;
@property(nonatomic, assign) NSInteger apiKeyCount;
@property(nonatomic, assign) NSInteger basicAuthenticationCount;
@property(nonatomic, assign) NSInteger creditCardCount;
@property(nonatomic, assign) NSInteger customFieldsCount;
@property(nonatomic, assign) NSInteger driversLicenseCount;
@property(nonatomic, assign) NSInteger generatedPasswordCount;
@property(nonatomic, assign) NSInteger identityDocumentCount;
@property(nonatomic, assign) NSInteger itemReferenceCount;
@property(nonatomic, assign) NSInteger noteCount;
@property(nonatomic, assign) NSInteger passkeyCount;
@property(nonatomic, assign) NSInteger passportCount;
@property(nonatomic, assign) NSInteger personNameCount;
@property(nonatomic, assign) NSInteger sshKeyCount;
@property(nonatomic, assign) NSInteger totpCount;
@property(nonatomic, assign) NSInteger wifiCount;

// This counts specific occurrences of the note credential, where it's used
// as a note for password credential.
@property(nonatomic, assign) NSInteger noteForPasswordCount;

@end

#endif  // IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_MODEL_IMPORT_STATS_H_
