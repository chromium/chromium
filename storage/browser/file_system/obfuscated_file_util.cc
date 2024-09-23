// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/obfuscated_file_util.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <tuple>
#include <utility>

#include "base/containers/queue.h"
#include "base/files/file_error_or.h"
#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/types/expected_macros.h"
#include "components/services/storage/public/cpp/buckets/bucket_init_params.h"
#include "storage/browser/file_system/file_observers.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation_context.h"
#include "storage/browser/file_system/file_system_util.h"
#include "storage/browser/file_system/obfuscated_file_util_disk_delegate.h"
#include "storage/browser/file_system/obfuscated_file_util_memory_delegate.h"
#include "storage/browser/file_system/quota/quota_limit_type.h"
#include "storage/browser/file_system/sandbox_file_system_backend.h"
#include "storage/browser/file_system/sandbox_origin_database.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/common/database/database_identifier.h"
#include "storage/common/file_system/file_system_types.h"
#include "storage/common/file_system/file_system_util.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/leveldatabase/leveldb_chrome.h"
#include "url/gurl.h"

// Example of various paths:
//   void ObfuscatedFileUtil::DoSomething(const FileSystemURL& url) {
//     base::FilePath virtual_path = url.path();
//     base::FilePath local_path = GetLocalFilePath(url);
//
//     NativeFileUtil::DoSomething(local_path);
//     file_util::DoAnother(local_path);
//  }

namespace storage {

namespace {

using FileId = SandboxDirectoryDatabase::FileId;
using FileInfo = SandboxDirectoryDatabase::FileInfo;

void InitFileInfo(SandboxDirectoryDatabase::FileInfo* file_info,
                  SandboxDirectoryDatabase::FileId parent_id,
                  const base::FilePath::StringType& file_name) {
  DCHECK(file_info);
  file_info->parent_id = parent_id;
  file_info->name = file_name;
}

// Costs computed as per crbug.com/86114, based on the LevelDB implementation of
// path storage under Linux.  It's not clear if that will differ on Windows, on
// which base::FilePath uses wide chars [since they're converted to UTF-8 for
// storage anyway], but as long as the cost is high enough that one can't cheat
// on quota by storing data in paths, it doesn't need to be all that accurate.
const int64_t kPathCreationQuotaCost = 146;  // Bytes per inode, basically.
const int64_t kPathByteQuotaCost =
    2;  // Bytes per byte of path length in UTF-8.

int64_t UsageForPath(size_t length) {
  return kPathCreationQuotaCost +
         static_cast<int64_t>(length) * kPathByteQuotaCost;
}

bool AllocateQuota(FileSystemOperationContext* context, int64_t growth) {
  if (context->allowed_bytes_growth() == QuotaManager::kNoLimit)
    return true;

  int64_t new_quota = context->allowed_bytes_growth() - growth;
  if (growth > 0 && new_quota < 0)
    return false;
  context->set_allowed_bytes_growth(new_quota);
  return true;
}

void UpdateUsage(FileSystemOperationContext* context,
                 const FileSystemURL& url,
                 int64_t growth) {
  context->update_observers()->Notify(&FileUpdateObserver::OnUpdate, url,
                                      growth);
}

void TouchDirectory(SandboxDirectoryDatabase* db, FileId dir_id) {
  DCHECK(db);
  if (!db->UpdateModificationTime(dir_id, base::Time::Now()))
    NOTREACHED();
}

enum IsolatedOriginStatus {
  kIsolatedOriginMatch,
  kIsolatedOriginDontMatch,
  kIsolatedOriginStatusMax,
};

}  // namespace

// Implementing the DatabaseKey for the directories_ map.
DatabaseKey::DatabaseKey() = default;
DatabaseKey::~DatabaseKey() = default;

// Copyable and moveable
DatabaseKey::DatabaseKey(const DatabaseKey& other) = default;
DatabaseKey& DatabaseKey::operator=(const DatabaseKey& other) = default;
DatabaseKey::DatabaseKey(DatabaseKey&& other) = default;
DatabaseKey& DatabaseKey::operator=(DatabaseKey&& other) = default;

DatabaseKey::DatabaseKey(const blink::StorageKey& storage_key,
                         const std::optional<BucketLocator>& bucket,
                         const std::string& type) {
  storage_key_ = storage_key;
  bucket_ = bucket;
  type_ = type;
}

bool DatabaseKey::operator==(const DatabaseKey& other) const {
  return std::tie(storage_key_, bucket_, type_) ==
         std::tie(other.storage_key_, other.bucket_, other.type_);
}

bool DatabaseKey::operator!=(const DatabaseKey& other) const {
  return std::tie(storage_key_, bucket_, type_) !=
         std::tie(other.storage_key_, other.bucket_, other.type_);
}

bool DatabaseKey::operator<(const DatabaseKey& other) const {
  return std::tie(storage_key_, bucket_, type_) <
         std::tie(other.storage_key_, other.bucket_, other.type_);
}

// end DatabaseKey implementation.

class ObfuscatedFileEnumerator final
    : public FileSystemFileUtil::AbstractFileEnumerator {
 public:
  ObfuscatedFileEnumerator(SandboxDirectoryDatabase* db,
                           FileSystemOperationContext* context,
                           ObfuscatedFileUtil* obfuscated_file_util,
                           const FileSystemURL& root_url,
                           bool recursive)
      : db_(db),
        context_(context),
        obfuscated_file_util_(obfuscated_file_util),
        root_url_(root_url),
        recursive_(recursive),
        current_file_id_(0) {
    base::FilePath root_virtual_path = root_url.path();
    FileId file_id;

    if (!db_->GetFileWithPath(root_virtual_path, &file_id))
      return;

    FileRecord record = {file_id, root_virtual_path};
    recurse_queue_.push(record);
  }

  ~ObfuscatedFileEnumerator() override = default;

  base::FilePath Next() override {
    FileInfo file_info;
    base::File::Error error;
    do {
      ProcessRecurseQueue();
      if (display_stack_.empty())
        return base::FilePath();

      current_file_id_ = display_stack_.back();
      display_stack_.pop_back();

      base::FilePath platform_file_path;
      error = obfuscated_file_util_->GetFileInfoInternal(
          db_, context_, root_url_, current_file_id_, &file_info,
          &current_platform_file_info_, &platform_file_path);
    } while (error != base::File::FILE_OK);

    base::FilePath virtual_path =
        current_parent_virtual_path_.Append(file_info.name);
    if (recursive_ && file_info.is_directory()) {
      FileRecord record = {current_file_id_, virtual_path};
      recurse_queue_.push(record);
    }
    return virtual_path;
  }

  int64_t Size() override { return current_platform_file_info_.size; }

  base::FilePath GetName() override { return base::FilePath(); }

  base::Time LastModifiedTime() override {
    return current_platform_file_info_.last_modified;
  }

  bool IsDirectory() override {
    return current_platform_file_info_.is_directory;
  }

 private:
  using FileId = SandboxDirectoryDatabase::FileId;
  using FileInfo = SandboxDirectoryDatabase::FileInfo;

  struct FileRecord {
    FileId file_id;
    base::FilePath virtual_path;
  };

  void ProcessRecurseQueue() {
    while (display_stack_.empty() && !recurse_queue_.empty()) {
      FileRecord entry = recurse_queue_.front();
      recurse_queue_.pop();
      if (!db_->ListChildren(entry.file_id, &display_stack_)) {
        display_stack_.clear();
        return;
      }
      current_parent_virtual_path_ = entry.virtual_path;
    }
  }

  raw_ptr<SandboxDirectoryDatabase> db_;
  raw_ptr<FileSystemOperationContext, DanglingUntriaged> context_;
  raw_ptr<ObfuscatedFileUtil> obfuscated_file_util_;
  FileSystemURL root_url_;
  bool recursive_;

  base::queue<FileRecord> recurse_queue_;
  std::vector<FileId> display_stack_;
  base::FilePath current_parent_virtual_path_;

  FileId current_file_id_;
  base::File::Info current_platform_file_info_;
};

// NOTE: currently, ObfuscatedFileUtil still relies on SandboxOriginDatabases
// for first-party StorageKeys/default buckets. The AbstractStorageKeyEnumerator
// class is only used in these cases. While this class stores and iterates
// through StorageKeys types, it works by retrieving OriginRecords from the
// SandboxOriginDatabase and converting those origins into first-party
// StorageKey values (e.g. blink::StorageKey(origin)). The goal is to eventually
// deprecate SandboxOriginDatabases and AbstractStorageKeyEnumerators and rely
// entirely on Storage Buckets.
class ObfuscatedStorageKeyEnumerator
    : public ObfuscatedFileUtil::AbstractStorageKeyEnumerator {
 public:
  using OriginRecord = SandboxOriginDatabase::OriginRecord;
  ObfuscatedStorageKeyEnumerator(
      SandboxOriginDatabaseInterface* origin_database,
      base::WeakPtr<ObfuscatedFileUtilMemoryDelegate> memory_file_util,
      const base::FilePath& base_file_path)
      : base_file_path_(base_file_path),
        memory_file_util_(std::move(memory_file_util)) {
    if (origin_database)
      origin_database->ListAllOrigins(&origin_records_);
  }

  ~ObfuscatedStorageKeyEnumerator() override = default;

  // Returns the next StorageKey. Returns empty if there are no more
  // StorageKeys.
  std::optional<blink::StorageKey> Next() override {
    OriginRecord record;
    if (origin_records_.empty()) {
      current_ = record;
      return std::nullopt;
    }
    record = origin_records_.back();
    origin_records_.pop_back();
    current_ = record;
    return blink::StorageKey::CreateFirstParty(
        GetOriginFromIdentifier(record.origin));
  }

  // Returns the current StorageKey.origin()'s information.
  bool HasTypeDirectory(const std::string& type_string) const override {
    if (current_.path.empty())
      return false;
    if (type_string.empty()) {
      NOTREACHED();
    }
    base::FilePath path =
        base_file_path_.Append(current_.path).AppendASCII(type_string);
    if (memory_file_util_)
      return memory_file_util_->DirectoryExists(path);
    else
      return base::DirectoryExists(path);
  }

 private:
  std::vector<OriginRecord> origin_records_;
  OriginRecord current_;
  base::FilePath base_file_path_;
  base::WeakPtr<ObfuscatedFileUtilMemoryDelegate> memory_file_util_;
};

const base::FilePath::CharType ObfuscatedFileUtil::kFileSystemDirectory[] =
    FILE_PATH_LITERAL("File System");

ObfuscatedFileUtil::ObfuscatedFileUtil(
    scoped_refptr<SpecialStoragePolicy> special_storage_policy,
    const base::FilePath& profile_path,
    leveldb::Env* env_override,
    const std::set<std::string>& known_type_strings,
    SandboxFileSystemBackendDelegate* sandbox_delegate,
    bool is_incognito)
    : special_storage_policy_(std::move(special_storage_policy)),
      env_override_(env_override),
      is_incognito_(is_incognito),
      db_flush_delay_seconds_(10 * 60),  // 10 mins.
      known_type_strings_(known_type_strings),
      sandbox_delegate_(sandbox_delegate) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  DCHECK(!is_incognito_ ||
         (env_override && leveldb_chrome::IsMemEnv(env_override)));
  file_system_directory_ = profile_path.Append(kFileSystemDirectory);

