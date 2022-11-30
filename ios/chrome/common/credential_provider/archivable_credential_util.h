// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_ARCHIVABLE_CREDENTIAL_UTIL_H_
#define IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_ARCHIVABLE_CREDENTIAL_UTIL_H_

#import <Foundation/Foundation.h>

// Constructs a record identifier for the given data. This should be as close
// as possible to `RecordIdentifierForPasswordForm`, as this is what is used
// to detect if a credential should be updated instead of created.
NSString* RecordIdentifierForData(NSURL* url, NSString* username);

#endif  // IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_ARCHIVABLE_CREDENTIAL_UTIL_H_
