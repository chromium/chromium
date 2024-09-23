// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/sandbox_directory_database.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <set>

#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/containers/stack.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/pickle.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "storage/browser/file_system/file_system_usage_cache.h"
#include "storage/common/file_system/file_system_util.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/leveldb_chrome.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

namespace storage {

void PickleFromFileInfo(const SandboxDirectoryDatabase::FileInfo& info,
                        base::Pickle* pickle) {
  DCHECK(pickle);
  std::string data_path;
  // Round off here to match the behavior of the filesystem on real files.
  base::Time time = base::Time::FromSecondsSinceUnixEpoch(
      floor(info.modification_time.InSecondsFSinceUnixEpoch()));
  std::string name;

  data_path = FilePathToString(info.data_path);
  name = FilePathToString(base::FilePath(info.name));

  pickle->WriteInt64(info.parent_id);
  pickle->WriteString(data_path);
  pickle->WriteString(name);
  pickle->WriteInt64(time.ToInternalValue());
}

bool FileInfoFromPickle(const base::Pickle& pickle,
                        SandboxDirectoryDatabase::FileInfo* info) {
  base::PickleIterator iter(pickle);
  std::string data_path;
  std::string name;
  int64_t internal_time;

  if (iter.ReadInt64(&info->parent_id) && iter.ReadString(&data_path) &&
      iter.ReadString(&name) && iter.ReadInt64(&internal_time)) {
    info->data_path = StringToFilePath(data_path);
    info->name = StringToFilePath(name).value();
    info->modification_time = base::Time::FromInternalValue(internal_time);
    return true;
  }
  LOG(ERROR) << "base::Pickle could not be digested!";
  return false;
}

const base::FilePath::CharType kDirectoryDatabaseName[] =
    FILE_PATH_LITERAL("Paths");
const char kChildLookupPrefix[] = "CHILD_OF:";
const char kChildLookupSeparator[] = ":";
const char kSandboxDirectoryLastFileIdKey[] = "LAST_FILE_ID";
const char kSandboxDirectoryLastIntegerKey[] = "LAST_INTEGER";
const int64_t kSandboxDirectoryMinimumReportIntervalHours = 1;
const char kSandboxDirectoryInitStatusHistogramLabel[] =
    "FileSystem.DirectoryDatabaseInit";

// These values are recorded in UMA. Changing existing values will invalidate
// results for older Chrome releases. Only add new values.
enum class SandboxDirectoryInitStatus {
  INIT_STATUS_OK = 0,
  INIT_STATUS_CORRUPTION,
  INIT_STATUS_IO_ERROR,
  INIT_STATUS_UNKNOWN_ERROR,
  INIT_STATUS_MAX
};

std::string GetChildLookupKey(SandboxDirectoryDatabase::FileId parent_id,
                              const base::FilePath::StringType& child_name) {
  std::string name;
  name = FilePathToString(base::FilePath(child_name));
  return std::string(kChildLookupPrefix) + base::NumberToString(parent_id) +
         std::string(kChildLookupSeparator) + name;
}

std::string GetChildListingKeyPrefix(
    SandboxDirectoryDatabase::FileId parent_id) {
  return std::string(kChildLookupPrefix) + base::NumberToString(parent_id) +
         std::string(kChildLookupSeparator);
}

const char* LastFileIdKey() {
  return kSandboxDirectoryLastFileIdKey;
}

const char* LastIntegerKey() {
  return kSandboxDirectoryLastIntegerKey;
}

std::string GetFileLookupKey(SandboxDirectoryDatabase::FileId file_id) {
  return base::NumberToString(file_id);
}

// Assumptions:
//  - Any database entry is one of:
//    - ("CHILD_OF:|parent_id|:<name>", "|file_id|"),
//    - ("LAST_FILE_ID", "|last_file_id|"),
//    - ("LAST_INTEGER", "|last_integer|"),
//    - ("|file_id|", "pickled FileInfo")
//        where FileInfo has |parent_id|, |data_path|, |name| and
//        |modification_time|,
// Constraints:
//  - Each file in the database has unique backing file.
//  - Each file in |filesystem_data_directory_| has a database entry.
//  - Directory structure is tree, i.e. connected and acyclic.
class DatabaseCheckHelper {
 public:
  using FileId = SandboxDirectoryDatabase::FileId;
  using FileInfo = SandboxDirectoryDatabase::FileInfo;