  if (is_incognito_) {
    // profile_path is passed here, so that the delegate is able to accommodate
    // both codepaths of {{profile_path}}/File System (first-party) and
    // {{profile_path}}/WebStorage (buckets-based).
    // See https://crrev.com/c/3817542 for more context.
    delegate_ =
        std::make_unique<ObfuscatedFileUtilMemoryDelegate>(profile_path);
  } else {
    delegate_ = std::make_unique<ObfuscatedFileUtilDiskDelegate>();
  }
}

ObfuscatedFileUtil::~ObfuscatedFileUtil() {
  DropDatabases();
}

base::File ObfuscatedFileUtil::CreateOrOpen(FileSystemOperationContext* context,
                                            const FileSystemURL& url,
                                            int file_flags) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::File file = CreateOrOpenInternal(context, url, file_flags);
  if (file.IsValid() && file_flags & base::File::FLAG_WRITE &&
      context->quota_limit_type() == QuotaLimitType::kUnlimited &&
      sandbox_delegate_) {
    sandbox_delegate_->StickyInvalidateUsageCache(url.storage_key(),
                                                  url.type());
  }
  return file;
}

base::File::Error ObfuscatedFileUtil::EnsureFileExists(
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    bool* created) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  SandboxDirectoryDatabase* db = GetDirectoryDatabase(url, true);
  if (!db)
    return base::File::FILE_ERROR_FAILED;

  FileId file_id;
  if (db->GetFileWithPath(url.path(), &file_id)) {
    FileInfo file_info;
    if (!db->GetFileInfo(file_id, &file_info)) {
      return base::File::FILE_ERROR_FAILED;
    }
    if (file_info.is_directory())
      return base::File::FILE_ERROR_NOT_A_FILE;
    if (created)
      *created = false;
    return base::File::FILE_OK;
  }
  FileId parent_id;
  if (!db->GetFileWithPath(VirtualPath::DirName(url.path()), &parent_id))
    return base::File::FILE_ERROR_NOT_FOUND;

  FileInfo file_info;
  InitFileInfo(&file_info, parent_id,
               VirtualPath::BaseName(url.path()).value());

  int64_t growth = UsageForPath(file_info.name.size());
  if (!AllocateQuota(context, growth))
    return base::File::FILE_ERROR_NO_SPACE;
  base::File::Error error = CreateFile(
      context, base::FilePath(), false /* foreign_source */, url, &file_info);
  if (created && base::File::FILE_OK == error) {
    *created = true;
    UpdateUsage(context, url, growth);
    context->change_observers()->Notify(&FileChangeObserver::OnCreateFile, url);
  }
  return error;
}

