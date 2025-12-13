// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_PERSIST_TAB_CONTEXT_METRICS_PERSIST_TAB_CONTEXT_METRICS_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_PERSIST_TAB_CONTEXT_METRICS_PERSIST_TAB_CONTEXT_METRICS_H_

// UMA histogram names.
extern const char kPersistTabContextSizeHistogram[];
extern const char kWriteTabContextResultHistogram[];
extern const char kDeleteTabContextResultHistogram[];
extern const char kReadTabContextResultHistogram[];
extern const char kCreateDirectoryResultHistogram[];
extern const char kPersistTabContextWriteTimeHistogram[];
extern const char kPersistTabContextReadTimeHistogram[];
extern const char kPersistTabContextDeleteTimeHistogram[];
extern const char kPersistTabContextStorageDifferenceHistogram[];
extern const char kPersistTabContextPurgeFileResultHistogram[];
extern const char kPersistTabContextDeleteDirectoryResultHistogram[];

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// Enum for IOS.PersistTabContext.ReadResult histogram.
// LINT.IfChange(IOSPersistTabContextReadResult)
enum class IOSPersistTabContextReadResult {
  kSuccess = 0,
  kFileNotFound = 1,
  kStoragePathEmptyFailure = 2,
  kReadFailure = 3,   // File exists but read failed.
  kParseFailure = 4,  // File read, but proto parsing failed.
  kMaxValue = kParseFailure,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:IOSPersistTabContextReadResult)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// Enum for IOS.PersistTabContext.WriteResult histogram.
// LINT.IfChange(IOSPersistTabContextWriteResult)
enum class IOSPersistTabContextWriteResult {
  kSuccess = 0,
  kStoragePathEmptyFailure = 1,
  kWriteFailure = 2,
  kMaxValue = kWriteFailure,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:IOSPersistTabContextWriteResult)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// Enum for the IOS.PersistTabContext.DeleteResult
// histogram.
// LINT.IfChange(IOSPersistTabContextDeleteResult)
enum class IOSPersistTabContextDeleteResult {
  kSuccess = 0,
  kStoragePathEmptyFailure = 1,
  kDeleteFailure = 2,
  kMaxValue = kDeleteFailure,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:IOSPersistTabContextDeleteResult)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// Enum for the IOS.PersistTabContext.DeleteDirectoryResult histogram.
// LINT.IfChange(IOSPersistTabContextDeleteDirectoryResult)
enum class IOSPersistTabContextDeleteDirectoryResult {
  kSuccess = 0,
  kDirectoryNotFound = 1,
  kDeleteFailure = 2,
  kMaxValue = kDeleteFailure,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:IOSPersistTabContextDeleteDirectoryResult)

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_PERSIST_TAB_CONTEXT_METRICS_PERSIST_TAB_CONTEXT_METRICS_H_
