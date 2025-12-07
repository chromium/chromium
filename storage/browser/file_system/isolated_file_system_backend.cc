// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/isolated_file_system_backend.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "storage/browser/file_system/async_file_util_adapter.h"
#include "storage/browser/file_system/copy_or_move_file_validator.h"
#include "storage/browser/file_system/dragged_file_util.h"
#include "storage/browser/file_system/file_stream_reader.h"
#include "storage/browser/file_system/file_stream_writer.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation.h"
#include "storage/browser/file_system/file_system_operation_context.h"
#include "storage/browser/file_system/isolated_context.h"
#include "storage/browser/file_system/native_file_util.h"
#include "storage/browser/file_system/transient_file_util.h"
#include "storage/browser/file_system/watcher_manager.h"
#include "storage/common/file_system/file_system_types.h"
#include "storage/common/file_system/file_system_util.h"

namespace storage {

IsolatedFileSystemBackend::IsolatedFileSystemBackend(
    bool use_for_type_native_local,
    bool use_for_type_platform_app)
    : use_for_type_native_local_(use_for_type_native_local),
      use_for_type_platform_app_(use_for_type_platform_app),
      isolated_file_util_(std::make_unique<AsyncFileUtilAdapter>(
          std::make_unique<LocalFileUtil>())),
      dragged_file_util_(std::make_unique<AsyncFileUtilAdapter>(
          std::make_unique<DraggedFileUtil>())),
      transient_file_util_(std::make_unique<AsyncFileUtilAdapter>(
          std::make_unique<TransientFileUtil>())) {}

IsolatedFileSystemBackend::~IsolatedFileSystemBackend() = default;

bool IsolatedFileSystemBackend::CanHandleType(FileSystemType type) const {
  switch (type) {
    case kFileSystemTypeIsolated:
    case kFileSystemTypeDragged:
    case kFileSystemTypeForTransientFile:
      return true;
    case kFileSystemTypeLocal:
      return use_for_type_native_local_;
    case kFileSystemTypeLocalForPlatformApp:
      return use_for_type_platform_app_;
    default:
      return false;
  }
}

void IsolatedFileSystemBackend::Initialize(FileSystemContext* context) {}

void IsolatedFileSystemBackend::ResolveURL(const FileSystemURL& url,
                                           OpenFileSystemMode mode,
                                           ResolveURLCallback callback) {
  // We never allow opening a new isolated FileSystem via usual ResolveURL.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), GURL(), std::string(),
                                base::File::FILE_ERROR_SECURITY));
}

AsyncFileUtil* IsolatedFileSystemBackend::GetAsyncFileUtil(
    FileSystemType type) {
  switch (type) {
    case kFileSystemTypeLocal:
      return isolated_file_util_.get();
    case kFileSystemTypeDragged:
      return dragged_file_util_.get();
    case kFileSystemTypeForTransientFile:
      return transient_file_util_.get();
    default:
      NOTREACHED();
  }
}

WatcherManager* IsolatedFileSystemBackend::GetWatcherManager(
    FileSystemType type) {
  return nullptr;
}

CopyOrMoveFileValidatorFactory*
IsolatedFileSystemBackend::GetCopyOrMoveFileValidatorFactory(
    FileSystemType type,
    base::File::Error* error_code) {
  DCHECK(error_code);
  *error_code = base::File::FILE_OK;
  return nullptr;
}

std::unique_ptr<FileSystemOperation>
IsolatedFileSystemBackend::CreateFileSystemOperation(
    OperationType type,
    const FileSystemURL& url,
    FileSystemContext* context,
    base::File::Error* error_code) const {
  return FileSystemOperation::Create(
      type, url, context,
      std::make_unique<FileSystemOperationContext>(context));
}

bool IsolatedFileSystemBackend::SupportsStreaming(
    const FileSystemURL& url) const {
  return false;
}

bool IsolatedFileSystemBackend::HasInplaceCopyImplementation(
    FileSystemType type) const {
  DCHECK(type == kFileSystemTypeLocal || type == kFileSystemTypeDragged ||
         type == kFileSystemTypeForTransientFile);
  return false;
}

std::unique_ptr<FileStreamReader>
IsolatedFileSystemBackend::CreateFileStreamReader(
    const FileSystemURL& url,
    int64_t offset,
    int64_t max_bytes_to_read,
    const base::Time& expected_modification_time,
    FileSystemContext* context,
    file_access::ScopedFileAccessDelegate::RequestFilesAccessIOCallback
        file_access) const {
  return FileStreamReader::CreateForLocalFile(
      context->default_file_task_runner(), url.path(), offset,
      expected_modification_time, std::move(file_access));
}

std::unique_ptr<FileStreamWriter>
IsolatedFileSystemBackend::CreateFileStreamWriter(
    const FileSystemURL& url,
    int64_t offset,
    FileSystemContext* context) const {
  return FileStreamWriter::CreateForLocalFile(
      context->default_file_task_runner(), url.path(), offset,
      FileStreamWriter::OPEN_EXISTING_FILE);
}

FileSystemQuotaUtil* IsolatedFileSystemBackend::GetQuotaUtil() {
  // No quota support.
  return nullptr;
}

const UpdateObserverList* IsolatedFileSystemBackend::GetUpdateObservers(
    FileSystemType type) const {
  return nullptr;
}

const ChangeObserverList* IsolatedFileSystemBackend::GetChangeObservers(
    FileSystemType type) const {
  return nullptr;
}

const AccessObserverList* IsolatedFileSystemBackend::GetAccessObservers(
    FileSystemType type) const {
  return nullptr;
}

}  // namespace storage