base::File::Error ObfuscatedFileUtil::CreateDirectory(
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    bool exclusive,
    bool recursive) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  SandboxDirectoryDatabase* db = GetDirectoryDatabase(url, true);
  if (!db)
    return base::File::FILE_ERROR_FAILED;

  FileId file_id;
  if (db->GetFileWithPath(url.path(), &file_id)) {
    FileInfo file_info;
    if (exclusive)
      return base::File::FILE_ERROR_EXISTS;
    if (!db->GetFileInfo(file_id, &file_info)) {
      return base::File::FILE_ERROR_FAILED;
    }
    if (!file_info.is_directory())
      return base::File::FILE_ERROR_NOT_A_DIRECTORY;
    return base::File::FILE_OK;
  }

  std::vector<base::FilePath::StringType> components =
      VirtualPath::GetComponents(url.path());
  FileId parent_id = 0;
  size_t index;
  for (index = 0; index < components.size(); ++index) {
    base::FilePath::StringType name = components[index];
    if (name == FILE_PATH_LITERAL("/"))
      continue;
    if (!db->GetChildWithName(parent_id, name, &parent_id))
      break;
  }
  if (!db->IsDirectory(parent_id))
    return base::File::FILE_ERROR_NOT_A_DIRECTORY;
  if (!recursive && components.size() - index > 1)
    return base::File::FILE_ERROR_NOT_FOUND;
  bool first = true;
  for (; index < components.size(); ++index) {
    FileInfo file_info;
    file_info.name = components[index];
    if (file_info.name == FILE_PATH_LITERAL("/"))
      continue;
    file_info.modification_time = base::Time::Now();
    file_info.parent_id = parent_id;
    int64_t growth = UsageForPath(file_info.name.size());
    if (!AllocateQuota(context, growth))
      return base::File::FILE_ERROR_NO_SPACE;
    base::File::Error error = db->AddFileInfo(file_info, &parent_id);
    if (error != base::File::FILE_OK)
      return error;
    UpdateUsage(context, url, growth);

    // Appropriately report changes when recursively creating a directory by
    // constructing the FileSystemURL of created intermediate directories.
    base::FilePath changed_path = url.virtual_path();
    for (size_t i = components.size() - 1; i > index; --i) {
      changed_path = VirtualPath::DirName(changed_path);
    }
    auto created_directory_url =
        context->file_system_context()->CreateCrackedFileSystemURL(
            url.storage_key(), url.mount_type(), changed_path);
    if (url.bucket().has_value()) {
      created_directory_url.SetBucket(url.bucket().value());
    }
    context->change_observers()->Notify(&FileChangeObserver::OnCreateDirectory,
                                        created_directory_url);
    if (first) {
      first = false;
      TouchDirectory(db, file_info.parent_id);
    }
  }
  return base::File::FILE_OK;
}

base::File::Error ObfuscatedFileUtil::GetFileInfo(
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    base::File::Info* file_info,
    base::FilePath* platform_file_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SandboxDirectoryDatabase* db = GetDirectoryDatabase(url, false);
  if (!db)
    return base::File::FILE_ERROR_NOT_FOUND;
  FileId file_id;
  if (!db->GetFileWithPath(url.path(), &file_id))
    return base::File::FILE_ERROR_NOT_FOUND;
  FileInfo local_info;
  return GetFileInfoInternal(db, context, url, file_id, &local_info, file_info,
                             platform_file_path);
}

base::File::Error ObfuscatedFileUtil::GetLocalFilePath(
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    base::FilePath* local_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SandboxDirectoryDatabase* db = GetDirectoryDatabase(url, false);
  if (!db)
    return base::File::FILE_ERROR_NOT_FOUND;
  FileId file_id;
  if (!db->GetFileWithPath(url.path(), &file_id))
    return base::File::FILE_ERROR_NOT_FOUND;
  FileInfo file_info;
  if (!db->GetFileInfo(file_id, &file_info) || file_info.is_directory()) {
    // Directories have no local file path.
    return base::File::FILE_ERROR_NOT_FOUND;
  }
  *local_path = DataPathToLocalPath(url, file_info.data_path);

  if (local_path->empty())
    return base::File::FILE_ERROR_NOT_FOUND;
  return base::File::FILE_OK;
}

base::File::Error ObfuscatedFileUtil::Touch(
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    const base::Time& last_access_time,
    const base::Time& last_modified_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SandboxDirectoryDatabase* db = GetDirectoryDatabase(url, false);
  if (!db)
    return base::File::FILE_ERROR_NOT_FOUND;
  FileId file_id;
  if (!db->GetFileWithPath(url.path(), &file_id))
    return base::File::FILE_ERROR_NOT_FOUND;

  FileInfo file_info;
  if (!db->GetFileInfo(file_id, &file_info)) {
    return base::File::FILE_ERROR_FAILED;
  }
  if (file_info.is_directory()) {
    if (!db->UpdateModificationTime(file_id, last_modified_time))
      return base::File::FILE_ERROR_FAILED;
    return base::File::FILE_OK;
  }
  return delegate_->Touch(DataPathToLocalPath(url, file_info.data_path),
                          last_access_time, last_modified_time);
}

base::File::Error ObfuscatedFileUtil::Truncate(
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    int64_t length) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::File::Info file_info;
  base::FilePath local_path;
  base::File::Error error = GetFileInfo(context, url, &file_info, &local_path);
  if (error != base::File::FILE_OK)
    return error;

  int64_t growth = length - file_info.size;
  if (!AllocateQuota(context, growth))
    return base::File::FILE_ERROR_NO_SPACE;
  error = delegate_->Truncate(local_path, length);
  if (error == base::File::FILE_OK) {
    UpdateUsage(context, url, growth);
    context->change_observers()->Notify(&FileChangeObserver::OnModifyFile, url);
  }
  return error;
}

