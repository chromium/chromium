// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/test/async_file_test_helper.h"

#include <memory>
#include <utility>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "storage/browser/blob/shareable_file_reference.h"
#include "storage/browser/file_system/copy_or_move_hook_delegate.h"
#include "storage/browser/file_system/file_system_backend.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/file_system/file_system_util.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/common/file_system/file_system_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace storage {

using FileEntryList = FileSystemOperation::FileEntryList;

namespace {

void AssignAndQuit(base::RunLoop* run_loop,
                   base::File::Error* result_out,
                   base::File::Error result) {
  *result_out = result;
  run_loop->Quit();
}

base::OnceCallback<void(base::File::Error)> AssignAndQuitCallback(
    base::RunLoop* run_loop,
    base::File::Error* result) {
  return base::BindOnce(&AssignAndQuit, run_loop, base::Unretained(result));
}

void GetMetadataCallback(base::RunLoop* run_loop,
                         base::File::Error* result_out,
                         base::File::Info* file_info_out,
                         base::File::Error result,
                         const base::File::Info& file_info) {
  *result_out = result;
  if (file_info_out)
    *file_info_out = file_info;
  run_loop->Quit();
}

void CreateSnapshotFileCallback(
    base::RunLoop* run_loop,
    base::File::Error* result_out,
    base::FilePath* platform_path_out,
    base::File::Error result,
    const base::File::Info& file_info,
    const base::FilePath& platform_path,
    scoped_refptr<ShareableFileReference> file_ref) {
  DCHECK(!file_ref.get());
  *result_out = result;
  if (platform_path_out)
    *platform_path_out = platform_path;
  run_loop->Quit();
}

void ReadDirectoryCallback(base::RunLoop* run_loop,
                           base::File::Error* result_out,
                           FileEntryList* entries_out,
                           base::File::Error result,
                           FileEntryList entries,
                           bool has_more) {
  *result_out = result;
  entries_out->insert(entries_out->end(), entries.begin(), entries.end());
  if (result != base::File::FILE_OK || !has_more)
    run_loop->Quit();
}

void DidGetUsageAndQuota(blink::mojom::QuotaStatusCode* status_out,
                         int64_t* usage_out,
                         int64_t* quota_out,
                         base::OnceClosure done_callback,
                         blink::mojom::QuotaStatusCode status,
                         int64_t usage,
                         int64_t quota) {
  if (status_out)
    *status_out = status;
  if (usage_out)
    *usage_out = usage;
  if (quota_out)
    *quota_out = quota;
  if (done_callback)
    std::move(done_callback).Run();
}

}  // namespace

const int64_t AsyncFileTestHelper::kDontCheckSize = -1;

base::File::Error AsyncFileTestHelper::Copy(FileSystemContext* context,
                                            const FileSystemURL& src,
                                            const FileSystemURL& dest) {
  return CopyWithHookDelegate(
      context, src, dest, FileSystemOperation::ERROR_BEHAVIOR_ABORT,
      std::make_unique<storage::CopyOrMoveHookDelegate>());
}

base::File::Error AsyncFileTestHelper::CopyWithHookDelegate(
    FileSystemContext* context,
    const FileSystemURL& src,
    const FileSystemURL& dest,
    FileSystemOperation::ErrorBehavior error_behavior,
    std::unique_ptr<CopyOrMoveHookDelegate> copy_or_move_hook_delegate) {
  base::File::Error result = base::File::FILE_ERROR_FAILED;
  base::RunLoop run_loop;
  context->operation_runner()->Copy(
      src, dest, FileSystemOperation::CopyOrMoveOptionSet(), error_behavior,
      std::move(copy_or_move_hook_delegate),
      AssignAndQuitCallback(&run_loop, &result));
  run_loop.Run();
  return result;
}

