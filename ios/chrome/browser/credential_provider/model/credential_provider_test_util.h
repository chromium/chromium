// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_MODEL_CREDENTIAL_PROVIDER_TEST_UTIL_H_
#define IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_MODEL_CREDENTIAL_PROVIDER_TEST_UTIL_H_

#import <Foundation/Foundation.h>

// Sets the folder URL for favicons in tests.
void SetFaviconsFolderURLForTesting(NSURL* url);

// Sets the max number of stored favicons in tests.
void SetMaxNumberOfFaviconsForTesting(NSUInteger max_favicons);

// Resets the max number of stored favicons in tests to default.
void ResetMaxNumberOfFaviconsForTesting();

#endif  // IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_MODEL_CREDENTIAL_PROVIDER_TEST_UTIL_H_