base::File::Error ObfuscatedFileUtil::CopyOrMoveFile(
    FileSystemOperationContext* context,
    const FileSystemURL& src_url,
    const FileSystemURL& dest_url,
    CopyOrMoveOptionSet options,
    bool copy) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Cross-filesystem copies and moves should be handled via CopyInForeignFile.
  DCHECK(src_url.origin() == dest_url.origin());
  DCHECK(src_url.type() == dest_url.type());

  SandboxDirectoryDatabase* db = GetDirectoryDatabase(src_url, true);
  if (!db)
    return base::File::FILE_ERROR_FAILED;

  FileId src_file_id;
  if (!db->GetFileWithPath(src_url.path(), &src_file_id))
    return base::File::FILE_ERROR_NOT_FOUND;

  FileId dest_file_id;
  bool overwrite = db->GetFileWithPath(dest_url.path(), &dest_file_id);

  FileInfo src_file_info;
  base::File::Info src_platform_file_info;
  base::FilePath src_local_path;
  base::File::Error error =
      GetFileInfoInternal(db, context, src_url, src_file_id, &src_file_info,
                          &src_platform_file_info, &src_local_path);
  if (error != base::File::FILE_OK)
    return error;
  if (src_file_info.is_directory())
    return base::File::FILE_ERROR_NOT_A_FILE;

  FileInfo dest_file_info;
  base::File::Info dest_platform_file_info;  // overwrite case only
  base::FilePath dest_local_path;            // overwrite case only
  if (overwrite) {
    error = GetFileInfoInternal(db, context, dest_url, dest_file_id,
                                &dest_file_info, &dest_platform_file_info,
                                &dest_local_path);
    if (error == base::File::FILE_ERROR_NOT_FOUND)
      overwrite = false;  // fallback to non-overwrite case
    else if (error != base::File::FILE_OK)
      return error;
    else if (dest_file_info.is_directory())
      return base::File::FILE_ERROR_INVALID_OPERATION;
  }
  if (!overwrite) {
    FileId dest_parent_id;
    if (!db->GetFileWithPath(VirtualPath::DirName(dest_url.path()),
                             &dest_parent_id)) {
      return base::File::FILE_ERROR_NOT_FOUND;
    }

    dest_file_info = src_file_info;
    dest_file_info.parent_id = dest_parent_id;
    dest_file_info.name = VirtualPath::BaseName(dest_url.path()).value();
  }

  int64_t growth = 0;
  if (copy)
    growth += src_platform_file_info.size;
  else
    growth -= UsageForPath(src_file_info.name.size());
  if (overwrite)
    growth -= dest_platform_file_info.size;
  else
    growth += UsageForPath(dest_file_info.name.size());
  if (!AllocateQuota(context, growth))
    return base::File::FILE_ERROR_NO_SPACE;

  /*
   * Copy-with-overwrite
   *  Just overwrite data file
   * Copy-without-overwrite
   *  Copy backing file
   *  Create new metadata pointing to new backing file.
   * Move-with-overwrite
   *  transaction:
   *    Remove source entry.
   *    Point target entry to source entry's backing file.
   *  Delete target entry's old backing file
   * Move-without-overwrite
   *  Just update metadata
   */
  error = base::File::FILE_ERROR_FAILED;
  if (copy) {
    if (overwrite) {
      error = delegate_->CopyOrMoveFile(
          src_local_path, dest_local_path, options,
          delegate_->CopyOrMoveModeForDestination(dest_url, true /* copy */));
    } else {  // non-overwrite
      error = CreateFile(context, src_local_path, false /* foreign_source */,
                         dest_url, &dest_file_info);
    }
  } else {
    if (overwrite) {
      if (db->OverwritingMoveFile(src_file_id, dest_file_id)) {
        if (base::File::FILE_OK != delegate_->DeleteFile(dest_local_path))
          LOG(WARNING) << "Leaked a backing file.";
        error = base::File::FILE_OK;
      } else {
        error = base::File::FILE_ERROR_FAILED;
      }
    } else {  // non-overwrite
      if (db->UpdateFileInfo(src_file_id, dest_file_info))
        error = base::File::FILE_OK;
      else
        error = base::File::FILE_ERROR_FAILED;
    }
  }

  if (error != base::File::FILE_OK)
    return error;

  if (copy) {
    context->change_observers()->Notify(&FileChangeObserver::OnCreateFileFrom,
                                        dest_url, src_url);
  } else {
    context->change_observers()->Notify(&FileChangeObserver::OnMoveFileFrom,
                                        dest_url, src_url);
    TouchDirectory(db, src_file_info.parent_id);
  }

  TouchDirectory(db, dest_file_info.parent_id);

  UpdateUsage(context, dest_url, growth);
  return error;
}

base::File::Error ObfuscatedFileUtil::CopyInForeignFile(
    FileSystemOperationContext* context,
    const base::FilePath& src_file_path,
    const FileSystemURL& dest_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SandboxDirectoryDatabase* db = GetDirectoryDatabase(dest_url, true);
  if (!db)
    return base::File::FILE_ERROR_FAILED;

  base::File::Info src_platform_file_info;
  // Foreign files are from another on-disk file system and don't require path
  // conversion.
  if (ObfuscatedFileUtilDiskDelegate().GetFileInfo(
          src_file_path, &src_platform_file_info) != base::File::FILE_OK)
    return base::File::FILE_ERROR_NOT_FOUND;

  FileId dest_file_id;
  bool overwrite = db->GetFileWithPath(dest_url.path(), &dest_file_id);

  FileInfo dest_file_info;
  base::File::Info dest_platform_file_info;  // overwrite case only
  if (overwrite) {
    base::FilePath dest_local_path;
    base::File::Error error = GetFileInfoInternal(
        db, context, dest_url, dest_file_id, &dest_file_info,
        &dest_platform_file_info, &dest_local_path);
    if (error == base::File::FILE_ERROR_NOT_FOUND)
      overwrite = false;  // fallback to non-overwrite case
    else if (error != base::File::FILE_OK)
      return error;
    else if (dest_file_info.is_directory())
      return base::File::FILE_ERROR_INVALID_OPERATION;
  }
  if (!overwrite) {
    FileId dest_parent_id;
    if (!db->GetFileWithPath(VirtualPath::DirName(dest_url.path()),
                             &dest_parent_id)) {
      return base::File::FILE_ERROR_NOT_FOUND;
    }
    if (!dest_file_info.is_directory())
      return base::File::FILE_ERROR_FAILED;
    InitFileInfo(&dest_file_info, dest_parent_id,
                 VirtualPath::BaseName(dest_url.path()).value());
  }

  int64_t growth = src_platform_file_info.size;
  if (overwrite)
    growth -= dest_platform_file_info.size;
  else
    growth += UsageForPath(dest_file_info.name.size());
  if (!AllocateQuota(context, growth))
    return base::File::FILE_ERROR_NO_SPACE;

  base::File::Error error;
  if (overwrite) {
    base::FilePath dest_local_path =
        DataPathToLocalPath(dest_url, dest_file_info.data_path);
    error = delegate_->CopyInForeignFile(
        src_file_path, dest_local_path,
        FileSystemOperation::CopyOrMoveOptionSet(),
        delegate_->CopyOrMoveModeForDestination(dest_url, true /* copy */));
  } else {
    error = CreateFile(context, src_file_path, true /* foreign_source */,
                       dest_url, &dest_file_info);
  }

  if (error != base::File::FILE_OK)
    return error;

  if (overwrite) {
    context->change_observers()->Notify(&FileChangeObserver::OnModifyFile,
                                        dest_url);
  } else {
    context->change_observers()->Notify(&FileChangeObserver::OnCreateFile,
                                        dest_url);
  }

  UpdateUsage(context, dest_url, growth);
  TouchDirectory(db, dest_file_info.parent_id);
  return base::File::FILE_OK;
}

base::File::Error ObfuscatedFileUtil::DeleteFile(
    FileSystemOperationContext* context,
    const FileSystemURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SandboxDirectoryDatabase* db = GetDirectoryDatabase(url, true);
  if (!db)
    return base::File::FILE_ERROR_FAILED;
  FileId file_id;
  if (!db->GetFileWithPath(url.path(), &file_id))
    return base::File::FILE_ERROR_NOT_FOUND;

  FileInfo file_info;
  base::File::Info platform_file_info;
  base::FilePath local_path;
  base::File::Error error = GetFileInfoInternal(
      db, context, url, file_id, &file_info, &platform_file_info, &local_path);
  if (error != base::File::FILE_ERROR_NOT_FOUND && error != base::File::FILE_OK)
    return error;

  if (file_info.is_directory())
    return base::File::FILE_ERROR_NOT_A_FILE;

  int64_t growth =
      -UsageForPath(file_info.name.size()) - platform_file_info.size;
  AllocateQuota(context, growth);
  if (!db->RemoveFileInfo(file_id)) {
    DUMP_WILL_BE_NOTREACHED();
    return base::File::FILE_ERROR_FAILED;
  }
  UpdateUsage(context, url, growth);
  TouchDirectory(db, file_info.parent_id);

  context->change_observers()->Notify(&FileChangeObserver::OnRemoveFile, url);

  if (error == base::File::FILE_ERROR_NOT_FOUND)
    return base::File::FILE_OK;

  error = delegate_->DeleteFile(local_path);
  if (base::File::FILE_OK != error)
    LOG(WARNING) << "Leaked a backing file.";
  return base::File::FILE_OK;
}