  DatabaseCheckHelper(SandboxDirectoryDatabase* dir_db,
                      leveldb::DB* db,
                      const base::FilePath& path);

  bool IsFileSystemConsistent() {
    return IsDatabaseEmpty() ||
           (ScanDatabase() && ScanDirectory() && ScanHierarchy());
  }

 private:
  bool IsDatabaseEmpty();
  // These 3 methods need to be called in the order.  Each method requires its
  // previous method finished successfully. They also require the database is
  // not empty.
  bool ScanDatabase();
  bool ScanDirectory();
  bool ScanHierarchy();

  raw_ptr<SandboxDirectoryDatabase> dir_db_;
  raw_ptr<leveldb::DB> db_;
  base::FilePath path_;

  std::set<base::FilePath> files_in_db_;

  size_t num_directories_in_db_;
  size_t num_files_in_db_;
  size_t num_hierarchy_links_in_db_;

  FileId last_file_id_;
  FileId last_integer_;
};

DatabaseCheckHelper::DatabaseCheckHelper(SandboxDirectoryDatabase* dir_db,
                                         leveldb::DB* db,
                                         const base::FilePath& path)
    : dir_db_(dir_db),
      db_(db),
      path_(path),
      num_directories_in_db_(0),
      num_files_in_db_(0),
      num_hierarchy_links_in_db_(0),
      last_file_id_(-1),
      last_integer_(-1) {
  DCHECK(dir_db_);
  DCHECK(db_);
  DCHECK(!path_.empty() && base::DirectoryExists(path_));
}

bool DatabaseCheckHelper::IsDatabaseEmpty() {
  std::unique_ptr<leveldb::Iterator> itr(
      db_->NewIterator(leveldb::ReadOptions()));
  itr->SeekToFirst();
  return !itr->Valid();
}

bool DatabaseCheckHelper::ScanDatabase() {
  // Scans all database entries sequentially to verify each of them has unique
  // backing file.
  int64_t max_file_id = -1;
  std::set<FileId> file_ids;

  std::unique_ptr<leveldb::Iterator> itr(
      db_->NewIterator(leveldb::ReadOptions()));
  for (itr->SeekToFirst(); itr->Valid(); itr->Next()) {
    std::string key = itr->key().ToString();
    if (base::StartsWith(key, kChildLookupPrefix,
                         base::CompareCase::SENSITIVE)) {
      // key: "CHILD_OF:<parent_id>:<name>"
      // value: "<child_id>"
      ++num_hierarchy_links_in_db_;
    } else if (key == kSandboxDirectoryLastFileIdKey) {
      // key: "LAST_FILE_ID"
      // value: "<last_file_id>"
      if (last_file_id_ >= 0 ||
          !base::StringToInt64(itr->value().ToString(), &last_file_id_))
        return false;

      if (last_file_id_ < 0)
        return false;
    } else if (key == kSandboxDirectoryLastIntegerKey) {
      // key: "LAST_INTEGER"
      // value: "<last_integer>"
      if (last_integer_ >= 0 ||
          !base::StringToInt64(itr->value().ToString(), &last_integer_))
        return false;
    } else {
      // key: "<entry_id>"
      // value: "<pickled FileInfo>"
      FileInfo file_info;
      if (!FileInfoFromPickle(
              base::Pickle::WithUnownedBuffer(base::as_byte_span(itr->value())),
              &file_info)) {
        return false;
      }

      FileId file_id = -1;
      if (!base::StringToInt64(key, &file_id) || file_id < 0)
        return false;

      if (max_file_id < file_id)
        max_file_id = file_id;
      if (!file_ids.insert(file_id).second)
        return false;

      if (file_info.is_directory()) {
        ++num_directories_in_db_;
        DCHECK(file_info.data_path.empty());
      } else {
        // Ensure any pair of file entry don't share their data_path.
        if (!files_in_db_.insert(file_info.data_path).second)
          return false;

        // Ensure the backing file exists as a normal file.
        base::File::Info platform_file_info;
        if (!base::GetFileInfo(path_.Append(file_info.data_path),
                               &platform_file_info) ||
            platform_file_info.is_directory ||
            platform_file_info.is_symbolic_link) {
          // leveldb::Iterator iterates a snapshot of the database.
          // So even after RemoveFileInfo() call, we'll visit hierarchy link
          // from |parent_id| to |file_id|.
          if (!dir_db_->RemoveFileInfo(file_id))
            return false;
          --num_hierarchy_links_in_db_;
          files_in_db_.erase(file_info.data_path);
        } else {
          ++num_files_in_db_;
        }
      }
    }
  }

  // TODO(tzik): Add constraint for |last_integer_| to avoid possible
  // data path confliction on ObfuscatedFileUtil.
  return max_file_id <= last_file_id_;
}

bool DatabaseCheckHelper::ScanDirectory() {
  // TODO(kinuko): Scans all local file system entries to verify each of them
  // has a database entry.
  const base::FilePath kExcludes[] = {
      base::FilePath(kDirectoryDatabaseName),
      base::FilePath(FileSystemUsageCache::kUsageFileName),
  };

  // Any path in |pending_directories| is relative to |path_|.
  base::stack<base::FilePath> pending_directories;
  pending_directories.push(base::FilePath());

  while (!pending_directories.empty()) {
    base::FilePath dir_path = pending_directories.top();
    pending_directories.pop();

    base::FileEnumerator file_enum(
        dir_path.empty() ? path_ : path_.Append(dir_path),
        false /* not recursive */,
        base::FileEnumerator::DIRECTORIES | base::FileEnumerator::FILES);

    base::FilePath absolute_file_path;
    while (!(absolute_file_path = file_enum.Next()).empty()) {
      base::FileEnumerator::FileInfo find_info = file_enum.GetInfo();

      base::FilePath relative_file_path;
      if (!path_.AppendRelativePath(absolute_file_path, &relative_file_path))
        return false;

      if (base::Contains(kExcludes, relative_file_path))
        continue;

      if (find_info.IsDirectory()) {
        pending_directories.push(relative_file_path);
        continue;
      }

      // Check if the file has a database entry.
      auto itr = files_in_db_.find(relative_file_path);
      if (itr == files_in_db_.end()) {
        if (!base::DeleteFile(absolute_file_path))
          return false;
      } else {
        files_in_db_.erase(itr);
      }
    }
  }

  return files_in_db_.empty();
}

bool DatabaseCheckHelper::ScanHierarchy() {
  size_t visited_directories = 0;
  size_t visited_files = 0;
  size_t visited_links = 0;

  base::stack<FileId> directories;
  directories.push(0);

  // Check if the root directory exists as a directory.
  FileInfo file_info;
  if (!dir_db_->GetFileInfo(0, &file_info))
    return false;
  if (file_info.parent_id != 0 || !file_info.is_directory())
    return false;

  while (!directories.empty()) {
    ++visited_directories;
    FileId dir_id = directories.top();
    directories.pop();

    std::vector<FileId> children;
    if (!dir_db_->ListChildren(dir_id, &children))
      return false;
    for (const FileId& id : children) {
      // Any directory must not have root directory as child.
      if (!id)
        return false;

      // Check if the child knows the parent as its parent.
      if (!dir_db_->GetFileInfo(id, &file_info))
        return false;
      if (file_info.parent_id != dir_id)
        return false;

      // Check if the parent knows the name of its child correctly.
      FileId file_id;
      if (!dir_db_->GetChildWithName(dir_id, file_info.name, &file_id) ||
          file_id != id)
        return false;

      if (file_info.is_directory())
        directories.push(id);
      else
        ++visited_files;
      ++visited_links;
    }
  }

  // Check if we've visited all database entries.
  return num_directories_in_db_ == visited_directories &&
         num_files_in_db_ == visited_files &&
         num_hierarchy_links_in_db_ == visited_links;
}

// Returns true if the given |data_path| contains no parent references ("..")
// and does not refer to special system files.
// This is called in GetFileInfo, AddFileInfo and UpdateFileInfo to
// ensure we're only dealing with valid data paths.
bool VerifyDataPath(const base::FilePath& data_path) {
  // |data_path| should not contain any ".." and should be a relative path
  // (to the filesystem_data_directory_).
  if (data_path.ReferencesParent() || data_path.IsAbsolute())
    return false;
  // See if it's not pointing to the special system paths.
  const base::FilePath kExcludes[] = {
      base::FilePath(kDirectoryDatabaseName),
      base::FilePath(FileSystemUsageCache::kUsageFileName),
  };
  for (const auto& exclude : kExcludes) {
    if (data_path == exclude || exclude.IsParent(data_path))
      return false;
  }
  return true;
}

SandboxDirectoryDatabase::FileInfo::FileInfo() : parent_id(0) {}

SandboxDirectoryDatabase::FileInfo::~FileInfo() = default;

SandboxDirectoryDatabase::SandboxDirectoryDatabase(
    const base::FilePath& filesystem_data_directory,
    leveldb::Env* env_override)
    : filesystem_data_directory_(filesystem_data_directory),
      env_override_(env_override) {}

SandboxDirectoryDatabase::~SandboxDirectoryDatabase() = default;

bool SandboxDirectoryDatabase::GetChildWithName(
    FileId parent_id,
    const base::FilePath::StringType& name,
    FileId* child_id) {
  if (!Init(REPAIR_ON_CORRUPTION))
    return false;
  DCHECK(child_id);
  std::string child_key = GetChildLookupKey(parent_id, name);
  std::string child_id_string;
  leveldb::Status status =
      db_->Get(leveldb::ReadOptions(), child_key, &child_id_string);
  if (status.IsNotFound())
    return false;
  if (status.ok()) {
    if (!base::StringToInt64(child_id_string, child_id)) {
      LOG(ERROR) << "Hit database corruption!";
      return false;
    }
    return true;
  }
  HandleError(FROM_HERE, status);
  return false;
}

bool SandboxDirectoryDatabase::GetFileWithPath(const base::FilePath& path,
                                               FileId* file_id) {
  FileId local_id = 0;
  for (const auto& path_component : VirtualPath::GetComponents(path)) {
    if (path_component == FILE_PATH_LITERAL("/"))
      continue;
    if (!GetChildWithName(local_id, path_component, &local_id))
      return false;
  }
  *file_id = local_id;
  return true;
}

bool SandboxDirectoryDatabase::ListChildren(FileId parent_id,
                                            std::vector<FileId>* children) {
  // Check to add later: fail if parent is a file, at least in debug builds.
  if (!Init(REPAIR_ON_CORRUPTION))
    return false;
  DCHECK(children);
  std::string child_key_prefix = GetChildListingKeyPrefix(parent_id);

  std::unique_ptr<leveldb::Iterator> iter(
      db_->NewIterator(leveldb::ReadOptions()));
  iter->Seek(child_key_prefix);
  children->clear();
  while (iter->Valid() &&
         base::StartsWith(iter->key().ToString(), child_key_prefix,
                          base::CompareCase::SENSITIVE)) {
    std::string child_id_string = iter->value().ToString();
    FileId child_id;
    if (!base::StringToInt64(child_id_string, &child_id)) {
      LOG(ERROR) << "Hit database corruption!";
      return false;
    }
    children->push_back(child_id);
    iter->Next();
  }
  return true;
}

bool SandboxDirectoryDatabase::GetFileInfo(FileId file_id, FileInfo* info) {
  if (!Init(REPAIR_ON_CORRUPTION))
    return false;
  DCHECK(info);
  std::string file_key = GetFileLookupKey(file_id);
  std::string file_data_string;
  leveldb::Status status =
      db_->Get(leveldb::ReadOptions(), file_key, &file_data_string);
  if (status.ok()) {
    bool success = FileInfoFromPickle(
        base::Pickle::WithUnownedBuffer(base::as_byte_span(file_data_string)),
        info);
    if (!success)
      return false;
    if (!VerifyDataPath(info->data_path)) {
      LOG(ERROR) << "Resolved data path is invalid: "
                 << info->data_path.value();
      return false;
    }
    return true;
  }
  // Special-case the root, for databases that haven't been initialized yet.
  // Without this, a query for the root's file info, made before creating the
  // first file in the database, will fail and confuse callers.
  if (status.IsNotFound() && !file_id) {
    info->name = base::FilePath::StringType();
    info->data_path = base::FilePath();
    info->modification_time = base::Time::Now();
    info->parent_id = 0;
    return true;
  }
  HandleError(FROM_HERE, status);
  return false;
}

base::File::Error SandboxDirectoryDatabase::AddFileInfo(const FileInfo& info,
                                                        FileId* file_id) {
  if (!Init(REPAIR_ON_CORRUPTION))
    return base::File::FILE_ERROR_FAILED;
  DCHECK(file_id);
  std::string child_key = GetChildLookupKey(info.parent_id, info.name);
  std::string child_id_string;
  leveldb::Status status =
      db_->Get(leveldb::ReadOptions(), child_key, &child_id_string);
  if (status.ok()) {
    LOG(ERROR) << "File exists already!";
    return base::File::FILE_ERROR_EXISTS;
  }
  if (!status.IsNotFound()) {
    HandleError(FROM_HERE, status);
    return base::File::FILE_ERROR_NOT_FOUND;
  }

  if (!IsDirectory(info.parent_id)) {
    LOG(ERROR) << "New parent directory is a file!";
    return base::File::FILE_ERROR_NOT_A_DIRECTORY;
  }

  // This would be a fine place to limit the number of files in a directory, if
  // we decide to add that restriction.

  FileId temp_id;
  if (!GetLastFileId(&temp_id))
    return base::File::FILE_ERROR_FAILED;
  ++temp_id;

  leveldb::WriteBatch batch;
  if (!AddFileInfoHelper(info, temp_id, &batch))
    return base::File::FILE_ERROR_FAILED;

  batch.Put(LastFileIdKey(), base::NumberToString(temp_id));
  status = db_->Write(leveldb::WriteOptions(), &batch);
  if (!status.ok()) {
    HandleError(FROM_HERE, status);
    return base::File::FILE_ERROR_FAILED;
  }
  *file_id = temp_id;
  return base::File::FILE_OK;
}

bool SandboxDirectoryDatabase::RemoveFileInfo(FileId file_id) {
  if (!Init(REPAIR_ON_CORRUPTION))
    return false;
  leveldb::WriteBatch batch;
  if (!RemoveFileInfoHelper(file_id, &batch))
    return false;
  leveldb::Status status = db_->Write(leveldb::WriteOptions(), &batch);
  if (!status.ok()) {
    HandleError(FROM_HERE, status);
    return false;
  }
  return true;
}

bool SandboxDirectoryDatabase::UpdateFileInfo(FileId file_id,
                                              const FileInfo& new_info) {
  // TODO(ericu): We should also check to see that this doesn't create a loop,
  // but perhaps only in a debug build.
  if (!Init(REPAIR_ON_CORRUPTION))
    return false;
  DCHECK(file_id);  // You can't remove the root, ever.  Just delete the DB.
  FileInfo old_info;
  if (!GetFileInfo(file_id, &old_info))
    return false;
  if (old_info.parent_id != new_info.parent_id &&
      !IsDirectory(new_info.parent_id))
    return false;
  if (old_info.parent_id != new_info.parent_id ||
      old_info.name != new_info.name) {
    // Check for name clashes.
    FileId temp_id;
    if (GetChildWithName(new_info.parent_id, new_info.name, &temp_id)) {
      LOG(ERROR) << "Name collision on move.";
      return false;
    }
  }
  leveldb::WriteBatch batch;
  if (!RemoveFileInfoHelper(file_id, &batch) ||
      !AddFileInfoHelper(new_info, file_id, &batch))
    return false;
  leveldb::Status status = db_->Write(leveldb::WriteOptions(), &batch);
  if (!status.ok()) {
    HandleError(FROM_HERE, status);
    return false;
  }
  return true;
}

bool SandboxDirectoryDatabase::UpdateModificationTime(
    FileId file_id,
    const base::Time& modification_time) {
  FileInfo info;
  if (!GetFileInfo(file_id, &info))
    return false;
  info.modification_time = modification_time;
  base::Pickle pickle;
  PickleFromFileInfo(info, &pickle);
  leveldb::Status status =
      db_->Put(leveldb::WriteOptions(), GetFileLookupKey(file_id),
               leveldb::Slice(reinterpret_cast<const char*>(pickle.data()),
                              pickle.size()));
  if (!status.ok()) {
    HandleError(FROM_HERE, status);
    return false;
  }
  return true;
}

bool SandboxDirectoryDatabase::OverwritingMoveFile(FileId src_file_id,
                                                   FileId dest_file_id) {
  FileInfo src_file_info;
  FileInfo dest_file_info;

  if (!GetFileInfo(src_file_id, &src_file_info))
    return false;
  if (!GetFileInfo(dest_file_id, &dest_file_info))
    return false;
  if (src_file_info.is_directory() || dest_file_info.is_directory())
    return false;
  leveldb::WriteBatch batch;
  // This is the only field that really gets moved over; if you add fields to
  // FileInfo, e.g. ctime, they might need to be copied here.
  dest_file_info.data_path = src_file_info.data_path;
  if (!RemoveFileInfoHelper(src_file_id, &batch))
    return false;
  base::Pickle pickle;
  PickleFromFileInfo(dest_file_info, &pickle);
  batch.Put(GetFileLookupKey(dest_file_id),
            leveldb::Slice(reinterpret_cast<const char*>(pickle.data()),
                           pickle.size()));
  leveldb::Status status = db_->Write(leveldb::WriteOptions(), &batch);
  if (!status.ok()) {
    HandleError(FROM_HERE, status);
    return false;
  }
  return true;
}

bool SandboxDirectoryDatabase::GetNextInteger(int64_t* next) {
  if (!Init(REPAIR_ON_CORRUPTION))
    return false;
  DCHECK(next);
  std::string int_string;
  leveldb::Status status =
      db_->Get(leveldb::ReadOptions(), LastIntegerKey(), &int_string);
  if (status.ok()) {
    int64_t temp;
    if (!base::StringToInt64(int_string, &temp)) {
      LOG(ERROR) << "Hit database corruption!";
      return false;
    }
    ++temp;
    status = db_->Put(leveldb::WriteOptions(), LastIntegerKey(),
                      base::NumberToString(temp));
    if (!status.ok()) {
      HandleError(FROM_HERE, status);
      return false;
    }
    *next = temp;
    return true;
  }
  if (!status.IsNotFound()) {
    HandleError(FROM_HERE, status);
    return false;
  }
  // The database must not yet exist; initialize it.
  if (!StoreDefaultValues())
    return false;

  return GetNextInteger(next);
}

bool SandboxDirectoryDatabase::DestroyDatabase() {
  db_.reset();
  const std::string path = FilePathToString(
      filesystem_data_directory_.Append(kDirectoryDatabaseName));
  leveldb_env::Options options;
  if (env_override_)
    options.env = env_override_;
  leveldb::Status status = leveldb::DestroyDB(path, options);
  if (status.ok())
    return true;
  LOG(WARNING) << "Failed to destroy a database with status "
               << status.ToString();
  return false;
}

bool SandboxDirectoryDatabase::Init(RecoveryOption recovery_option) {
  if (db_)
    return true;

  std::string path = FilePathToString(
      filesystem_data_directory_.Append(kDirectoryDatabaseName));
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
      LOG(WARNING) << "Corrupted SandboxDirectoryDatabase detected."
                   << " Attempting to repair.";
      if (RepairDatabase(path)) {
        return true;
      }
      LOG(WARNING) << "Failed to repair SandboxDirectoryDatabase.";
      [[fallthrough]];
    case DELETE_ON_CORRUPTION:
      LOG(WARNING) << "Clearing SandboxDirectoryDatabase.";
      if (!leveldb_chrome::DeleteDB(filesystem_data_directory_, options).ok())
        return false;
      if (!base::CreateDirectory(filesystem_data_directory_))
        return false;
      return Init(FAIL_ON_CORRUPTION);
  }

