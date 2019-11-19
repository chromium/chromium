// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/obfuscated_file_util.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <tuple>

#include "base/bind.h"
#include "base/containers/queue.h"
#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "storage/browser/file_system/file_observers.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation_context.h"
#include "storage/browser/file_system/obfuscated_file_util_disk_delegate.h"
#include "storage/browser/file_system/obfuscated_file_util_memory_delegate.h"
#include "storage/browser/file_system/sandbox_file_system_backend.h"
#include "storage/browser/file_system/sandbox_isolated_origin_database.h"
#include "storage/browser/file_system/sandbox_origin_database.h"
#include "storage/browser/file_system/sandbox_prioritized_origin_database.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/common/database/database_identifier.h"
#include "storage/common/file_system/file_system_util.h"
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

const char kDirectoryDatabaseKeySeparator = ' ';

int64_t UsageForPath(size_t length) {
  return kPathCreationQuotaCost +
         static_cast<int64_t>(length) * kPathByteQuotaCost;
}

bool AllocateQuota(FileSystemOperationContext* context, int64_t growth) {
  if (context->allowed_bytes_growth() == storage::QuotaManager::kNoLimit)
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

  SandboxDirectoryDatabase* db_;
  FileSystemOperationContext* context_;
  ObfuscatedFileUtil* obfuscated_file_util_;
  FileSystemURL root_url_;
  bool recursive_;

  base::queue<FileRecord> recurse_queue_;
  std::vector<FileId> display_stack_;
  base::FilePath current_parent_virtual_path_;

  FileId current_file_id_;
  base::File::Info current_platform_file_info_;
};

class ObfuscatedOriginEnumerator
    : public ObfuscatedFileUtil::AbstractOriginEnumerator {
 public:
  using OriginRecord = SandboxOriginDatabase::OriginRecord;
  ObfuscatedOriginEnumerator(
      SandboxOriginDatabaseInterface* origin_database,
      base::WeakPtr<ObfuscatedFileUtilMemoryDelegate> memory_file_util,
      const base::FilePath& base_file_path)
      : base_file_path_(base_file_path),
        memory_file_util_(std::move(memory_file_util)) {
    if (origin_database)
      origin_database->ListAllOrigins(&origins_);
  }

  ~ObfuscatedOriginEnumerator() override = default;

  // Returns the next origin.  Returns empty if there are no more origins.
  GURL Next() override {
    OriginRecord record;
    if (!origins_.empty()) {
      record = origins_.back();
      origins_.pop_back();
    }
    current_ = record;
    return storage::GetOriginURLFromIdentifier(record.origin);
  }

  // Returns the current origin's information.
  bool HasTypeDirectory(const std::string& type_string) const override {
    if (current_.path.empty())
      return false;
    if (type_string.empty()) {
      NOTREACHED();
      return false;
    }
    base::FilePath path =
        base_file_path_.Append(current_.path).AppendASCII(type_string);
    if (memory_file_util_)
      return memory_file_util_->DirectoryExists(path);
    else
      return base::DirectoryExists(path);
  }

 private:
  std::vector<OriginRecord> origins_;
  OriginRecord current_;
  base::FilePath base_file_path_;
  base::WeakPtr<ObfuscatedFileUtilMemoryDelegate> memory_file_util_;
};

ObfuscatedFileUtil::ObfuscatedFileUtil(
    storage::SpecialStoragePolicy* special_storage_policy,
    const base::FilePath& file_system_directory,
    leveldb::Env* env_override,
    GetTypeStringForURLCallback get_type_string_for_url,
    const std::set<std::string>& known_type_strings,
    SandboxFileSystemBackendDelegate* sandbox_delegate,
    bool is_incognito)
    : special_storage_policy_(special_storage_policy),
      file_system_directory_(file_system_directory),
      env_override_(env_override),
      is_incognito_(is_incognito),
      db_flush_delay_seconds_(10 * 60),  // 10 mins.
      get_type_string_for_url_(std::move(get_type_string_for_url)),
      known_type_strings_(known_type_strings),
      sandbox_delegate_(sandbox_delegate) {
  DCHECK(!get_type_string_for_url_.is_null());
  DETACH_FROM_SEQUENCE(sequence_checker_);
  DCHECK(!is_incognito_ ||
         (env_override && leveldb_chrome::IsMemEnv(env_override)));

  if (is_incognito_) {
    delegate_ = std::make_unique<ObfuscatedFileUtilMemoryDelegate>(
        file_system_directory_);
  } else {
    delegate_ = std::make_unique<ObfuscatedFileUtilDiskDelegate>();
  }
}