base::File::Error ObfuscatedFileUtil::DeleteDirectory(
    FileSystemOperationContext* context,
    const FileSystemURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SandboxDirectoryDatabase* db = GetDirectoryDatabase(url, true);
  if (!db)
    return base::File::FILE_ERROR_FAILED;

  FileId file_id;
  if (!db->GetFileWithPath(url.path(), &file_id))
    return base::File::FILE_ERROR_NOT_FOUND;
  if (!file_id) {
    // Attempting to remove the root directory. Delete the whole file system.
    // This code should only be reached by the File System Access API when
    // deleting the root of an Origin Private File System. All sandboxed URLs
    // coming from that API are guaranteed to include bucket information.
    DCHECK(url.bucket().has_value());
    DCHECK(url.type() == kFileSystemTypeTemporary);
    DeleteDirectoryForBucketAndType(url.bucket().value(), url.type());
    context->change_observers()->Notify(&FileChangeObserver::OnRemoveDirectory,
                                        url);
    return base::File::FILE_OK;
  }
  FileInfo file_info;
  if (!db->GetFileInfo(file_id, &file_info)) {
    return base::File::FILE_ERROR_FAILED;
  }
  if (!file_info.is_directory())
    return base::File::FILE_ERROR_NOT_A_DIRECTORY;
  if (!db->RemoveFileInfo(file_id))
    return base::File::FILE_ERROR_NOT_EMPTY;
  int64_t growth = -UsageForPath(file_info.name.size());
  AllocateQuota(context, growth);
  UpdateUsage(context, url, growth);
  TouchDirectory(db, file_info.parent_id);
  context->change_observers()->Notify(&FileChangeObserver::OnRemoveDirectory,
                                      url);
  return base::File::FILE_OK;
}

ScopedFile ObfuscatedFileUtil::CreateSnapshotFile(
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    base::File::Error* error,
    base::File::Info* file_info,
    base::FilePath* platform_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // We're just returning the local file information.
  *error = GetFileInfo(context, url, file_info, platform_path);
  if (*error == base::File::FILE_OK && file_info->is_directory) {
    *file_info = base::File::Info();
    *error = base::File::FILE_ERROR_NOT_A_FILE;
  }
  // An empty ScopedFile does not have any on-disk operation, therefore it can
  // be handled the same way by on-disk and in-memory implementations.
  return ScopedFile();
}

std::unique_ptr<FileSystemFileUtil::AbstractFileEnumerator>
ObfuscatedFileUtil::CreateFileEnumerator(FileSystemOperationContext* context,
                                         const FileSystemURL& root_url,
                                         bool recursive) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SandboxDirectoryDatabase* db = GetDirectoryDatabase(root_url, false);
  if (!db) {
    return std::make_unique<EmptyFileEnumerator>();
  }
  return std::make_unique<ObfuscatedFileEnumerator>(
      db, context, this, root_url, recursive);
}

bool ObfuscatedFileUtil::IsDirectoryEmpty(FileSystemOperationContext* context,
                                          const FileSystemURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SandboxDirectoryDatabase* db = GetDirectoryDatabase(url, false);
  if (!db)
    return true;  // Not a great answer, but it's what others do.
  FileId file_id;
  if (!db->GetFileWithPath(url.path(), &file_id))
    return true;  // Ditto.
  FileInfo file_info;
  if (!db->GetFileInfo(file_id, &file_info)) {
    // It's the root directory and the database hasn't been initialized yet.
    return true;
  }
  if (!file_info.is_directory())
    return true;
  std::vector<FileId> children;
  // TODO(ericu): This could easily be made faster with help from the database.
  if (!db->ListChildren(file_id, &children))
    return true;
  return children.empty();
}

base::FileErrorOr<base::FilePath>
ObfuscatedFileUtil::GetDirectoryForBucketAndType(
    const BucketLocator& bucket,
    const std::optional<FileSystemType>& type,
    bool create) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // A default bucket in a first-party context uses
  // GetDirectoryForStorageKeyAndType() to determine its file path.
  // In tests, `sandbox_delegate_->quota_manager_proxy()` may be null.
  if ((bucket.storage_key.IsFirstPartyContext() && bucket.is_default) ||
      !sandbox_delegate_->quota_manager_proxy()) {
    return GetDirectoryForStorageKeyAndType(bucket.storage_key, type, create);
  }

  // All other contexts use the provided bucket information to construct the
  // file path.
  base::FilePath path =
      sandbox_delegate_->quota_manager_proxy()->GetClientBucketPath(
          bucket, QuotaClientType::kFileSystem);
  // Append the file system type and verify the path is valid.
  if (type) {
    path = path.AppendASCII(
        SandboxFileSystemBackendDelegate::GetTypeString(type.value()));
  }
  base::File::Error error = GetDirectoryHelper(path, create);
  if (error != base::File::FILE_OK)
    return base::unexpected(error);
  return path;
}

base::FileErrorOr<base::FilePath>
ObfuscatedFileUtil::GetDirectoryForStorageKeyAndType(
    const blink::StorageKey& storage_key,
    const std::optional<FileSystemType>& type,
    bool create) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ASSIGN_OR_RETURN(base::FilePath path,
                   GetDirectoryForStorageKey(storage_key, create));
  DCHECK(!path.empty());
  if (!type) {
    return path;
  }
  // Append the file system type and verify the path is valid.
  path = path.AppendASCII(
      SandboxFileSystemBackendDelegate::GetTypeString(type.value()));
  base::File::Error error = GetDirectoryHelper(path, create);
  if (error != base::File::FILE_OK)
    return base::unexpected(error);
  return path;
}

bool ObfuscatedFileUtil::DeleteDirectoryForBucketAndType(
    const BucketLocator& bucket_locator,
    const std::optional<FileSystemType>& type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DestroyDirectoryDatabaseForBucket(bucket_locator, type);

  // Get the base path for the bucket without the type string appended.
  base::FileErrorOr<base::FilePath> path_without_type =
      GetDirectoryForBucketAndType(bucket_locator, /*type=*/std::nullopt,
                                   /*create=*/false);
  if (!path_without_type.has_value() || path_without_type->empty()) {
    return true;
  }

  if (type) {
    // Delete the filesystem type directory.
    ASSIGN_OR_RETURN(
        const base::FilePath path_with_type,
        GetDirectoryForBucketAndType(bucket_locator, type.value(), false),
        [](auto) { return false; });
    if (!path_with_type.empty() && !delegate_->DeleteFileOrDirectory(
                                       path_with_type, true /* recursive */)) {
      return false;
    }

    // At this point we are sure we had successfully deleted the bucket/type
    // directory. Now we need to see if we have other sub-type-directories under
    // the higher-level `path_without_type` directory. If so, we need to return
    // early to avoid deleting the higher-level `path_without_type` directory
    const std::string type_string =
        SandboxFileSystemBackendDelegate::GetTypeString(type.value());
    for (const std::string& known_type : known_type_strings_) {
      if (known_type == type_string) {
        continue;
      }
      if (delegate_->DirectoryExists(
              path_without_type->AppendASCII(known_type))) {
        // Other type's directory exists; return to avoid deleting the higher
        // level directory.
        return true;
      }
    }
  }

  // No other directories seem to exist. If we have a first-party default
  // bucket, try deleting the entire origin directory.
  if (bucket_locator.is_default &&
      bucket_locator.storage_key.IsFirstPartyContext()) {
    InitOriginDatabase(bucket_locator.storage_key.origin(), false);
    if (origin_database_) {
      origin_database_->RemovePathForOrigin(
          GetIdentifierFromOrigin(bucket_locator.storage_key.origin()));
    }
  }
  // Delete the higher-level directory.
  return delegate_->DeleteFileOrDirectory(path_without_type.value(),
                                          true /* recursive */);
}