  NOTREACHED();
}

bool SandboxDirectoryDatabase::RepairDatabase(const std::string& db_path) {
  DCHECK(!db_.get());
  leveldb_env::Options options;
  options.reuse_logs = false;
  options.max_open_files = 0;  // Use minimum.
  if (env_override_)
    options.env = env_override_;
  if (!leveldb::RepairDB(db_path, options).ok())
    return false;
  if (!Init(FAIL_ON_CORRUPTION))
    return false;
  if (IsFileSystemConsistent())
    return true;
  db_.reset();
  return false;
}

bool SandboxDirectoryDatabase::IsDirectory(FileId file_id) {
  FileInfo info;
  if (!file_id)
    return true;  // The root is a directory.
  if (!GetFileInfo(file_id, &info))
    return false;
  if (!info.is_directory())
    return false;
  return true;
}

bool SandboxDirectoryDatabase::IsFileSystemConsistent() {
  if (!Init(FAIL_ON_CORRUPTION))
    return false;
  DatabaseCheckHelper helper(this, db_.get(), filesystem_data_directory_);
  return helper.IsFileSystemConsistent();
}

void SandboxDirectoryDatabase::ReportInitStatus(const leveldb::Status& status) {
  base::Time now = base::Time::Now();
  const base::TimeDelta minimum_interval =
      base::Hours(kSandboxDirectoryMinimumReportIntervalHours);
  if (last_reported_time_ + minimum_interval >= now)
    return;
  last_reported_time_ = now;

  if (status.ok()) {
    UMA_HISTOGRAM_ENUMERATION(kSandboxDirectoryInitStatusHistogramLabel,
                              SandboxDirectoryInitStatus::INIT_STATUS_OK,
                              SandboxDirectoryInitStatus::INIT_STATUS_MAX);
  } else if (status.IsCorruption()) {
    UMA_HISTOGRAM_ENUMERATION(
        kSandboxDirectoryInitStatusHistogramLabel,
        SandboxDirectoryInitStatus::INIT_STATUS_CORRUPTION,
        SandboxDirectoryInitStatus::INIT_STATUS_MAX);
  } else if (status.IsIOError()) {
    UMA_HISTOGRAM_ENUMERATION(kSandboxDirectoryInitStatusHistogramLabel,
                              SandboxDirectoryInitStatus::INIT_STATUS_IO_ERROR,
                              SandboxDirectoryInitStatus::INIT_STATUS_MAX);
  } else {
    UMA_HISTOGRAM_ENUMERATION(
        kSandboxDirectoryInitStatusHistogramLabel,
        SandboxDirectoryInitStatus::INIT_STATUS_UNKNOWN_ERROR,
        SandboxDirectoryInitStatus::INIT_STATUS_MAX);
  }
}

