// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_METRICS_UTIL_H_
#define IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_METRICS_UTIL_H_

#import <Foundation/Foundation.h>

// Increases by 1 the app group shared metrics count for given key.
void UpdateUMACountForKey(NSString* key);

// Increases the count for the given histogram and bucket by 1.
void UpdateHistogramCount(NSString* histogram, int bucket);

#endif  // IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_METRICS_UTIL_H_
