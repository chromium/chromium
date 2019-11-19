// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/test/async_file_test_helper.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "storage/browser/blob/shareable_file_reference.h"
#include "storage/browser/file_system/file_system_backend.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/common/file_system/file_system_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

using FileEntryList = storage::FileSystemOperation::FileEntryList;

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
    scoped_refptr<storage::ShareableFileReference> file_ref) {
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

base::File::Error AsyncFileTestHelper::Copy(
    storage::FileSystemContext* context,
    const storage::FileSystemURL& src,
    const storage::FileSystemURL& dest) {
  return CopyWithProgress(context, src, dest, CopyProgressCallback());
}

base::File::Error AsyncFileTestHelper::CopyWithProgress(
    storage::FileSystemContext* context,
    const storage::FileSystemURL& src,
    const storage::FileSystemURL& dest,
    const CopyProgressCallback& progress_callback) {
  base::File::Error result = base::File::FILE_ERROR_FAILED;
  base::RunLoop run_loop;
  context->operation_runner()->Copy(
      src, dest, storage::FileSystemOperation::OPTION_NONE,
      storage::FileSystemOperation::ERROR_BEHAVIOR_ABORT, progress_callback,
      AssignAndQuitCallback(&run_loop, &result));
  run_loop.Run();
  return result;
}

base::File::Error AsyncFileTestHelper::Move(
    storage::FileSystemContext* context,
    const storage::FileSystemURL& src,
    const storage::FileSystemURL& dest) {
  base::File::Error result = base::File::FILE_ERROR_FAILED;
  base::RunLoop run_loop;
  context->operation_runner()->Move(src, dest,
                                    storage::FileSystemOperation::OPTION_NONE,
                                    AssignAndQuitCallback(&run_loop, &result));
  run_loop.Run();
  return result;
}

base::File::Error AsyncFileTestHelper::Remove(
    storage::FileSystemContext* context,
    const storage::FileSystemURL& url,
    bool recursive) {
  base::File::Error result = base::File::FILE_ERROR_FAILED;
  base::RunLoop run_loop;
  context->operation_runner()->Remove(
      url, recursive, AssignAndQuitCallback(&run_loop, &result));
  run_loop.Run();
  return result;
}

base::File::Error AsyncFileTestHelper::ReadDirectory(
    storage::FileSystemContext* context,
    const storage::FileSystemURL& url,
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
    storage::FileSystemContext* context,
    const storage::FileSystemURL& url) {
  base::File::Error result = base::File::FILE_ERROR_FAILED;
  base::RunLoop run_loop;
  context->operation_runner()->CreateDirectory(
      url, false /* exclusive */, false /* recursive */,
      AssignAndQuitCallback(&run_loop, &result));
  run_loop.Run();
  return result;
}

base::File::Error AsyncFileTestHelper::CreateFile(
    storage::FileSystemContext* context,
    const storage::FileSystemURL& url) {
  base::File::Error result = base::File::FILE_ERROR_FAILED;
  base::RunLoop run_loop;
  context->operation_runner()->CreateFile(
      url, false /* exclusive */, AssignAndQuitCallback(&run_loop, &result));
  run_loop.Run();
  return result;
}

base::File::Error AsyncFileTestHelper::CreateFileWithData(
    storage::FileSystemContext* context,
    const storage::FileSystemURL& url,
    const char* buf,
    int buf_size) {
  base::ScopedTempDir dir;
  if (!dir.CreateUniqueTempDir())
    return base::File::FILE_ERROR_FAILED;
  base::FilePath local_path = dir.GetPath().AppendASCII("tmp");
  if (buf_size != base::WriteFile(local_path, buf, buf_size))
    return base::File::FILE_ERROR_FAILED;
  base::File::Error result = base::File::FILE_ERROR_FAILED;
  base::RunLoop run_loop;
  context->operation_runner()->CopyInForeignFile(
      local_path, url, AssignAndQuitCallback(&run_loop, &result));
  run_loop.Run();
  return result;
}

base::File::Error AsyncFileTestHelper::TruncateFile(
    storage::FileSystemContext* context,
    const storage::FileSystemURL& url,
    size_t size) {
  base::RunLoop run_loop;
  base::File::Error result = base::File::FILE_ERROR_FAILED;
  context->operation_runner()->Truncate(
      url, size, AssignAndQuitCallback(&run_loop, &result));
  run_loop.Run();
  return result;
}

base::File::Error AsyncFileTestHelper::GetMetadata(
    storage::FileSystemContext* context,
    const storage::FileSystemURL& url,
    base::File::Info* file_info) {
  base::File::Error result = base::File::FILE_ERROR_FAILED;
  base::RunLoop run_loop;
  context->operation_runner()->GetMetadata(
      url,
      storage::FileSystemOperation::GET_METADATA_FIELD_IS_DIRECTORY |
          storage::FileSystemOperation::GET_METADATA_FIELD_SIZE |
          storage::FileSystemOperation::GET_METADATA_FIELD_LAST_MODIFIED,
      base::BindOnce(&GetMetadataCallback, &run_loop, &result, file_info));
  run_loop.Run();
  return result;
}

base::File::Error AsyncFileTestHelper::GetPlatformPath(
    storage::FileSystemContext* context,
    const storage::FileSystemURL& url,
    base::FilePath* platform_path) {
  base::File::Error result = base::File::FILE_ERROR_FAILED;
  base::RunLoop run_loop;
  context->operation_runner()->CreateSnapshotFile(
      url, base::BindOnce(&CreateSnapshotFileCallback, &run_loop, &result,
                          platform_path));
  run_loop.Run();
  return result;
}

bool AsyncFileTestHelper::FileExists(storage::FileSystemContext* context,
                                     const storage::FileSystemURL& url,
                                     int64_t expected_size) {
  base::File::Info file_info;
  base::File::Error result = GetMetadata(context, url, &file_info);
  if (result != base::File::FILE_OK || file_info.is_directory)
    return false;
  return expected_size == kDontCheckSize || file_info.size == expected_size;
}

bool AsyncFileTestHelper::DirectoryExists(storage::FileSystemContext* context,
                                          const storage::FileSystemURL& url) {
  base::File::Info file_info;
  base::File::Error result = GetMetadata(context, url, &file_info);
  return (result == base::File::FILE_OK) && file_info.is_directory;
}

blink::mojom::QuotaStatusCode AsyncFileTestHelper::GetUsageAndQuota(
    storage::QuotaManager* quota_manager,
    const url::Origin& origin,
    storage::FileSystemType type,
    int64_t* usage,
    int64_t* quota) {
  blink::mojom::QuotaStatusCode status =
      blink::mojom::QuotaStatusCode::kUnknown;
  base::RunLoop run_loop;
  quota_manager->GetUsageAndQuota(
      origin, FileSystemTypeToQuotaStorageType(type),
      base::BindOnce(&DidGetUsageAndQuota, &status, usage, quota,
                     run_loop.QuitWhenIdleClosure()));
  run_loop.Run();
  return status;
}

}  // namespace content