bool SandboxDirectoryDatabase::StoreDefaultValues() {
  // Verify that this is a totally new database, and initialize it.
  {
    // Scope the iterator to ensure deleted before database is closed.
    std::unique_ptr<leveldb::Iterator> iter(
        db_->NewIterator(leveldb::ReadOptions()));
    iter->SeekToFirst();
    if (iter->Valid()) {  // DB was not empty--we shouldn't have been called.
      LOG(ERROR) << "File system origin database is corrupt!";
      return false;
    }
  }
  // This is always the first write into the database.  If we ever add a
  // version number, it should go in this transaction too.
  FileInfo root;
  root.parent_id = 0;
  root.modification_time = base::Time::Now();
  leveldb::WriteBatch batch;
  if (!AddFileInfoHelper(root, 0, &batch))
    return false;
  batch.Put(LastFileIdKey(), base::NumberToString(0));
  batch.Put(LastIntegerKey(), base::NumberToString(-1));
  leveldb::Status status = db_->Write(leveldb::WriteOptions(), &batch);
  if (!status.ok()) {
    HandleError(FROM_HERE, status);
    return false;
  }
  return true;
}

bool SandboxDirectoryDatabase::GetLastFileId(FileId* file_id) {
  if (!Init(REPAIR_ON_CORRUPTION))
    return false;
  DCHECK(file_id);
  std::string id_string;
  leveldb::Status status =
      db_->Get(leveldb::ReadOptions(), LastFileIdKey(), &id_string);
  if (status.ok()) {
    if (!base::StringToInt64(id_string, file_id)) {
      LOG(ERROR) << "Hit database corruption!";
      return false;
    }
    return true;
  }
  if (!status.IsNotFound()) {
    HandleError(FROM_HERE, status);
    return false;
  }
  // The database must not yet exist; initialize it.
  if (!StoreDefaultValues())
    return false;
  *file_id = 0;
  return true;
}