ObfuscatedFileUtil::~ObfuscatedFileUtil() {
  // Destruction can happen on any sequence.
  DETACH_FROM_SEQUENCE(sequence_checker_);

  DropDatabases();
}

base::File ObfuscatedFileUtil::CreateOrOpen(FileSystemOperationContext* context,
                                            const FileSystemURL& url,
                                            int file_flags) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::File file = CreateOrOpenInternal(context, url, file_flags);
  if (file.IsValid() && file_flags & base::File::FLAG_WRITE &&
      context->quota_limit_type() == storage::kQuotaLimitTypeUnlimited &&
      sandbox_delegate_) {
    sandbox_delegate_->StickyInvalidateUsageCache(url.origin().GetURL(),
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
      NOTREACHED();
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
      NOTREACHED();
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
    context->change_observers()->Notify(&FileChangeObserver::OnCreateDirectory,
                                        url);
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
    NOTREACHED();
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
    NOTREACHED();
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
    CopyOrMoveOption option,
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
          src_local_path, dest_local_path, option,
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

  if (overwrite) {
    context->change_observers()->Notify(&FileChangeObserver::OnModifyFile,
                                        dest_url);
  } else {
    context->change_observers()->Notify(&FileChangeObserver::OnCreateFileFrom,
                                        dest_url, src_url);
  }

  if (!copy) {
    context->change_observers()->Notify(&FileChangeObserver::OnRemoveFile,
                                        src_url);
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
        src_file_path, dest_local_path, FileSystemOperation::OPTION_NONE,
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
    NOTREACHED();
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
  FileInfo file_info;
  if (!db->GetFileInfo(file_id, &file_info)) {
    NOTREACHED();
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

storage::ScopedFile ObfuscatedFileUtil::CreateSnapshotFile(
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
  return storage::ScopedFile();
}

std::unique_ptr<FileSystemFileUtil::AbstractFileEnumerator>
ObfuscatedFileUtil::CreateFileEnumerator(FileSystemOperationContext* context,
                                         const FileSystemURL& root_url,
                                         bool recursive) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SandboxDirectoryDatabase* db = GetDirectoryDatabase(root_url, false);
  if (!db) {
    return base::WrapUnique(new EmptyFileEnumerator);
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
    DCHECK(!file_id);
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

base::FilePath ObfuscatedFileUtil::GetDirectoryForOriginAndType(
    const GURL& origin,
    const std::string& type_string,
    bool create,
    base::File::Error* error_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::FilePath origin_dir = GetDirectoryForOrigin(origin, create, error_code);
  if (origin_dir.empty())
    return base::FilePath();
  if (type_string.empty())
    return origin_dir;
  base::FilePath path = origin_dir.AppendASCII(type_string);
  base::File::Error error = base::File::FILE_OK;
  if (!delegate_->DirectoryExists(path) &&
      (!create || delegate_->CreateDirectory(path, false /* exclusive */,
                                             true /* recursive */) !=
                      base::File::FILE_OK)) {
    error = create ? base::File::FILE_ERROR_FAILED
                   : base::File::FILE_ERROR_NOT_FOUND;
  }

  if (error_code)
    *error_code = error;
  return path;
}

bool ObfuscatedFileUtil::DeleteDirectoryForOriginAndType(
    const GURL& origin,
    const std::string& type_string) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DestroyDirectoryDatabase(origin, type_string);

  const base::FilePath origin_path =
      GetDirectoryForOrigin(origin, false, nullptr);
  if (origin_path.empty())
    return true;

  if (!type_string.empty()) {
    // Delete the filesystem type directory.
    base::File::Error error = base::File::FILE_OK;
    const base::FilePath origin_type_path =
        GetDirectoryForOriginAndType(origin, type_string, false, &error);
    if (error == base::File::FILE_ERROR_FAILED)
      return false;
    if (error == base::File::FILE_OK && !origin_type_path.empty() &&
        !delegate_->DeleteFileOrDirectory(origin_type_path,
                                          true /* recursive */)) {
      return false;
    }

    // At this point we are sure we had successfully deleted the origin/type
    // directory (i.e. we're ready to just return true).
    // See if we have other directories in this origin directory.
    for (const std::string& type : known_type_strings_) {
      if (type == type_string)
        continue;
      if (delegate_->DirectoryExists(origin_path.AppendASCII(type))) {
        // Other type's directory exists; just return true here.
        return true;
      }
    }
  }

  // No other directories seem exist. Try deleting the entire origin directory.
  InitOriginDatabase(origin, false);
  if (origin_database_) {
    origin_database_->RemovePathForOrigin(
        storage::GetIdentifierFromOrigin(origin));
  }
  return delegate_->DeleteFileOrDirectory(origin_path, true /* recursive */);
}

void ObfuscatedFileUtil::CloseFileSystemForOriginAndType(
    const GURL& origin,
    const std::string& type_string) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const std::string key_prefix = GetDirectoryDatabaseKey(origin, type_string);
  for (auto iter = directories_.lower_bound(key_prefix);
       iter != directories_.end();) {
    if (!base::StartsWith(iter->first, key_prefix,
                          base::CompareCase::SENSITIVE))
      break;
    DCHECK(type_string.empty() || iter->first == key_prefix);
    directories_.erase(iter++);
  }
}

std::unique_ptr<ObfuscatedFileUtil::AbstractOriginEnumerator>
ObfuscatedFileUtil::CreateOriginEnumerator() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<SandboxOriginDatabase::OriginRecord> origins;

  InitOriginDatabase(GURL(), false);
  base::WeakPtr<ObfuscatedFileUtilMemoryDelegate> file_util_delegate;
  if (is_incognito()) {
    file_util_delegate =
        static_cast<ObfuscatedFileUtilMemoryDelegate*>(delegate())
            ->GetWeakPtr();
  }
  return std::make_unique<ObfuscatedOriginEnumerator>(
      origin_database_.get(), file_util_delegate, file_system_directory_);
}

void ObfuscatedFileUtil::DestroyDirectoryDatabase(
    const GURL& origin,
    const std::string& type_string) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If |type_string| is empty, delete all filesystem types under |origin|.
  const std::string key_prefix = GetDirectoryDatabaseKey(origin, type_string);
  for (auto iter = directories_.lower_bound(key_prefix);
       iter != directories_.end();) {
    if (!base::StartsWith(iter->first, key_prefix,
                          base::CompareCase::SENSITIVE))
      break;
    DCHECK(type_string.empty() || iter->first == key_prefix);
    std::unique_ptr<SandboxDirectoryDatabase> database =
        std::move(iter->second);
    directories_.erase(iter++);

    // Continue to destroy databases even if it failed because it doesn't affect
    // the final result.
    database->DestroyDatabase();
  }
}

// static
int64_t ObfuscatedFileUtil::ComputeFilePathCost(const base::FilePath& path) {
  return UsageForPath(VirtualPath::BaseName(path).value().size());
}

void ObfuscatedFileUtil::MaybePrepopulateDatabase(
    const std::vector<std::string>& type_strings_to_prepopulate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  SandboxPrioritizedOriginDatabase database(file_system_directory_,
                                            env_override_);
  std::string origin_string = database.GetPrimaryOrigin();
  if (origin_string.empty() || !database.HasOriginPath(origin_string))
    return;
  const GURL origin = storage::GetOriginURLFromIdentifier(origin_string);

  // Prepopulate the directory database(s) if and only if this instance
  // has primary origin and the directory database is already there.
  for (size_t i = 0; i < type_strings_to_prepopulate.size(); ++i) {
    const std::string type_string = type_strings_to_prepopulate[i];
    // Only handles known types.
    if (!base::Contains(known_type_strings_, type_string))
      continue;
    base::File::Error error = base::File::FILE_ERROR_FAILED;
    base::FilePath path =
        GetDirectoryForOriginAndType(origin, type_string, false, &error);
    if (error != base::File::FILE_OK)
      continue;
    std::unique_ptr<SandboxDirectoryDatabase> db =
        std::make_unique<SandboxDirectoryDatabase>(path, env_override_);
    if (db->Init(SandboxDirectoryDatabase::FAIL_ON_CORRUPTION)) {
      directories_[GetDirectoryDatabaseKey(origin, type_string)] =
          std::move(db);
      MarkUsed();
      // Don't populate more than one database, as it may rather hurt
      // performance.
      break;
    }
  }
}

base::FilePath ObfuscatedFileUtil::GetDirectoryForURL(
    const FileSystemURL& url,
    bool create,
    base::File::Error* error_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetDirectoryForOriginAndType(
      url.origin().GetURL(), CallGetTypeStringForURL(url), create, error_code);
}

std::string ObfuscatedFileUtil::CallGetTypeStringForURL(
    const FileSystemURL& url) {
  DCHECK(!get_type_string_for_url_.is_null());
  return get_type_string_for_url_.Run(url);
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
    NOTREACHED();
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
    InvalidateUsageCache(context, url.origin(), url.type());
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
    InvalidateUsageCache(context, dest_url.origin(), dest_url.type());
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
      InvalidateUsageCache(context, dest_url.origin(), dest_url.type());
    }

    error = delegate_->EnsureFileExists(dest_local_path, &created);
  } else {
    if (foreign_source) {
      error = delegate_->CopyInForeignFile(
          src_file_path, dest_local_path, FileSystemOperation::OPTION_NONE,
          delegate_->CopyOrMoveModeForDestination(dest_url, true /* copy */));
    } else {
      error = delegate_->CopyOrMoveFile(
          src_file_path, dest_local_path, FileSystemOperation::OPTION_NONE,
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
  base::File::Error error = base::File::FILE_OK;
  base::FilePath root = GetDirectoryForURL(url, false, &error);
  if (error != base::File::FILE_OK)
    return base::FilePath();
  return root.Append(data_path);
}

std::string ObfuscatedFileUtil::GetDirectoryDatabaseKey(
    const GURL& origin,
    const std::string& type_string) {
  // For isolated origin we just use a type string as a key.
  return storage::GetIdentifierFromOrigin(origin) +
         kDirectoryDatabaseKeySeparator + type_string;
}

// TODO(ericu): How to do the whole validation-without-creation thing?
// We may not have quota even to create the database.
// Ah, in that case don't even get here?
// Still doesn't answer the quota issue, though.
SandboxDirectoryDatabase* ObfuscatedFileUtil::GetDirectoryDatabase(
    const FileSystemURL& url,
    bool create) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::string key = GetDirectoryDatabaseKey(url.origin().GetURL(),
                                            CallGetTypeStringForURL(url));
  if (key.empty())
    return nullptr;

  auto iter = directories_.find(key);
  if (iter != directories_.end()) {
    MarkUsed();
    return iter->second.get();
  }

  base::File::Error error = base::File::FILE_OK;
  base::FilePath path = GetDirectoryForURL(url, create, &error);
  if (error != base::File::FILE_OK) {
    LOG(WARNING) << "Failed to get origin+type directory: " << url.DebugString()
                 << " error:" << error;
    return nullptr;
  }
  MarkUsed();
  directories_[key] =
      std::make_unique<SandboxDirectoryDatabase>(path, env_override_);
  return directories_[key].get();
}

base::FilePath ObfuscatedFileUtil::GetDirectoryForOrigin(
    const GURL& origin,
    bool create,
    base::File::Error* error_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!InitOriginDatabase(origin, create)) {
    if (error_code) {
      *error_code = create ? base::File::FILE_ERROR_FAILED
                           : base::File::FILE_ERROR_NOT_FOUND;
    }
    return base::FilePath();
  }
  base::FilePath directory_name;
  std::string id = storage::GetIdentifierFromOrigin(origin);

  bool exists_in_db = origin_database_->HasOriginPath(id);
  if (!exists_in_db && !create) {
    if (error_code)
      *error_code = base::File::FILE_ERROR_NOT_FOUND;
    return base::FilePath();
  }
  if (!origin_database_->GetPathForOrigin(id, &directory_name)) {
    if (error_code)
      *error_code = base::File::FILE_ERROR_FAILED;
    return base::FilePath();
  }

  base::FilePath path = file_system_directory_.Append(directory_name);
  bool exists_in_fs = delegate_->DirectoryExists(path);
  if (!exists_in_db && exists_in_fs) {
    if (!delegate_->DeleteFileOrDirectory(path, true)) {
      if (error_code)
        *error_code = base::File::FILE_ERROR_FAILED;
      return base::FilePath();
    }
    exists_in_fs = false;
  }

  if (!exists_in_fs) {
    if (!create || delegate_->CreateDirectory(path, false /* exclusive */,
                                              true /* recursive */) !=
                       base::File::FILE_OK) {
      if (error_code)
        *error_code = create ? base::File::FILE_ERROR_FAILED
                             : base::File::FILE_ERROR_NOT_FOUND;
      return base::FilePath();
    }
  }

  if (error_code)
    *error_code = base::File::FILE_OK;

  return path;
}

