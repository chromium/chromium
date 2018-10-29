// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/plugin_private_storage_helper.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "ppapi/shared_impl/ppapi_constants.h"
#include "storage/browser/fileapi/async_file_util.h"
#include "storage/browser/fileapi/async_file_util_adapter.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "storage/browser/fileapi/isolated_context.h"
#include "storage/browser/fileapi/obfuscated_file_util.h"
#include "storage/common/fileapi/file_system_util.h"

namespace content {

namespace {

std::string StringTypeToString(const base::FilePath::StringType& value) {
#if defined(OS_POSIX)
  return value;
#elif defined(OS_WIN)
  return base::WideToUTF8(value);
#endif
}

// Helper for checking the plugin private data for a specified origin and
// plugin for the existence of any file that matches the time range specified.
// All of the operations in this class are done on the IO thread.
//
// This class keeps track of outstanding async requests it generates, and does
// not call |callback_| until they all respond (and thus don't need to worry
// about lifetime of |this| in the async requests). If the data for the origin
// needs to be deleted, it needs to be done on the file task runner, so we
// want to ensure that there are no pending requests that may prevent the
// date from being deleted.
class PluginPrivateDataByOriginChecker {
 public:
  PluginPrivateDataByOriginChecker(
      storage::FileSystemContext* filesystem_context,
      const GURL& origin,
      const std::string& plugin_name,
      const base::Time begin,
      const base::Time end,
      const base::Callback<void(bool, const GURL&)>& callback)
      : filesystem_context_(filesystem_context),
        origin_(origin),
        plugin_name_(plugin_name),
        begin_(begin),
        end_(end),
        callback_(callback) {
    // Create the filesystem ID.
    fsid_ = storage::IsolatedContext::GetInstance()
                ->RegisterFileSystemForVirtualPath(
                    storage::kFileSystemTypePluginPrivate,
                    ppapi::kPluginPrivateRootName, base::FilePath());
  }
  ~PluginPrivateDataByOriginChecker() {}

  // Checks the files contained in the plugin private filesystem for |origin_|
  // and |plugin_name_| for any file whose last modified time is between
  // |begin_| and |end_|. |callback_| is called when all actions are complete
  // with true and the origin if any such file is found, false and empty GURL
  // otherwise.
  void CheckFilesOnIOThread();

 private:
  void OnFileSystemOpened(base::File::Error result);
  void OnDirectoryRead(const std::string& root,
                       base::File::Error result,
                       storage::AsyncFileUtil::EntryList file_list,
                       bool has_more);
  void OnFileInfo(const std::string& file_name,
                  base::File::Error result,
                  const base::File::Info& file_info);

  // Keeps track of the pending work. When |task_count_| goes to 0 then
  // |callback_| is called and this helper object is destroyed.
  void IncrementTaskCount();
  void DecrementTaskCount();

  // Not owned by this object. Caller is responsible for keeping the
  // FileSystemContext alive until |callback_| is called.
  storage::FileSystemContext* filesystem_context_;

  const GURL origin_;
  const std::string plugin_name_;
  const base::Time begin_;
  const base::Time end_;
  const base::Callback<void(bool, const GURL&)> callback_;
  std::string fsid_;
  int task_count_ = 0;

  // Keep track if the data for this origin needs to be deleted due to
  // any file found that has last modified time between |begin_| and |end_|.
  bool delete_this_origin_data_ = false;

  // Keep track if any files exist for this origin.
  bool files_found_ = false;
};

void PluginPrivateDataByOriginChecker::CheckFilesOnIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(storage::ValidateIsolatedFileSystemId(fsid_));

  IncrementTaskCount();
  filesystem_context_->OpenPluginPrivateFileSystem(
      origin_, storage::kFileSystemTypePluginPrivate, fsid_, plugin_name_,
      storage::OPEN_FILE_SYSTEM_FAIL_IF_NONEXISTENT,
      base::BindOnce(&PluginPrivateDataByOriginChecker::OnFileSystemOpened,
                     base::Unretained(this)));
}

void PluginPrivateDataByOriginChecker::OnFileSystemOpened(
    base::File::Error result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(3) << "Opened filesystem for " << origin_ << ":" << plugin_name_
           << ", result: " << result;

  // If we can't open the directory, we can't delete files so simply return.
  if (result != base::File::FILE_OK) {
    DecrementTaskCount();
    return;
  }

  storage::AsyncFileUtil* file_util = filesystem_context_->GetAsyncFileUtil(
      storage::kFileSystemTypePluginPrivate);
  std::string root = storage::GetIsolatedFileSystemRootURIString(
      origin_, fsid_, ppapi::kPluginPrivateRootName);
  std::unique_ptr<storage::FileSystemOperationContext> operation_context =
      std::make_unique<storage::FileSystemOperationContext>(
          filesystem_context_);
  file_util->ReadDirectory(
      std::move(operation_context), filesystem_context_->CrackURL(GURL(root)),
      base::BindRepeating(&PluginPrivateDataByOriginChecker::OnDirectoryRead,
                          base::Unretained(this), root));
}

void PluginPrivateDataByOriginChecker::OnDirectoryRead(
    const std::string& root,
    base::File::Error result,
    storage::AsyncFileUtil::EntryList file_list,
    bool has_more) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(3) << __func__ << " result: " << result
           << ", #files: " << file_list.size();