// This does very few safety checks!
bool SandboxDirectoryDatabase::AddFileInfoHelper(const FileInfo& info,
                                                 FileId file_id,
                                                 leveldb::WriteBatch* batch) {
  if (!VerifyDataPath(info.data_path)) {
    LOG(ERROR) << "Invalid data path is given: " << info.data_path.value();
    return false;
  }
  std::string id_string = GetFileLookupKey(file_id);
  if (!file_id) {
    // The root directory doesn't need to be looked up by path from its parent.
    DCHECK(!info.parent_id);
    DCHECK(info.data_path.empty());
  } else {
    std::string child_key = GetChildLookupKey(info.parent_id, info.name);
    batch->Put(child_key, id_string);
  }
  base::Pickle pickle;
  PickleFromFileInfo(info, &pickle);
  batch->Put(id_string,
             leveldb::Slice(reinterpret_cast<const char*>(pickle.data()),
                            pickle.size()));
  return true;
}

// This does very few safety checks!
bool SandboxDirectoryDatabase::RemoveFileInfoHelper(
    FileId file_id,
    leveldb::WriteBatch* batch) {
  DCHECK(file_id);  // You can't remove the root, ever.  Just delete the DB.
  FileInfo info;
  if (!GetFileInfo(file_id, &info))
    return false;
  if (info.data_path.empty()) {  // It's a directory
    std::vector<FileId> children;
    // TODO(ericu): Make a faster is-the-directory-empty check.
    if (!ListChildren(file_id, &children))
      return false;
    if (children.size()) {
      return false;
    }
  }
  batch->Delete(GetChildLookupKey(info.parent_id, info.name));
  batch->Delete(GetFileLookupKey(file_id));
  return true;
}

void SandboxDirectoryDatabase::HandleError(const base::Location& from_here,
                                           const leveldb::Status& status) {
  LOG(ERROR) << "SandboxDirectoryDatabase failed at: " << from_here.ToString()
             << " with error: " << status.ToString();
  db_.reset();
}

}  // namespace storage
