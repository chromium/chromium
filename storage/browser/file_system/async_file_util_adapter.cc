// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/async_file_util_adapter.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "components/services/filesystem/public/mojom/types.mojom.h"
#include "storage/browser/blob/shareable_file_reference.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_file_util.h"
#include "storage/browser/file_system/file_system_operation_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/common/file_system/file_system_util.h"

using base::Unretained;

namespace storage {

namespace {

// TODO(crbug.com/492450476): This should actually return base::expected.
std::tuple<base::File::Error, bool> EnsureFileExistsHelper(
    FileSystemFileUtil& file_util,
    std::unique_ptr<FileSystemOperationContext> context,
    const FileSystemURL& url) {
  bool created = false;
  base::File::Error error =
      file_util.EnsureFileExists(context.get(), url, &created);
  return {error, created};
}

std::tuple<base::File::Error, base::File::Info> GetFileInfoHelper(
    FileSystemFileUtil& file_util,
    std::unique_ptr<FileSystemOperationContext> context,
    const FileSystemURL& url,
    bool calculate_total_size) {
  base::File::Info file_info;
  base::FilePath platform_path;
  base::File::Error error =
      file_util.GetFileInfo(context.get(), url, &file_info, &platform_path);
  if (error == base::File::FILE_OK && calculate_total_size &&
      file_info.is_directory) {
    file_info.size = 0;
    std::unique_ptr<FileSystemFileUtil::AbstractFileEnumerator> enumerator =
        file_util.CreateFileEnumerator(context.get(), url, true);
    base::FilePath path = enumerator->Next();
    while (!path.empty()) {
      if (!enumerator->IsDirectory()) {
        file_info.size += enumerator->Size();
      }
      path = enumerator->Next();
    }
  }
  return {error, file_info};
}

std::tuple<base::File::Error,
           base::File::Info,
           base::FilePath,
           scoped_refptr<ShareableFileReference>>
CreateSnapshotFileHelper(FileSystemFileUtil& file_util,
                         std::unique_ptr<FileSystemOperationContext> context,
                         const FileSystemURL& url) {
  base::File::Error error;
  base::File::Info file_info;
  base::FilePath platform_path;
  ScopedFile scoped_file = file_util.CreateSnapshotFile(
      context.get(), url, &error, &file_info, &platform_path);
  return {error, file_info, platform_path,
          ShareableFileReference::GetOrCreate(std::move(scoped_file))};
}

void ReadDirectoryHelper(FileSystemFileUtil& file_util,
                         std::unique_ptr<FileSystemOperationContext> context,
                         const FileSystemURL& url,
                         AsyncFileUtil::ReadDirectoryCallback callback) {
  base::File::Info file_info;
  base::FilePath platform_path;
  base::File::Error error =
      file_util.GetFileInfo(context.get(), url, &file_info, &platform_path);

  if (error == base::File::FILE_OK && !file_info.is_directory)
    error = base::File::FILE_ERROR_NOT_A_DIRECTORY;

  std::vector<filesystem::mojom::DirectoryEntry> entries;
  if (error != base::File::FILE_OK) {
    callback.Run(error, std::move(entries), false /* has_more */);
    return;
  }

  // Note: Increasing this value may make some tests in LayoutTests meaningless.
  // (Namely, read-directory-many.html and read-directory-sync-many.html are
  // assuming that they are reading much more entries than this constant.)
  const size_t kResultChunkSize = 100;

  std::unique_ptr<FileSystemFileUtil::AbstractFileEnumerator> file_enum(
      file_util.CreateFileEnumerator(context.get(), url, false));

  base::FilePath current;
  while (!(current = file_enum->Next()).empty()) {
    auto name = base::SafeBaseName::Create(current);
    CHECK(name) << current;
    entries.emplace_back(*name, file_enum->GetName().AsUTF8Unsafe(),
                         file_enum->IsDirectory()
                             ? filesystem::mojom::FsFileType::DIRECTORY
                             : filesystem::mojom::FsFileType::REGULAR_FILE);

    if (entries.size() == kResultChunkSize) {
      callback.Run(base::File::FILE_OK, std::move(entries),
                   true /* has_more */);
      entries.clear();
    }
  }

  error = file_enum->GetError();
  if ((error != base::File::FILE_OK) && !entries.empty()) {
    callback.Run(base::File::FILE_OK, std::move(entries), true /* has_more */);
    entries.clear();
  }
  callback.Run(error, std::move(entries), false /* has_more */);
}

void RunCreateOrOpenCallback(
    std::unique_ptr<FileSystemOperationContext> context,
    AsyncFileUtil::CreateOrOpenCallback callback,
    base::File file) {
  if (callback.IsCancelled()) {
    // If |callback| been cancelled, free |file| on the correct task runner.
    context->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce([](base::File file) { file.Close(); }, std::move(file)));
    return;
  }