  // Quit if there is an error.
  if (result != base::File::FILE_OK) {
    DLOG(ERROR) << "Unable to read directory for " << origin_ << ":"
                << plugin_name_;
    DecrementTaskCount();
    return;
  }

  // If there are files found, keep track of it.
  if (!file_list.empty())
    files_found_ = true;

  // No error, process the files returned. No need to do this if we have
  // already decided to delete all the data for this origin.
  if (!delete_this_origin_data_) {
    storage::AsyncFileUtil* file_util = filesystem_context_->GetAsyncFileUtil(
        storage::kFileSystemTypePluginPrivate);
    for (const auto& file : file_list) {
      DVLOG(3) << __func__ << " file: " << file.name.value();
      // Nested directories not implemented.
      DCHECK_NE(file.type, filesystem::mojom::FsFileType::DIRECTORY);

      std::unique_ptr<storage::FileSystemOperationContext> operation_context =
          std::make_unique<storage::FileSystemOperationContext>(
              filesystem_context_);
      storage::FileSystemURL file_url = filesystem_context_->CrackURL(
          GURL(root + StringTypeToString(file.name.value())));
      IncrementTaskCount();
      file_util->GetFileInfo(
          std::move(operation_context), file_url,
          storage::FileSystemOperation::GET_METADATA_FIELD_SIZE |
              storage::FileSystemOperation::GET_METADATA_FIELD_LAST_MODIFIED,
          base::BindOnce(&PluginPrivateDataByOriginChecker::OnFileInfo,
                         base::Unretained(this),
                         StringTypeToString(file.name.value())));
    }
  }

  // If there are more files in this directory, wait for the next call.
  if (has_more)
    return;

  DecrementTaskCount();
}

void PluginPrivateDataByOriginChecker::OnFileInfo(
    const std::string& file_name,
    base::File::Error result,
    const base::File::Info& file_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (result == base::File::FILE_OK) {
    DVLOG(3) << __func__ << " name: " << file_name
             << ", size: " << file_info.size
             << ", modified: " << file_info.last_modified;
    if (file_info.last_modified >= begin_ && file_info.last_modified <= end_)
      delete_this_origin_data_ = true;
  }

  DecrementTaskCount();
}

void PluginPrivateDataByOriginChecker::IncrementTaskCount() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  ++task_count_;
}

void PluginPrivateDataByOriginChecker::DecrementTaskCount() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_GT(task_count_, 0);
  --task_count_;
  if (task_count_)
    return;

  // If no files exist for this origin, then we can safely delete it.
  if (!files_found_)
    delete_this_origin_data_ = true;

  // If there are no more tasks in progress, then run |callback_| on the
  // proper thread.
  filesystem_context_->default_file_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(callback_, delete_this_origin_data_, origin_));
  delete this;
}

// Helper for deleting the plugin private data.
// All of the operations in this class are done on the file task runner.
class PluginPrivateDataDeletionHelper {
 public:
  PluginPrivateDataDeletionHelper(
      scoped_refptr<storage::FileSystemContext> filesystem_context,
      const base::Time begin,
      const base::Time end,
      const base::Closure& callback)
      : filesystem_context_(std::move(filesystem_context)),
        begin_(begin),
        end_(end),
        callback_(callback) {}
  ~PluginPrivateDataDeletionHelper() {}

  void CheckOriginsOnFileTaskRunner(const std::set<GURL>& origins);

 private:
  // Keeps track of the pending work. When |task_count_| goes to 0 then
  // |callback_| is called and this helper object is destroyed.
  void IncrementTaskCount();
  void DecrementTaskCount(bool delete_data_for_origin, const GURL& origin);

  // Keep a reference to FileSystemContext until we are done with it.
  scoped_refptr<storage::FileSystemContext> filesystem_context_;

  const base::Time begin_;
  const base::Time end_;
  const base::Closure callback_;
  int task_count_ = 0;
};

