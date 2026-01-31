// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_MODEL_METRICS_UTIL_H_
#define IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_MODEL_METRICS_UTIL_H_

#import <Foundation/Foundation.h>

@class ImportStats;

// Logs counts of different credential types received via credential exchange
// import.
void LogImportStats(ImportStats* stats);

#endif  // IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_MODEL_METRICS_UTIL_H_