std::unique_ptr<ObfuscatedFileUtil::AbstractStorageKeyEnumerator>
ObfuscatedFileUtil::CreateStorageKeyEnumerator() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  InitOriginDatabase(url::Origin(), false);
  base::WeakPtr<ObfuscatedFileUtilMemoryDelegate> file_util_delegate;
  if (is_incognito()) {
    file_util_delegate =
        static_cast<ObfuscatedFileUtilMemoryDelegate*>(delegate())
            ->GetWeakPtr();
  }
  return std::make_unique<ObfuscatedStorageKeyEnumerator>(
      origin_database_.get(), file_util_delegate, file_system_directory_);
}

void ObfuscatedFileUtil::DestroyDirectoryDatabaseForBucket(
    const BucketLocator& bucket_locator,
    const std::optional<FileSystemType>& type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DatabaseKey key_prefix;
  const std::string type_string =
      type ? SandboxFileSystemBackendDelegate::GetTypeString(type.value())
           : std::string();
  // `key.bucket()` is std::nullopt for all non-kTemporary types.
  if (type && (FileSystemTypeToQuotaStorageType(type.value()) ==
               ::blink::mojom::StorageType::kTemporary)) {
    key_prefix =
        DatabaseKey(bucket_locator.storage_key, bucket_locator, type_string);
  } else {  // All other storage types.
    key_prefix =
        DatabaseKey(bucket_locator.storage_key, std::nullopt, type_string);
  }

  // If `type` is empty, delete all filesystem types under `storage_key`.
  for (auto iter = directories_.lower_bound(key_prefix);
       iter != directories_.end();) {
    // If the key matches exactly or `type` is std::nullopt and just the
    // StorageKey and BucketLocator match exactly, delete the database.
    if (iter->first == key_prefix ||
        (type == std::nullopt &&
         iter->first.storage_key() == key_prefix.storage_key() &&
         iter->first.bucket() == key_prefix.bucket())) {
      std::unique_ptr<SandboxDirectoryDatabase> database =
          std::move(iter->second);
      directories_.erase(iter++);
      database->DestroyDatabase();
    } else {
      break;
    }
  }
}

// static
int64_t ObfuscatedFileUtil::ComputeFilePathCost(const base::FilePath& path) {
  return UsageForPath(VirtualPath::BaseName(path).value().size());
}

base::FileErrorOr<base::FilePath> ObfuscatedFileUtil::GetDirectoryForURL(
    const FileSystemURL& url,
    bool create) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!url.bucket().has_value()) {
    // Access the SandboxDirectoryDatabase to construct the file path.
    return GetDirectoryForStorageKeyAndType(url.storage_key(), url.type(),
                                            create);
  }
  // Construct the file path using the provided bucket information.
  return GetDirectoryForBucketAndType(url.bucket().value(), url.type(), create);
}

QuotaErrorOr<BucketLocator> ObfuscatedFileUtil::GetOrCreateDefaultBucket(
    const blink::StorageKey& storage_key) {
  // If we have already looked up this default bucket for this StorageKey,
  // return the cached value.
  auto iter = default_buckets_.find(storage_key);
  if (iter != default_buckets_.end()) {
    return iter->second;
  }
  // GetOrCreateBucketSync() called below requires the use of the
  // base::WaitableEvent sync primitive. We must explicitly declare the usage
  // of this primitive to avoid thread restriction errors.
  base::ScopedAllowBaseSyncPrimitives allow_wait;
  // Instead of crashing, return a QuotaError if the proxy is a nullptr.
  if (!sandbox_delegate_->quota_manager_proxy()) {
    LOG(WARNING) << "Failed to GetOrCreateBucket: QuotaManagerProxy is null";
    return base::unexpected(QuotaError::kUnknownError);
  }
  // Retrieve or create the default bucket for this StorageKey.
  ASSIGN_OR_RETURN(
      BucketInfo bucket,
      sandbox_delegate_->quota_manager_proxy()->GetOrCreateBucketSync(
          BucketInitParams::ForDefaultBucket(storage_key)));
  default_buckets_[storage_key] = bucket.ToBucketLocator();
  return bucket.ToBucketLocator();
}

base::File::Error ObfuscatedFileUtil::GetFileInfoInternal(
    SandboxDirectoryDatabase* db,
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    FileId file_id,
    FileInfo* local_info,
    base::File::Info* file_info,
    base::FilePath* platform_file_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db);
  DCHECK(context);
  DCHECK(file_info);
  DCHECK(platform_file_path);

  if (!db->GetFileInfo(file_id, local_info)) {
    return base::File::FILE_ERROR_FAILED;
  }

  if (local_info->is_directory()) {
    file_info->size = 0;
    file_info->is_directory = true;
    file_info->is_symbolic_link = false;
    file_info->last_modified = local_info->modification_time;
    *platform_file_path = base::FilePath();
    // We don't fill in ctime or atime.
    return base::File::FILE_OK;
  }
  if (local_info->data_path.empty())
    return base::File::FILE_ERROR_INVALID_OPERATION;
  base::FilePath local_path = DataPathToLocalPath(url, local_info->data_path);
  base::File::Error error = delegate_->GetFileInfo(local_path, file_info);
  // We should not follow symbolic links in sandboxed file system.
  if (delegate_->IsLink(local_path)) {
    LOG(WARNING) << "Found a symbolic file.";
    error = base::File::FILE_ERROR_NOT_FOUND;
  }
  if (error == base::File::FILE_OK) {
    *platform_file_path = local_path;
  } else if (error == base::File::FILE_ERROR_NOT_FOUND) {
    LOG(WARNING) << "Lost a backing file.";
    InvalidateUsageCache(context, url.storage_key(), url.type());
    if (!db->RemoveFileInfo(file_id))
      return base::File::FILE_ERROR_FAILED;
  }
  return error;
}

base::File ObfuscatedFileUtil::CreateAndOpenFile(
    FileSystemOperationContext* context,
    const FileSystemURL& dest_url,
    FileInfo* dest_file_info,
    int file_flags) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SandboxDirectoryDatabase* db = GetDirectoryDatabase(dest_url, true);

  base::FilePath root, dest_local_path;
  base::File::Error error =
      GenerateNewLocalPath(db, context, dest_url, &root, &dest_local_path);
  if (error != base::File::FILE_OK)
    return base::File(error);

  if (delegate_->PathExists(dest_local_path)) {
    if (!delegate_->DeleteFileOrDirectory(dest_local_path,
                                          false /* recursive */))
      return base::File(base::File::FILE_ERROR_FAILED);
    LOG(WARNING) << "A stray file detected";
    InvalidateUsageCache(context, dest_url.storage_key(), dest_url.type());
  }

  base::File file = delegate_->CreateOrOpen(dest_local_path, file_flags);
  if (!file.IsValid())
    return file;

  if (!file.created()) {
    file.Close();
    delegate_->DeleteFile(dest_local_path);
    return base::File(base::File::FILE_ERROR_FAILED);
  }

  error = CommitCreateFile(root, dest_local_path, db, dest_file_info);
  if (error != base::File::FILE_OK) {
    file.Close();
    delegate_->DeleteFile(dest_local_path);
    return base::File(error);
  }

  return file;
}