void PluginPrivateDataDeletionHelper::CheckOriginsOnFileTaskRunner(
    const std::set<GURL>& origins) {
  DCHECK(filesystem_context_->default_file_task_runner()
             ->RunsTasksInCurrentSequence());
  IncrementTaskCount();

  base::Callback<void(bool, const GURL&)> decrement_callback =
      base::Bind(&PluginPrivateDataDeletionHelper::DecrementTaskCount,
                 base::Unretained(this));
  storage::AsyncFileUtil* async_file_util =
      filesystem_context_->GetAsyncFileUtil(
          storage::kFileSystemTypePluginPrivate);
  storage::ObfuscatedFileUtil* obfuscated_file_util =
      static_cast<storage::ObfuscatedFileUtil*>(
          static_cast<storage::AsyncFileUtilAdapter*>(async_file_util)
              ->sync_file_util());
  for (const auto& origin : origins) {
    // Determine the available plugin private filesystem directories
    // for this origin.
    base::File::Error error;
    base::FilePath path = obfuscated_file_util->GetDirectoryForOriginAndType(
        origin, "", false, &error);
    if (error != base::File::FILE_OK) {
      DLOG(ERROR) << "Unable to read directory for " << origin;
      continue;
    }

    // Currently the plugin private filesystem is only used by Encrypted
    // Media Content Decryption Modules (CDM), which used to be hosted as pepper
    // plugins. Each CDM gets a directory based on the CdmInfo::file_system_id,
    // e.g. application/x-ppapi-widevine-cdm (same as previous plugin mimetypes
    // to avoid data migration). See https://crbug.com/479923 for the history.
    // Enumerate through the set of directories so that data from any CDM used
    // by this origin is deleted.
    base::FileEnumerator file_enumerator(path, false,
                                         base::FileEnumerator::DIRECTORIES);
    for (base::FilePath plugin_path = file_enumerator.Next();
         !plugin_path.empty(); plugin_path = file_enumerator.Next()) {
      IncrementTaskCount();
      PluginPrivateDataByOriginChecker* helper =
          new PluginPrivateDataByOriginChecker(
              filesystem_context_.get(), origin.GetOrigin(),
              plugin_path.BaseName().MaybeAsASCII(), begin_, end_,
              decrement_callback);
      base::PostTaskWithTraits(
          FROM_HERE, {BrowserThread::IO},
          base::BindOnce(
              &PluginPrivateDataByOriginChecker::CheckFilesOnIOThread,
              base::Unretained(helper)));

      // |helper| will delete itself when it is done.
    }
  }

  // Cancels out the call to IncrementTaskCount() at the start of this method.
  // If there are no origins specified then this will cause this helper to
  // be destroyed.
  DecrementTaskCount(false, GURL());
}

void PluginPrivateDataDeletionHelper::IncrementTaskCount() {
  DCHECK(filesystem_context_->default_file_task_runner()
             ->RunsTasksInCurrentSequence());
  ++task_count_;
}

void PluginPrivateDataDeletionHelper::DecrementTaskCount(
    bool delete_data_for_origin,
    const GURL& origin) {
  DCHECK(filesystem_context_->default_file_task_runner()
             ->RunsTasksInCurrentSequence());

  // Since the PluginPrivateDataByOriginChecker runs on the IO thread,
  // delete all the data for |origin| if needed.
  if (delete_data_for_origin) {
    DCHECK(!origin.is_empty());
    DVLOG(3) << "Deleting plugin data for " << origin;
    storage::FileSystemBackend* backend =
        filesystem_context_->GetFileSystemBackend(
            storage::kFileSystemTypePluginPrivate);
    storage::FileSystemQuotaUtil* quota_util = backend->GetQuotaUtil();
    base::File::Error result = quota_util->DeleteOriginDataOnFileTaskRunner(
        filesystem_context_.get(), nullptr, origin,
        storage::kFileSystemTypePluginPrivate);
    ALLOW_UNUSED_LOCAL(result);
    DLOG_IF(ERROR, result != base::File::FILE_OK)
        << "Unable to delete the plugin data for " << origin;
  }

  DCHECK_GT(task_count_, 0);
  --task_count_;
  if (task_count_)
    return;

  // If there are no more tasks in progress, run |callback_| and then
  // this helper can be deleted.
  callback_.Run();
  delete this;
}

}  // namespace

void ClearPluginPrivateDataOnFileTaskRunner(
    scoped_refptr<storage::FileSystemContext> filesystem_context,
    const GURL& storage_origin,
    const base::Time begin,
    const base::Time end,
    const base::Closure& callback) {
  DCHECK(filesystem_context->default_file_task_runner()
             ->RunsTasksInCurrentSequence());
  DVLOG(3) << "Clearing plugin data for origin: " << storage_origin;

  storage::FileSystemBackend* backend =
      filesystem_context->GetFileSystemBackend(
          storage::kFileSystemTypePluginPrivate);
  storage::FileSystemQuotaUtil* quota_util = backend->GetQuotaUtil();

  // Determine the set of origins used.
  std::set<GURL> origins;
  quota_util->GetOriginsForTypeOnFileTaskRunner(
      storage::kFileSystemTypePluginPrivate, &origins);

  if (origins.empty()) {
    // No origins, so nothing to do.
    callback.Run();
    return;
  }

  // If a specific origin is provided, then check that it is in the list
  // returned and remove all the other origins.
  if (!storage_origin.is_empty()) {
    if (!base::ContainsKey(origins, storage_origin)) {
      // Nothing matches, so nothing to do.
      callback.Run();
      return;
    }

    // List should only contain the one value that matches.
    origins.clear();
    origins.insert(storage_origin);
  }

  PluginPrivateDataDeletionHelper* helper = new PluginPrivateDataDeletionHelper(
      std::move(filesystem_context), begin, end, callback);
  helper->CheckOriginsOnFileTaskRunner(origins);
  // |helper| will delete itself when all origins have been checked.
}

}  // namespace content
