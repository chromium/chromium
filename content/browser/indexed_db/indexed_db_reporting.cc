// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_reporting.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"
#include "content/browser/indexed_db/indexed_db_leveldb_env.h"
#include "url/origin.h"

namespace content {
namespace indexed_db {

namespace {

std::string OriginToCustomHistogramSuffix(const url::Origin& origin) {
  if (origin.host() == "docs.google.com")
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

void ReportOpenStatus(IndexedDBBackingStoreOpenResult result,
                      const url::Origin& origin) {
  base::UmaHistogramEnumeration("WebCore.IndexedDB.BackingStore.OpenStatus",
                                result, INDEXED_DB_BACKING_STORE_OPEN_MAX);
  const std::string suffix = OriginToCustomHistogramSuffix(origin);
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

void ReportInternalError(const char* type,
                         IndexedDBBackingStoreErrorSource location) {
  base::Histogram::FactoryGet(
      base::StrCat({"WebCore.IndexedDB.BackingStore.", type, "Error"}), 1,
      INTERNAL_ERROR_MAX, INTERNAL_ERROR_MAX + 1,
      base::HistogramBase::kUmaTargetedHistogramFlag)
      ->Add(location);
}

void ReportSchemaVersion(int version, const url::Origin& origin) {
  UMA_HISTOGRAM_ENUMERATION("WebCore.IndexedDB.SchemaVersion", version,
                            kLatestKnownSchemaVersion + 1);
  const std::string suffix = OriginToCustomHistogramSuffix(origin);
  if (!suffix.empty()) {
    base::LinearHistogram::FactoryGet(
        base::StrCat({"WebCore.IndexedDB.SchemaVersion", suffix}), 0,
        indexed_db::kLatestKnownSchemaVersion,
        indexed_db::kLatestKnownSchemaVersion + 1,
        base::HistogramBase::kUmaTargetedHistogramFlag)
        ->Add(version);
  }
}

void ReportV2Schema(bool has_broken_blobs, const url::Origin& origin) {
  base::UmaHistogramBoolean("WebCore.IndexedDB.SchemaV2HasBlobs",
                            has_broken_blobs);
  const std::string suffix = OriginToCustomHistogramSuffix(origin);
  if (!suffix.empty()) {
    base::BooleanHistogram::FactoryGet(
        base::StrCat({"WebCore.IndexedDB.SchemaV2HasBlobs", suffix}),
        base::HistogramBase::kUmaTargetedHistogramFlag)
        ->Add(has_broken_blobs);
  }
}

void ReportLevelDBError(const std::string& histogram_name,
                        const leveldb::Status& s) {
  if (s.ok()) {
    NOTREACHED();
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

}  // namespace indexed_db
}  // namespace content