base::File::Error AsyncFileTestHelper::CopyFileLocal(
    FileSystemContext* context,
    const FileSystemURL& src,
    const FileSystemURL& dest) {
  base::File::Error result = base::File::FILE_ERROR_FAILED;
  base::RunLoop run_loop;
  context->operation_runner()->CopyFileLocal(
      src, dest, FileSystemOperation::CopyOrMoveOptionSet(),
      FileSystemOperation::CopyFileProgressCallback(),
      AssignAndQuitCallback(&run_loop, &result));
  run_loop.Run();
  return result;
}

base::File::Error AsyncFileTestHelper::Move(FileSystemContext* context,
                                            const FileSystemURL& src,
                                            const FileSystemURL& dest) {
  return MoveWithHookDelegate(
      context, src, dest, FileSystemOperation::ERROR_BEHAVIOR_ABORT,
      std::make_unique<storage::CopyOrMoveHookDelegate>());
}

base::File::Error AsyncFileTestHelper::MoveWithHookDelegate(
    FileSystemContext* context,
    const FileSystemURL& src,
    const FileSystemURL& dest,
    FileSystemOperation::ErrorBehavior error_behavior,
    std::unique_ptr<CopyOrMoveHookDelegate> copy_or_move_hook_delegate) {
  base::File::Error result = base::File::FILE_ERROR_FAILED;
  base::RunLoop run_loop;
  context->operation_runner()->Move(
      src, dest, FileSystemOperation::CopyOrMoveOptionSet(), error_behavior,
      std::move(copy_or_move_hook_delegate),
      AssignAndQuitCallback(&run_loop, &result));
  run_loop.Run();
  return result;
}

base::File::Error AsyncFileTestHelper::MoveFileLocal(
    FileSystemContext* context,
    const FileSystemURL& src,
    const FileSystemURL& dest) {
  base::File::Error result = base::File::FILE_ERROR_FAILED;
  base::RunLoop run_loop;
  context->operation_runner()->MoveFileLocal(
      src, dest, FileSystemOperation::CopyOrMoveOptionSet(),
      AssignAndQuitCallback(&run_loop, &result));
  run_loop.Run();
  return result;
}

base::File::Error AsyncFileTestHelper::Remove(FileSystemContext* context,
                                              const FileSystemURL& url,
                                              bool recursive) {
  base::File::Error result = base::File::FILE_ERROR_FAILED;
  base::RunLoop run_loop;
  context->operation_runner()->Remove(
      url, recursive, AssignAndQuitCallback(&run_loop, &result));
  run_loop.Run();
  return result;
}

base::File::Error AsyncFileTestHelper::ReadDirectory(FileSystemContext* context,
                                                     const FileSystemURL& url,
                                                     FileEntryList* entries) {
  base::File::Error result = base::File::FILE_ERROR_FAILED;
  DCHECK(entries);
  entries->clear();
  base::RunLoop run_loop;
  context->operation_runner()->ReadDirectory(
      url,
      base::BindRepeating(&ReadDirectoryCallback, &run_loop, &result, entries));
  run_loop.Run();
  return result;
}

base::File::Error AsyncFileTestHelper::CreateDirectory(
    FileSystemContext* context,
    const FileSystemURL& url) {
  base::File::Error result = base::File::FILE_ERROR_FAILED;
  base::RunLoop run_loop;
  context->operation_runner()->CreateDirectory(
      url, false /* exclusive */, false /* recursive */,
      AssignAndQuitCallback(&run_loop, &result));
  run_loop.Run();
  return result;
}

base::File::Error AsyncFileTestHelper::CreateFile(FileSystemContext* context,
                                                  const FileSystemURL& url) {
  base::File::Error result = base::File::FILE_ERROR_FAILED;
  base::RunLoop run_loop;
  context->operation_runner()->CreateFile(
      url, false /* exclusive */, AssignAndQuitCallback(&run_loop, &result));
  run_loop.Run();
  return result;
}