base::File::Error ObfuscatedFileUtil::CreateFile(
    FileSystemOperationContext* context,
    const base::FilePath& src_file_path,
    bool foreign_source,
    const FileSystemURL& dest_url,
    FileInfo* dest_file_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SandboxDirectoryDatabase* db = GetDirectoryDatabase(dest_url, true);

  base::FilePath root, dest_local_path;
  base::File::Error error =
      GenerateNewLocalPath(db, context, dest_url, &root, &dest_local_path);
  if (error != base::File::FILE_OK)
    return error;

  bool created = false;
  if (src_file_path.empty()) {
    if (delegate_->PathExists(dest_local_path)) {
      if (!delegate_->DeleteFileOrDirectory(dest_local_path,
                                            false /* recursive */))
        return base::File::FILE_ERROR_FAILED;
      LOG(WARNING) << "A stray file detected";
      InvalidateUsageCache(context, dest_url.storage_key(), dest_url.type());
    }

    error = delegate_->EnsureFileExists(dest_local_path, &created);
  } else {
    if (foreign_source) {
      error = delegate_->CopyInForeignFile(
          src_file_path, dest_local_path,
          FileSystemOperation::CopyOrMoveOptionSet(),
          delegate_->CopyOrMoveModeForDestination(dest_url, true /* copy */));
    } else {
      error = delegate_->CopyOrMoveFile(
          src_file_path, dest_local_path,
          FileSystemOperation::CopyOrMoveOptionSet(),
          delegate_->CopyOrMoveModeForDestination(dest_url, true /* copy */));
    }
    created = true;
  }

  if (error != base::File::FILE_OK)
    return error;
  if (!created)
    return base::File::FILE_ERROR_FAILED;

  return CommitCreateFile(root, dest_local_path, db, dest_file_info);
}

base::File::Error ObfuscatedFileUtil::CommitCreateFile(
    const base::FilePath& root,
    const base::FilePath& local_path,
    SandboxDirectoryDatabase* db,
    FileInfo* dest_file_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // This removes the root, including the trailing slash, leaving a relative
  // path.
  dest_file_info->data_path =
      base::FilePath(local_path.value().substr(root.value().length() + 1));

  FileId file_id;
  base::File::Error error = db->AddFileInfo(*dest_file_info, &file_id);
  if (error != base::File::FILE_OK)
    return error;

  TouchDirectory(db, dest_file_info->parent_id);
  return base::File::FILE_OK;
}

base::FilePath ObfuscatedFileUtil::DataPathToLocalPath(
    const FileSystemURL& url,
    const base::FilePath& data_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ASSIGN_OR_RETURN(base::FilePath root, GetDirectoryForURL(url, false),
                   [](auto) { return base::FilePath(); });
  return root.Append(data_path);
}

// TODO(ericu): How to do the whole validation-without-creation thing?
// We may not have quota even to create the database.
// Ah, in that case don't even get here?
// Still doesn't answer the quota issue, though.
SandboxDirectoryDatabase* ObfuscatedFileUtil::GetDirectoryDatabase(
    const FileSystemURL& url,
    bool create) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DatabaseKey key;
  const std::string type_string =
      SandboxFileSystemBackendDelegate::GetTypeString(url.type());
  // `key.bucket()` is std::nullopt for all non-kTemporary types.
  if (FileSystemTypeToQuotaStorageType(url.type()) ==
      ::blink::mojom::StorageType::kTemporary) {
    if (url.bucket().has_value()) {
      key = DatabaseKey(url.storage_key(), url.bucket().value(), type_string);
    } else {
      // If we are not provided a custom bucket value we must find the default
      // bucket corresponding to the url's StorageKey.
      ASSIGN_OR_RETURN(BucketLocator default_bucket,
                       GetOrCreateDefaultBucket(url.storage_key()),
                       [](auto) { return nullptr; });
      key = DatabaseKey(url.storage_key(), std::move(default_bucket),
                        type_string);
    }
  } else {  // All other storage types.
    key = DatabaseKey(url.storage_key(), std::nullopt, type_string);
  }

  auto iter = directories_.find(key);
  if (iter != directories_.end()) {
    MarkUsed();
    return iter->second.get();
  }

  ASSIGN_OR_RETURN(base::FilePath path, GetDirectoryForURL(url, create),
                   [&](base::File::Error error) {
                     LOG(WARNING) << "Failed to get origin+type directory: "
                                  << url.DebugString() << " error:" << error;
                     return nullptr;
                   });
  MarkUsed();
  directories_[key] = std::make_unique<SandboxDirectoryDatabase>(
      std::move(path), env_override_);
  return directories_[key].get();
}

base::FileErrorOr<base::FilePath> ObfuscatedFileUtil::GetDirectoryForStorageKey(
    const blink::StorageKey& storage_key,
    bool create) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (storage_key.IsThirdPartyContext()) {
    // Retrieve the default bucket value for `storage_key`.
    ASSIGN_OR_RETURN(BucketLocator bucket,
                     GetOrCreateDefaultBucket(storage_key),
                     [](auto) { return base::File::FILE_ERROR_FAILED; });
    // Get the path and verify it is valid.
    base::FilePath path =
        sandbox_delegate_->quota_manager_proxy()->GetClientBucketPath(
            std::move(bucket), QuotaClientType::kFileSystem);
    base::File::Error error = GetDirectoryHelper(path, create);
    if (error != base::File::FILE_OK)
      return base::unexpected(error);
    return path;
  }

  if (!InitOriginDatabase(storage_key.origin(), create)) {
    return base::FileErrorOr<base::FilePath>(
        base::unexpected(create ? base::File::FILE_ERROR_FAILED
                                : base::File::FILE_ERROR_NOT_FOUND));
  }
  base::FilePath directory_name;
  std::string id = GetIdentifierFromOrigin(storage_key.origin());

  bool exists_in_db = origin_database_->HasOriginPath(id);
  if (!exists_in_db && !create) {
    return base::unexpected(base::File::FILE_ERROR_NOT_FOUND);
  }
  if (!origin_database_->GetPathForOrigin(id, &directory_name)) {
    return base::unexpected(base::File::FILE_ERROR_FAILED);
  }

  base::FilePath path = file_system_directory_.Append(directory_name);
  bool exists_in_fs = delegate_->DirectoryExists(path);
  if (!exists_in_db && exists_in_fs) {
    if (!delegate_->DeleteFileOrDirectory(path, true)) {
      return base::unexpected(base::File::FILE_ERROR_FAILED);
    }
    exists_in_fs = false;
  }

  if (!exists_in_fs) {
    if (!create || delegate_->CreateDirectory(path, false /* exclusive */,
                                              true /* recursive */) !=
                       base::File::FILE_OK) {
      return base::unexpected(create ? base::File::FILE_ERROR_FAILED
                                     : base::File::FILE_ERROR_NOT_FOUND);
    }
  }
  return path;
}

