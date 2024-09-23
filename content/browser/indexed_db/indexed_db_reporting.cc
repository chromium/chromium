// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_reporting.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/leveldatabase/env_chromium.h"

namespace content::indexed_db {

namespace {

std::string BucketLocatorToCustomHistogramSuffix(
    const storage::BucketLocator& bucket_locator) {
  if (bucket_locator.storage_key.origin().host() == "docs.google.com")
    return ".Docs";
  return std::string();
}

void ParseAndReportIOErrorDetails(const std::string& histogram_name,
                                  const leveldb::Status& s) {
  leveldb_env::MethodID method;
  base::File::Error error = base::File::FILE_OK;
  leveldb_env::ErrorParsingResult result =
      leveldb_env::ParseMethodAndError(s, &method, &error);
  if (result == leveldb_env::NONE)
    return;
  base::LinearHistogram::FactoryGet(
      base::StrCat({histogram_name, ".EnvMethod"}), 1, leveldb_env::kNumEntries,
      leveldb_env::kNumEntries + 1,
      base::HistogramBase::kUmaTargetedHistogramFlag)
      ->Add(method);

  if (result == leveldb_env::METHOD_AND_BFE) {
    DCHECK_LT(error, 0);
    base::LinearHistogram::FactoryGet(
        base::StrCat(
            {histogram_name, ".BFE.", leveldb_env::MethodIDToString(method)}),
        1, -base::File::FILE_ERROR_MAX, -base::File::FILE_ERROR_MAX + 1,
        base::HistogramBase::kUmaTargetedHistogramFlag)
        ->Add(-error);
  }
}

void ParseAndReportCorruptionDetails(const std::string& histogram_name,
                                     const leveldb::Status& status) {
  int error = leveldb_env::GetCorruptionCode(status);
  DCHECK_GE(error, 0);
  const int kNumPatterns = leveldb_env::GetNumCorruptionCodes();
  base::LinearHistogram::FactoryGet(
      base::StrCat({histogram_name, ".Corruption"}), 1, kNumPatterns,
      kNumPatterns + 1, base::HistogramBase::kUmaTargetedHistogramFlag)
      ->Add(error);
}

}  // namespace

void ReportOpenStatus(BackingStoreOpenResult result,
                      const storage::BucketLocator& bucket_locator) {
  base::UmaHistogramEnumeration("WebCore.IndexedDB.BackingStore.OpenStatus",
                                result, INDEXED_DB_BACKING_STORE_OPEN_MAX);
  const std::string suffix =
      BucketLocatorToCustomHistogramSuffix(bucket_locator);
  // Data from the WebCore.IndexedDB.BackingStore.OpenStatus histogram is used
  // to generate a graph. So as not to alter the meaning of that graph,
  // continue to collect all stats there (above) but also now collect docs stats
  // separately (below).
  if (!suffix.empty()) {
    base::LinearHistogram::FactoryGet(
        base::StrCat({"WebCore.IndexedDB.BackingStore.OpenStatus", suffix}), 1,
        INDEXED_DB_BACKING_STORE_OPEN_MAX,
        INDEXED_DB_BACKING_STORE_OPEN_MAX + 1,
        base::HistogramBase::kUmaTargetedHistogramFlag)
        ->Add(result);
  }
}

void ReportInternalError(const char* type, BackingStoreErrorSource location) {
  base::Histogram::FactoryGet(
      base::StrCat({"WebCore.IndexedDB.BackingStore.", type, "Error"}), 1,
      INTERNAL_ERROR_MAX, INTERNAL_ERROR_MAX + 1,
      base::HistogramBase::kUmaTargetedHistogramFlag)
      ->Add(location);
}

void ReportLevelDBError(const std::string& histogram_name,
                        const leveldb::Status& s) {
  if (s.ok()) {
    NOTREACHED_IN_MIGRATION();
    return;
  }
  enum {
    LEVEL_DB_NOT_FOUND,
    LEVEL_DB_CORRUPTION,
    LEVEL_DB_IO_ERROR,
    LEVEL_DB_OTHER,
    LEVEL_DB_MAX_ERROR
  };
  int leveldb_error = LEVEL_DB_OTHER;
  if (s.IsNotFound())
    leveldb_error = LEVEL_DB_NOT_FOUND;
  else if (s.IsCorruption())
    leveldb_error = LEVEL_DB_CORRUPTION;
  else if (s.IsIOError())
    leveldb_error = LEVEL_DB_IO_ERROR;
  base::Histogram::FactoryGet(histogram_name, 1, LEVEL_DB_MAX_ERROR,
                              LEVEL_DB_MAX_ERROR + 1,
                              base::HistogramBase::kUmaTargetedHistogramFlag)
      ->Add(leveldb_error);
  if (s.IsIOError())
    ParseAndReportIOErrorDetails(histogram_name, s);
  else
    ParseAndReportCorruptionDetails(histogram_name, s);
}

}  // namespace content::indexed_db