  std::move(callback).Run(std::move(file), base::OnceClosure());
}

}  // namespace

AsyncFileUtilAdapter::AsyncFileUtilAdapter(
    std::unique_ptr<FileSystemFileUtil> sync_file_util)
    : sync_file_util_(std::move(sync_file_util)) {
  DCHECK(sync_file_util_.get());
}

AsyncFileUtilAdapter::~AsyncFileUtilAdapter() = default;

void AsyncFileUtilAdapter::CreateOrOpen(
    std::unique_ptr<FileSystemOperationContext> context,
    const FileSystemURL& url,
    uint32_t file_flags,
    CreateOrOpenCallback callback) {
  scoped_refptr<base::SequencedTaskRunner> task_runner = context->task_runner();
  FileSystemOperationContext* context_ptr = context.get();
  task_runner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FileSystemFileUtil::CreateOrOpen,
                     Unretained(sync_file_util_.get()), context_ptr, url,
                     file_flags),
      base::BindOnce(&RunCreateOrOpenCallback, std::move(context),
                     std::move(callback)));
}

void AsyncFileUtilAdapter::EnsureFileExists(
    std::unique_ptr<FileSystemOperationContext> context,
    const FileSystemURL& url,
    EnsureFileExistsCallback callback) {
  scoped_refptr<base::SequencedTaskRunner> task_runner = context->task_runner();
  const bool success = task_runner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&EnsureFileExistsHelper, std::ref(*sync_file_util_),
                     std::move(context), url),
      std::move(callback));
  DCHECK(success);
}

void AsyncFileUtilAdapter::CreateDirectory(
    std::unique_ptr<FileSystemOperationContext> context,
    const FileSystemURL& url,
    bool exclusive,
    bool recursive,
    StatusCallback callback) {
  scoped_refptr<base::SequencedTaskRunner> task_runner = context->task_runner();
  const bool success = task_runner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FileSystemFileUtil::CreateDirectory,
                     Unretained(sync_file_util_.get()),
                     base::Owned(std::move(context)), url, exclusive,
                     recursive),
      std::move(callback));
  DCHECK(success);
}

void AsyncFileUtilAdapter::GetFileInfo(
    std::unique_ptr<FileSystemOperationContext> context,
    const FileSystemURL& url,
    GetMetadataFieldSet fields,
    GetFileInfoCallback callback) {
  bool calculate_total_size =
      fields.Has(FileSystemOperation::GetMetadataField::kRecursiveSize);
  scoped_refptr<base::SequencedTaskRunner> task_runner = context->task_runner();
  const bool success = task_runner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&GetFileInfoHelper, std::ref(*sync_file_util_),
                     std::move(context), url, calculate_total_size),
      std::move(callback));
  DCHECK(success);
}

void AsyncFileUtilAdapter::ReadDirectory(
    std::unique_ptr<FileSystemOperationContext> context,
    const FileSystemURL& url,
    ReadDirectoryCallback callback) {
  scoped_refptr<base::SequencedTaskRunner> task_runner = context->task_runner();
  const bool success = task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&ReadDirectoryHelper, std::ref(*sync_file_util_),
                     std::move(context), url,
                     base::BindPostTaskToCurrentDefault(std::move(callback))));
  DCHECK(success);
}

void AsyncFileUtilAdapter::Touch(
    std::unique_ptr<FileSystemOperationContext> context,
    const FileSystemURL& url,
    const base::Time& last_access_time,
    const base::Time& last_modified_time,
    StatusCallback callback) {
  scoped_refptr<base::SequencedTaskRunner> task_runner = context->task_runner();
  const bool success = task_runner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FileSystemFileUtil::Touch,
                     Unretained(sync_file_util_.get()),
                     base::Owned(std::move(context)), url, last_access_time,
                     last_modified_time),
      std::move(callback));
  DCHECK(success);
}