base::File::Error ObfuscatedFileUtil::GetDirectoryHelper(
    const base::FilePath& path,
    bool create) {
  if (!delegate_->DirectoryExists(path) &&
      (!create ||
       delegate_->CreateDirectory(path, /*exclusive=*/false,
                                  /*recursive=*/true) != base::File::FILE_OK)) {
    return create ? base::File::FILE_ERROR_FAILED
                  : base::File::FILE_ERROR_NOT_FOUND;
  }
  return base::File::FILE_OK;
}

void ObfuscatedFileUtil::InvalidateUsageCache(
    FileSystemOperationContext* context,
    const blink::StorageKey& storage_key,
    FileSystemType type) {
  if (sandbox_delegate_)
    sandbox_delegate_->InvalidateUsageCache(storage_key, type);
}

void ObfuscatedFileUtil::MarkUsed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  timer_.Start(FROM_HERE, base::Seconds(db_flush_delay_seconds_), this,
               &ObfuscatedFileUtil::DropDatabases);
}

void ObfuscatedFileUtil::DropDatabases() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  origin_database_.reset();
  directories_.clear();
  timer_.Stop();
}

void ObfuscatedFileUtil::RewriteDatabases() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (origin_database_)
    origin_database_->RewriteDatabase();
}

void ObfuscatedFileUtil::DeleteDefaultBucketForStorageKey(
    const blink::StorageKey& storage_key) {
  auto default_bucket_iter = default_buckets_.find(storage_key);
  // If we are not already caching the bucket for that StorageKey, it does not
  // need to be deleted.
  if (default_bucket_iter == default_buckets_.end())
    return;
  BucketLocator default_bucket = default_buckets_[storage_key];
  // Ensure that all directories with that StorageKey and bucket have been
  // erased.
  DCHECK(directories_.find(
             DatabaseKey(storage_key, default_bucket,
                         SandboxFileSystemBackendDelegate::GetTypeString(
                             FileSystemType::kFileSystemTypeTemporary))) ==
         directories_.end());
  default_buckets_.erase(default_bucket_iter);
}

bool ObfuscatedFileUtil::InitOriginDatabase(const url::Origin& origin_hint,
                                            bool create) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (origin_database_)
    return true;

  if (!delegate_->DirectoryExists(file_system_directory_)) {
    if (!create)
      return false;
    if (delegate_->CreateDirectory(
            file_system_directory_, false /* exclusive */,
            true /* recursive */) != base::File::FILE_OK) {
      LOG(WARNING) << "Failed to create FileSystem directory: "
                   << file_system_directory_.value();
      return false;
    }
  }

  origin_database_ = std::make_unique<SandboxOriginDatabase>(
      file_system_directory_, env_override_);

  return true;
}

base::File::Error ObfuscatedFileUtil::GenerateNewLocalPath(
    SandboxDirectoryDatabase* db,
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    base::FilePath* root,
    base::FilePath* local_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(local_path);
  int64_t number;
  if (!db || !db->GetNextInteger(&number))
    return base::File::FILE_ERROR_FAILED;

  ASSIGN_OR_RETURN(*root, GetDirectoryForURL(url, false));

  // We use the third- and fourth-to-last digits as the directory.
  int64_t directory_number = number % 10000 / 100;
  base::FilePath new_local_path =
      root->AppendASCII(base::StringPrintf("%02" PRId64, directory_number));

  base::File::Error error = base::File::FILE_OK;
  error = delegate_->CreateDirectory(new_local_path, false /* exclusive */,
                                     false /* recursive */);
  if (error != base::File::FILE_OK)
    return error;

  *local_path =
      new_local_path.AppendASCII(base::StringPrintf("%08" PRId64, number));
  return base::File::FILE_OK;
}

base::File ObfuscatedFileUtil::CreateOrOpenInternal(
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    int file_flags) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!(file_flags &
           (base::File::FLAG_DELETE_ON_CLOSE | base::File::FLAG_WIN_HIDDEN |
            base::File::FLAG_WIN_EXCLUSIVE_READ |
            base::File::FLAG_WIN_EXCLUSIVE_WRITE)));
  SandboxDirectoryDatabase* db = GetDirectoryDatabase(url, true);
  if (!db)
    return base::File(base::File::FILE_ERROR_FAILED);
  FileId file_id;
  if (!db->GetFileWithPath(url.path(), &file_id)) {
    // The file doesn't exist.
    if (!(file_flags &
          (base::File::FLAG_CREATE | base::File::FLAG_CREATE_ALWAYS |
           base::File::FLAG_OPEN_ALWAYS))) {
      return base::File(base::File::FILE_ERROR_NOT_FOUND);
    }
    FileId parent_id;
    if (!db->GetFileWithPath(VirtualPath::DirName(url.path()), &parent_id))
      return base::File(base::File::FILE_ERROR_NOT_FOUND);
    FileInfo file_info;
    InitFileInfo(&file_info, parent_id,
                 VirtualPath::BaseName(url.path()).value());

    int64_t growth = UsageForPath(file_info.name.size());
    if (!AllocateQuota(context, growth))
      return base::File(base::File::FILE_ERROR_NO_SPACE);
    base::File file = CreateAndOpenFile(context, url, &file_info, file_flags);
    if (file.IsValid()) {
      UpdateUsage(context, url, growth);
      context->change_observers()->Notify(&FileChangeObserver::OnCreateFile,
                                          url);
    }
    return file;
  }

  if (file_flags & base::File::FLAG_CREATE)
    return base::File(base::File::FILE_ERROR_EXISTS);

  base::File::Info platform_file_info;
  base::FilePath local_path;
  FileInfo file_info;
  base::File::Error error = GetFileInfoInternal(
      db, context, url, file_id, &file_info, &platform_file_info, &local_path);
  if (error != base::File::FILE_OK)
    return base::File(error);
  if (file_info.is_directory())
    return base::File(base::File::FILE_ERROR_NOT_A_FILE);

  int64_t delta = 0;
  if (file_flags &
      (base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_OPEN_TRUNCATED)) {
    // The file exists and we're truncating.
    delta = -platform_file_info.size;
    AllocateQuota(context, delta);
  }

  base::File file = delegate_->CreateOrOpen(local_path, file_flags);
  if (!file.IsValid()) {
    error = file.error_details();
    if (error == base::File::FILE_ERROR_NOT_FOUND) {
      // TODO(tzik): Also invalidate on-memory usage cache in UsageTracker.
      // TODO(tzik): Delete database entry after ensuring the file lost.
      InvalidateUsageCache(context, url.storage_key(), url.type());
      LOG(WARNING) << "Lost a backing file.";
      return base::File(base::File::FILE_ERROR_FAILED);
    }
    return file;
  }

  // If truncating we need to update the usage.
  if (delta) {
    UpdateUsage(context, url, delta);
    context->change_observers()->Notify(&FileChangeObserver::OnModifyFile, url);
  }
  return file;
}

bool ObfuscatedFileUtil::HasIsolatedStorage(
    const blink::StorageKey& storage_key) {
  return special_storage_policy_.get() &&
         special_storage_policy_->HasIsolatedStorage(
             storage_key.origin().GetURL());
}

}  // namespace storage
