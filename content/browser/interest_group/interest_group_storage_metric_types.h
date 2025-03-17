// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_STORAGE_METRIC_TYPES_H_
#define CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_STORAGE_METRIC_TYPES_H_

namespace content {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(AdProtoDecompressionOutcome)
enum class AdProtoDecompressionOutcome {
  kSuccess = 0,
  kFailure = 1,

  kMaxValue = kFailure,
};

// LINT.ThenChange(//tools/metrics/histograms/metadata/storage/enums.xml:AdProtoDecompressionOutcome)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(InterestGroupStorageInitializationResult)
enum class InterestGroupStorageInitializationResult {
  kSuccessAlreadyCurrent = 0,
  kSuccessUpgraded = 1,
  kSuccessCreateSchema = 2,
  kSuccessCreateSchemaAfterIncompatibleRaze = 3,
  kSuccessCreateSchemaAfterNoMetaTableRaze = 4,
  kFailedCreateInMemory = 5,
  kFailedCreateDirectory = 6,
  kFailedCreateFile = 7,
  kFailedToRazeIncompatible = 8,
  kFailedToRazeNoMetaTable = 9,
  kFailedMetaTableInit = 10,
  kFailedCreateSchema = 11,
  kFailedCreateSchemaAfterIncompatibleRaze = 12,
  kFailedCreateSchemaAfterNoMetaTableRaze = 13,
  kFailedUpgradeDB = 14,

  kMaxValue = kFailedUpgradeDB,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/storage/enums.xml:InterestGroupStorageInitializationResult)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(InterestGroupStorageJSONDeserializationResult)
enum class InterestGroupStorageJSONDeserializationResult {
  kSucceeded = 0,
  kFailed = 1,

  kMaxValue = kFailed,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/storage/enums.xml:InterestGroupStorageJSONDeserializationResult)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(InterestGroupStorageJSONSerializationResult)
enum class InterestGroupStorageJSONSerializationResult {
  kSucceeded = 0,
  kFailed = 1,

  kMaxValue = kFailed,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/storage/enums.xml:InterestGroupStorageJSONSerializationResult)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(InterestGroupStorageProtoDeserializationResult)
enum class InterestGroupStorageProtoDeserializationResult {
  kSucceeded = 0,
  kFailed = 1,

  kMaxValue = kFailed,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/storage/enums.xml:InterestGroupStorageProtoDeserializationResult)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(InterestGroupStorageProtoSerializationResult)
enum class InterestGroupStorageProtoSerializationResult {
  kSucceeded = 0,
  kFailed = 1,

  kMaxValue = kFailed,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/storage/enums.xml:InterestGroupStorageProtoSerializationResult)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(InterestGroupStorageVacuumResult)
enum class InterestGroupStorageVacuumResult {
  kSucceeded = 0,
  kFailed = 1,

  kMaxValue = kFailed,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/storage/enums.xml:InterestGroupStorageVacuumResult)

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_STORAGE_METRIC_TYPES_H_
