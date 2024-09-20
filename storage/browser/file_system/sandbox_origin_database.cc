// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/sandbox_origin_database.h"

#include <stdint.h>

#include <memory>
#include <set>
#include <utility>

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "storage/common/file_system/file_system_util.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

namespace {

const base::FilePath::CharType kOriginDatabaseName[] =
    FILE_PATH_LITERAL("Origins");
const char kOriginKeyPrefix[] = "ORIGIN:";
const char kSandboxOriginLastPathKey[] = "LAST_PATH";
const int64_t kSandboxOriginMinimumReportIntervalHours = 1;
const char kSandboxOriginInitStatusHistogramLabel[] =
    "FileSystem.OriginDatabaseInit";
const char kSandboxOriginDatabaseRepairHistogramLabel[] =
    "FileSystem.OriginDatabaseRepair";

enum class InitSandboxOriginStatus {
  INIT_STATUS_OK = 0,
  INIT_STATUS_CORRUPTION,
  INIT_STATUS_IO_ERROR,
  INIT_STATUS_UNKNOWN_ERROR,
  INIT_STATUS_MAX
};

enum class SandboxOriginRepairResult {
  DB_REPAIR_SUCCEEDED = 0,
  DB_REPAIR_FAILED,
  DB_REPAIR_MAX
};

std::string OriginToOriginKey(const std::string& origin) {
  std::string key(kOriginKeyPrefix);
  return key + origin;
}

const char* LastPathKey() {
  return kSandboxOriginLastPathKey;
}

}  // namespace