base::File::Error AsyncFileTestHelper::CreateFileWithData(
    FileSystemContext* context,
    const FileSystemURL& url,
    std::string_view data) {
  base::ScopedTempDir dir;
  if (!dir.CreateUniqueTempDir())
    return base::File::FILE_ERROR_FAILED;
  base::FilePath local_path = dir.GetPath().AppendASCII("tmp");
  if (!base::WriteFile(local_path, data)) {
    return base::File::FILE_ERROR_FAILED;
  }
  base::File::Error result = base::File::FILE_ERROR_FAILED;
  base::RunLoop run_loop;
  context->operation_runner()->CopyInForeignFile(
      local_path, url, AssignAndQuitCallback(&run_loop, &result));
  run_loop.Run();
  return result;
}

base::File::Error AsyncFileTestHelper::TruncateFile(FileSystemContext* context,
                                                    const FileSystemURL& url,
                                                    size_t size) {
  base::RunLoop run_loop;
  base::File::Error result = base::File::FILE_ERROR_FAILED;
  context->operation_runner()->Truncate(
      url, size, AssignAndQuitCallback(&run_loop, &result));
  run_loop.Run();
  return result;
}

base::File::Error AsyncFileTestHelper::GetMetadata(
    FileSystemContext* context,
    const FileSystemURL& url,
    base::File::Info* file_info) {
  base::File::Error result = base::File::FILE_ERROR_FAILED;
  base::RunLoop run_loop;
  context->operation_runner()->GetMetadata(
      url,
      {storage::FileSystemOperation::GetMetadataField::kIsDirectory,
       storage::FileSystemOperation::GetMetadataField::kSize,
       storage::FileSystemOperation::GetMetadataField::kLastModified},
      base::BindOnce(&GetMetadataCallback, &run_loop, &result, file_info));
  run_loop.Run();
  return result;
}

base::File::Error AsyncFileTestHelper::GetPlatformPath(
    FileSystemContext* context,
    const FileSystemURL& url,
    base::FilePath* platform_path) {
  base::File::Error result = base::File::FILE_ERROR_FAILED;
  base::RunLoop run_loop;
  context->operation_runner()->CreateSnapshotFile(
      url, base::BindOnce(&CreateSnapshotFileCallback, &run_loop, &result,
                          platform_path));
  run_loop.Run();
  return result;
}

bool AsyncFileTestHelper::FileExists(FileSystemContext* context,
                                     const FileSystemURL& url,
                                     int64_t expected_size) {
  base::File::Info file_info;
  base::File::Error result = GetMetadata(context, url, &file_info);
  if (result != base::File::FILE_OK || file_info.is_directory)
    return false;
  return expected_size == kDontCheckSize || file_info.size == expected_size;
}

bool AsyncFileTestHelper::DirectoryExists(FileSystemContext* context,
                                          const FileSystemURL& url) {
  base::File::Info file_info;
  base::File::Error result = GetMetadata(context, url, &file_info);
  return (result == base::File::FILE_OK) && file_info.is_directory;
}

blink::mojom::QuotaStatusCode AsyncFileTestHelper::GetUsageAndQuota(
    QuotaManagerProxy* quota_manager_proxy,
    const blink::StorageKey& storage_key,
    FileSystemType type,
    int64_t* usage,
    int64_t* quota) {
  blink::mojom::QuotaStatusCode status =
      blink::mojom::QuotaStatusCode::kUnknown;
  base::RunLoop run_loop;
  quota_manager_proxy->GetUsageAndQuota(
      storage_key, FileSystemTypeToQuotaStorageType(type),
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce(&DidGetUsageAndQuota, &status, usage, quota,
                     run_loop.QuitWhenIdleClosure()));
  run_loop.Run();
  return status;
}

base::File::Error AsyncFileTestHelper::TouchFile(
    FileSystemContext* context,
    const FileSystemURL& url,
    const base::Time& last_access_time,
    const base::Time& last_modified_time) {
  base::File::Error result = base::File::FILE_ERROR_FAILED;
  base::RunLoop run_loop;
  context->operation_runner()->TouchFile(
      url, last_access_time, last_modified_time,
      AssignAndQuitCallback(&run_loop, &result));
  run_loop.Run();
  return result;
}

}  // namespace storage