void AsyncFileUtilAdapter::Truncate(
    std::unique_ptr<FileSystemOperationContext> context,
    const FileSystemURL& url,
    int64_t length,
    StatusCallback callback) {
  scoped_refptr<base::SequencedTaskRunner> task_runner = context->task_runner();
  const bool success = task_runner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FileSystemFileUtil::Truncate,
                     Unretained(sync_file_util_.get()),
                     base::Owned(std::move(context)), url, length),
      std::move(callback));
  DCHECK(success);
}

void AsyncFileUtilAdapter::CopyFileLocal(
    std::unique_ptr<FileSystemOperationContext> context,
    const FileSystemURL& src_url,
    const FileSystemURL& dest_url,
    CopyOrMoveOptionSet options,
    CopyFileProgressCallback progress_callback,
    StatusCallback callback) {
  // TODO(hidehiko): Support progress_callback.
  scoped_refptr<base::SequencedTaskRunner> task_runner = context->task_runner();
  const bool success = task_runner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FileSystemFileUtil::CopyOrMoveFile,
                     Unretained(sync_file_util_.get()),
                     base::Owned(std::move(context)), src_url, dest_url,
                     options, true /* copy */),
      std::move(callback));
  DCHECK(success);
}

void AsyncFileUtilAdapter::MoveFileLocal(
    std::unique_ptr<FileSystemOperationContext> context,
    const FileSystemURL& src_url,
    const FileSystemURL& dest_url,
    CopyOrMoveOptionSet options,
    StatusCallback callback) {
  scoped_refptr<base::SequencedTaskRunner> task_runner = context->task_runner();
  const bool success = task_runner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FileSystemFileUtil::CopyOrMoveFile,
                     Unretained(sync_file_util_.get()),
                     base::Owned(std::move(context)), src_url, dest_url,
                     options, false /* copy */),
      std::move(callback));
  DCHECK(success);
}

void AsyncFileUtilAdapter::CopyInForeignFile(
    std::unique_ptr<FileSystemOperationContext> context,
    const base::FilePath& src_file_path,
    const FileSystemURL& dest_url,
    StatusCallback callback) {
  scoped_refptr<base::SequencedTaskRunner> task_runner = context->task_runner();
  const bool success = task_runner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FileSystemFileUtil::CopyInForeignFile,
                     Unretained(sync_file_util_.get()),
                     base::Owned(std::move(context)), src_file_path, dest_url),
      std::move(callback));
  DCHECK(success);
}

void AsyncFileUtilAdapter::DeleteFile(
    std::unique_ptr<FileSystemOperationContext> context,
    const FileSystemURL& url,
    StatusCallback callback) {
  scoped_refptr<base::SequencedTaskRunner> task_runner = context->task_runner();
  const bool success = task_runner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FileSystemFileUtil::DeleteFile,
                     Unretained(sync_file_util_.get()),
                     base::Owned(std::move(context)), url),
      std::move(callback));
  DCHECK(success);
}

void AsyncFileUtilAdapter::DeleteDirectory(
    std::unique_ptr<FileSystemOperationContext> context,
    const FileSystemURL& url,
    StatusCallback callback) {
  scoped_refptr<base::SequencedTaskRunner> task_runner = context->task_runner();
  const bool success = task_runner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FileSystemFileUtil::DeleteDirectory,
                     Unretained(sync_file_util_.get()),
                     base::Owned(std::move(context)), url),
      std::move(callback));
  DCHECK(success);
}

void AsyncFileUtilAdapter::DeleteRecursively(
    std::unique_ptr<FileSystemOperationContext> context,
    const FileSystemURL& url,
    StatusCallback callback) {
  std::move(callback).Run(base::File::FILE_ERROR_INVALID_OPERATION);
}

void AsyncFileUtilAdapter::CreateSnapshotFile(
    std::unique_ptr<FileSystemOperationContext> context,
    const FileSystemURL& url,
    CreateSnapshotFileCallback callback) {
  scoped_refptr<base::SequencedTaskRunner> task_runner = context->task_runner();
  const bool success = task_runner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&CreateSnapshotFileHelper, std::ref(*sync_file_util_),
                     std::move(context), url),
      std::move(callback));
  DCHECK(success);
}

}  // namespace storage