namespace storage {

SandboxOriginDatabase::SandboxOriginDatabase(
    const base::FilePath& file_system_directory,
    leveldb::Env* env_override)
    : file_system_directory_(file_system_directory),
      env_override_(env_override) {}

SandboxOriginDatabase::~SandboxOriginDatabase() = default;

bool SandboxOriginDatabase::Init(InitOption init_option,
                                 RecoveryOption recovery_option) {
  if (db_)
    return true;

  base::FilePath db_path = GetDatabasePath();
  if (init_option == FAIL_IF_NONEXISTENT && !base::PathExists(db_path))
    return false;

  std::string path = FilePathToString(db_path);
  leveldb_env::Options options;
  options.max_open_files = 0;  // Use minimum.
  options.create_if_missing = true;
  if (env_override_)
    options.env = env_override_;
  leveldb::Status status = leveldb_env::OpenDB(options, path, &db_);
  ReportInitStatus(status);
  if (status.ok()) {
    return true;
  }
  HandleError(FROM_HERE, status);

  // Corruption due to missing necessary MANIFEST-* file causes IOError instead
  // of Corruption error.
  // Try to repair database even when IOError case.
  if (!status.IsCorruption() && !status.IsIOError())
    return false;

  switch (recovery_option) {
    case FAIL_ON_CORRUPTION:
      return false;
    case REPAIR_ON_CORRUPTION:
      LOG(WARNING) << "Attempting to repair SandboxOriginDatabase.";

      if (RepairDatabase(path)) {
        UMA_HISTOGRAM_ENUMERATION(
            kSandboxOriginDatabaseRepairHistogramLabel,
            SandboxOriginRepairResult::DB_REPAIR_SUCCEEDED,
            SandboxOriginRepairResult::DB_REPAIR_MAX);
        LOG(WARNING) << "Repairing SandboxOriginDatabase completed.";
        return true;
      }
      UMA_HISTOGRAM_ENUMERATION(kSandboxOriginDatabaseRepairHistogramLabel,
                                SandboxOriginRepairResult::DB_REPAIR_FAILED,
                                SandboxOriginRepairResult::DB_REPAIR_MAX);
      [[fallthrough]];
    case DELETE_ON_CORRUPTION:
      if (!base::DeletePathRecursively(file_system_directory_))
        return false;
      if (!base::CreateDirectory(file_system_directory_))
        return false;
      return Init(init_option, FAIL_ON_CORRUPTION);
  }
  NOTREACHED();
}

bool SandboxOriginDatabase::RepairDatabase(const std::string& db_path) {
  CHECK(!db_.get(), base::NotFatalUntil::M130);
  leveldb_env::Options options;
  options.reuse_logs = false;
  options.max_open_files = 0;  // Use minimum.
  if (env_override_)
    options.env = env_override_;
  if (!leveldb::RepairDB(db_path, options).ok() ||
      !Init(FAIL_IF_NONEXISTENT, FAIL_ON_CORRUPTION)) {
    LOG(WARNING) << "Failed to repair SandboxOriginDatabase.";
    return false;
  }

  // See if the repaired entries match with what we have on disk.
  std::set<base::FilePath> directories;
  base::FileEnumerator file_enum(file_system_directory_, false /* recursive */,
                                 base::FileEnumerator::DIRECTORIES);
  base::FilePath path_each;
  while (!(path_each = file_enum.Next()).empty())
    directories.insert(path_each.BaseName());
  auto db_dir_itr = directories.find(base::FilePath(kOriginDatabaseName));
  // Make sure we have the database file in its directory and therefore we are
  // working on the correct path.
  CHECK(db_dir_itr != directories.end(), base::NotFatalUntil::M130);
  directories.erase(db_dir_itr);

  std::vector<OriginRecord> origins;
  if (!ListAllOrigins(&origins)) {
    DropDatabase();
    return false;
  }

  // Delete any obsolete entries from the origins database.
  for (const OriginRecord& record : origins) {
    auto dir_itr = directories.find(record.path);
    if (dir_itr == directories.end()) {
      if (!RemovePathForOrigin(record.origin)) {
        DropDatabase();
        return false;
      }
    } else {
      directories.erase(dir_itr);
    }
  }

  // Delete any directories not listed in the origins database.
  for (const base::FilePath& dir : directories) {
    if (!base::DeletePathRecursively(file_system_directory_.Append(dir))) {
      DropDatabase();
      return false;
    }
  }

  return true;
}

void SandboxOriginDatabase::HandleError(const base::Location& from_here,
                                        const leveldb::Status& status) {
  db_.reset();
  LOG(ERROR) << "SandboxOriginDatabase failed at: " << from_here.ToString()
             << " with error: " << status.ToString();
}

void SandboxOriginDatabase::ReportInitStatus(const leveldb::Status& status) {
  base::Time now = base::Time::Now();
  base::TimeDelta minimum_interval =
      base::Hours(kSandboxOriginMinimumReportIntervalHours);
  if (last_reported_time_ + minimum_interval >= now)
    return;
  last_reported_time_ = now;

  if (status.ok()) {
    UMA_HISTOGRAM_ENUMERATION(kSandboxOriginInitStatusHistogramLabel,
                              InitSandboxOriginStatus::INIT_STATUS_OK,
                              InitSandboxOriginStatus::INIT_STATUS_MAX);
  } else if (status.IsCorruption()) {
    UMA_HISTOGRAM_ENUMERATION(kSandboxOriginInitStatusHistogramLabel,
                              InitSandboxOriginStatus::INIT_STATUS_CORRUPTION,
                              InitSandboxOriginStatus::INIT_STATUS_MAX);
  } else if (status.IsIOError()) {
    UMA_HISTOGRAM_ENUMERATION(kSandboxOriginInitStatusHistogramLabel,
                              InitSandboxOriginStatus::INIT_STATUS_IO_ERROR,
                              InitSandboxOriginStatus::INIT_STATUS_MAX);
  } else {
    UMA_HISTOGRAM_ENUMERATION(
        kSandboxOriginInitStatusHistogramLabel,
        InitSandboxOriginStatus::INIT_STATUS_UNKNOWN_ERROR,
        InitSandboxOriginStatus::INIT_STATUS_MAX);
  }
}

bool SandboxOriginDatabase::HasOriginPath(const std::string& origin) {
  if (!Init(FAIL_IF_NONEXISTENT, REPAIR_ON_CORRUPTION))
    return false;
  if (origin.empty())
    return false;
  std::string path;
  leveldb::Status status =
      db_->Get(leveldb::ReadOptions(), OriginToOriginKey(origin), &path);
  if (status.ok())
    return true;
  if (status.IsNotFound())
    return false;
  HandleError(FROM_HERE, status);
  return false;
}

bool SandboxOriginDatabase::GetPathForOrigin(const std::string& origin,
                                             base::FilePath* directory) {
  if (!Init(CREATE_IF_NONEXISTENT, REPAIR_ON_CORRUPTION))
    return false;
  DCHECK(directory);
  if (origin.empty())
    return false;
  std::string path_string;
  std::string origin_key = OriginToOriginKey(origin);
  leveldb::Status status =
      db_->Get(leveldb::ReadOptions(), origin_key, &path_string);
  if (status.IsNotFound()) {
    int last_path_number;
    if (!GetLastPathNumber(&last_path_number))
      return false;
    path_string = base::StringPrintf("%03u", last_path_number + 1);
    // store both back as a single transaction
    leveldb::WriteBatch batch;
    batch.Put(LastPathKey(), path_string);
    batch.Put(origin_key, path_string);
    status = db_->Write(leveldb::WriteOptions(), &batch);
    if (!status.ok()) {
      HandleError(FROM_HERE, status);
      return false;
    }
  }
  if (status.ok()) {
    *directory = StringToFilePath(path_string);
    return true;
  }
  HandleError(FROM_HERE, status);
  return false;
}

bool SandboxOriginDatabase::RemovePathForOrigin(const std::string& origin) {
  if (!Init(CREATE_IF_NONEXISTENT, REPAIR_ON_CORRUPTION))
    return false;
  leveldb::Status status =
      db_->Delete(leveldb::WriteOptions(), OriginToOriginKey(origin));
  if (status.ok() || status.IsNotFound())
    return true;
  HandleError(FROM_HERE, status);
  return false;
}

bool SandboxOriginDatabase::ListAllOrigins(std::vector<OriginRecord>* origins) {
  DCHECK(origins);
  if (!Init(CREATE_IF_NONEXISTENT, REPAIR_ON_CORRUPTION)) {
    origins->clear();
    return false;
  }
  std::unique_ptr<leveldb::Iterator> iter(
      db_->NewIterator(leveldb::ReadOptions()));
  std::string origin_key_prefix = OriginToOriginKey(std::string());
  iter->Seek(origin_key_prefix);
  origins->clear();
  while (iter->Valid() &&
         base::StartsWith(iter->key().ToString(), origin_key_prefix,
                          base::CompareCase::SENSITIVE)) {
    std::string origin =
        iter->key().ToString().substr(origin_key_prefix.length());
    base::FilePath path = StringToFilePath(iter->value().ToString());
    origins->push_back(OriginRecord(origin, path));
    iter->Next();
  }
  return true;
}

void SandboxOriginDatabase::DropDatabase() {
  db_.reset();
}

void SandboxOriginDatabase::RewriteDatabase() {
  if (!Init(FAIL_IF_NONEXISTENT, FAIL_ON_CORRUPTION))
    return;
  base::FilePath db_path = GetDatabasePath();
  std::string path = FilePathToString(db_path);
  leveldb_env::Options options;
  options.max_open_files = 0;  // Use minimum.
  options.create_if_missing = true;
  if (env_override_)
    options.env = env_override_;
  // There is a possibility that |db_| is null after this call. This case
  // will be handled by the |!Init(...)| checks above each method.
  leveldb_env::RewriteDB(options, path, &db_);
}

base::FilePath SandboxOriginDatabase::GetDatabasePath() const {
  return file_system_directory_.Append(kOriginDatabaseName);
}

void SandboxOriginDatabase::RemoveDatabase() {
  DropDatabase();
  base::DeletePathRecursively(GetDatabasePath());
}

bool SandboxOriginDatabase::GetLastPathNumber(int* number) {
  DCHECK(db_);
  DCHECK(number);
  std::string number_string;
  leveldb::Status status =
      db_->Get(leveldb::ReadOptions(), LastPathKey(), &number_string);
  if (status.ok())
    return base::StringToInt(number_string, number);
  if (!status.IsNotFound()) {
    HandleError(FROM_HERE, status);
    return false;
  }
  // Verify that this is a totally new database, and initialize it.
  {
    // Scope the iterator to ensure it is deleted before database is closed.
    std::unique_ptr<leveldb::Iterator> iter(
        db_->NewIterator(leveldb::ReadOptions()));
    iter->SeekToFirst();
    if (iter->Valid()) {  // DB was not empty, but had no last path number!
      LOG(ERROR) << "File system origin database is corrupt!";
      return false;
    }
  }
  // This is always the first write into the database.  If we ever add a
  // version number, they should go in in a single transaction.
  status = db_->Put(leveldb::WriteOptions(), LastPathKey(), std::string("-1"));
  if (!status.ok()) {
    HandleError(FROM_HERE, status);
    return false;
  }
  *number = -1;
  return true;
}

}  // namespace storage