void ObfuscatedFileUtil::InvalidateUsageCache(
    FileSystemOperationContext* context,
    const url::Origin& origin,
    FileSystemType type) {
  if (sandbox_delegate_)
    sandbox_delegate_->InvalidateUsageCache(origin.GetURL(), type);
}

void ObfuscatedFileUtil::MarkUsed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (timer_.IsRunning()) {
    timer_.Reset();
  } else {
    timer_.Start(FROM_HERE,
                 base::TimeDelta::FromSeconds(db_flush_delay_seconds_),
                 base::BindOnce(&ObfuscatedFileUtil::DropDatabases,
                                base::Unretained(this)));
  }
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

bool ObfuscatedFileUtil::InitOriginDatabase(const GURL& origin_hint,
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

  SandboxPrioritizedOriginDatabase* prioritized_origin_database =
      new SandboxPrioritizedOriginDatabase(file_system_directory_,
                                           env_override_);
  origin_database_.reset(prioritized_origin_database);

  if (origin_hint.is_empty() || !HasIsolatedStorage(origin_hint))
    return true;

  const std::string isolated_origin_string =
      storage::GetIdentifierFromOrigin(origin_hint);

  prioritized_origin_database->InitializePrimaryOrigin(isolated_origin_string);

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

  base::File::Error error = base::File::FILE_OK;
  *root = GetDirectoryForURL(url, false, &error);
  if (error != base::File::FILE_OK)
    return error;

  // We use the third- and fourth-to-last digits as the directory.
  int64_t directory_number = number % 10000 / 100;
  base::FilePath new_local_path =
      root->AppendASCII(base::StringPrintf("%02" PRId64, directory_number));

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
  DCHECK(
      !(file_flags &
        (base::File::FLAG_DELETE_ON_CLOSE | base::File::FLAG_HIDDEN |
         base::File::FLAG_EXCLUSIVE_READ | base::File::FLAG_EXCLUSIVE_WRITE)));
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
      InvalidateUsageCache(context, url.origin(), url.type());
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

bool ObfuscatedFileUtil::HasIsolatedStorage(const GURL& origin) {
  return special_storage_policy_.get() &&
         special_storage_policy_->HasIsolatedStorage(origin);
}

}  // namespace storage
