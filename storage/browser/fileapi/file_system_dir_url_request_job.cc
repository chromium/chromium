// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/fileapi/file_system_dir_url_request_job.h"

#include <stddef.h>

#include <algorithm>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/services/filesystem/public/interfaces/types.mojom.h"
#include "net/base/directory_listing.h"
#include "net/base/io_buffer.h"
#include "net/url_request/url_request.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "storage/browser/fileapi/file_system_operation_runner.h"
#include "storage/common/fileapi/file_system_util.h"
#include "url/gurl.h"

using net::NetworkDelegate;
using net::URLRequest;
using net::URLRequestJob;
using net::URLRequestStatus;

namespace storage {

FileSystemDirURLRequestJob::FileSystemDirURLRequestJob(
    URLRequest* request,
    NetworkDelegate* network_delegate,
    const std::string& storage_domain,
    FileSystemContext* file_system_context)
    : URLRequestJob(request, network_delegate),
      storage_domain_(storage_domain),
      file_system_context_(file_system_context),
      weak_factory_(this) {
}

FileSystemDirURLRequestJob::~FileSystemDirURLRequestJob() = default;

int FileSystemDirURLRequestJob::ReadRawData(net::IOBuffer* dest,
                                            int dest_size) {
  int count = std::min(dest_size, base::checked_cast<int>(data_.size()));
  if (count > 0) {
    memcpy(dest->data(), data_.data(), count);
    data_.erase(0, count);
  }
  return count;
}

void FileSystemDirURLRequestJob::Start() {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&FileSystemDirURLRequestJob::StartAsync,
                                weak_factory_.GetWeakPtr()));
}

void FileSystemDirURLRequestJob::Kill() {
  URLRequestJob::Kill();
  weak_factory_.InvalidateWeakPtrs();
}

bool FileSystemDirURLRequestJob::GetMimeType(std::string* mime_type) const {
  *mime_type = "text/html";
  return true;
}

bool FileSystemDirURLRequestJob::GetCharset(std::string* charset) {
  *charset = "utf-8";
  return true;
}

void FileSystemDirURLRequestJob::StartAsync() {
  if (!request_)
    return;
  url_ = file_system_context_->CrackURL(request_->url());
  if (!url_.is_valid()) {
    const FileSystemRequestInfo request_info = {request_->url(), request_,
                                                storage_domain_, 0};
    file_system_context_->AttemptAutoMountForURLRequest(
        request_info,
        base::BindOnce(&FileSystemDirURLRequestJob::DidAttemptAutoMount,
                       weak_factory_.GetWeakPtr()));
    return;
  }
  if (!file_system_context_->CanServeURLRequest(url_)) {
    // In incognito mode the API is not usable and there should be no data.
    if (url_.is_valid() && VirtualPath::IsRootPath(url_.virtual_path())) {
      // Return an empty directory if the filesystem root is queried.
      DidReadDirectory(base::File::FILE_OK,
                       std::vector<filesystem::mojom::DirectoryEntry>(), false);
      return;
    }
    NotifyStartError(URLRequestStatus::FromError(net::ERR_FILE_NOT_FOUND));
    return;
  }
  file_system_context_->operation_runner()->ReadDirectory(
      url_, base::BindRepeating(&FileSystemDirURLRequestJob::DidReadDirectory,
                                weak_factory_.GetWeakPtr()));
}

void FileSystemDirURLRequestJob::DidAttemptAutoMount(base::File::Error result) {
  if (result >= 0 &&
      file_system_context_->CrackURL(request_->url()).is_valid()) {
    StartAsync();
  } else {
    NotifyStartError(URLRequestStatus::FromError(net::ERR_FILE_NOT_FOUND));
  }
}

void FileSystemDirURLRequestJob::DidReadDirectory(
    base::File::Error result,
    std::vector<filesystem::mojom::DirectoryEntry> entries,
    bool has_more) {
  if (result != base::File::FILE_OK) {
    int rv = net::ERR_FILE_NOT_FOUND;
    if (result == base::File::FILE_ERROR_INVALID_URL)
      rv = net::ERR_INVALID_URL;
    NotifyStartError(URLRequestStatus::FromError(rv));
    return;
  }

  if (!request_)
    return;

  if (data_.empty()) {
    base::FilePath relative_path = url_.path();
#if defined(OS_POSIX)
    relative_path =
        base::FilePath(FILE_PATH_LITERAL("/") + relative_path.value());
#endif
    const base::string16& title = relative_path.LossyDisplayName();
    data_.append(net::GetDirectoryListingHeader(title));
  }

  entries_.insert(entries_.end(), entries.begin(), entries.end());

  if (!has_more) {
    if (entries_.size()) {
      GetMetadata(0);
    } else {
      set_expected_content_size(data_.size());
      NotifyHeadersComplete();
    }
  }
}

void FileSystemDirURLRequestJob::GetMetadata(size_t index) {
  const filesystem::mojom::DirectoryEntry& entry = entries_[index];
  const FileSystemURL url = file_system_context_->CreateCrackedFileSystemURL(
      url_.origin(), url_.type(),
      url_.path().Append(base::FilePath(entry.name)));
  DCHECK(url.is_valid());
  file_system_context_->operation_runner()->GetMetadata(
      url,
      FileSystemOperation::GET_METADATA_FIELD_SIZE |
          FileSystemOperation::GET_METADATA_FIELD_LAST_MODIFIED,
      base::BindOnce(&FileSystemDirURLRequestJob::DidGetMetadata,
                     weak_factory_.GetWeakPtr(), index));
}

void FileSystemDirURLRequestJob::DidGetMetadata(
    size_t index,
    base::File::Error result,
    const base::File::Info& file_info) {
  if (result != base::File::FILE_OK) {
    int rv = net::ERR_FILE_NOT_FOUND;
    if (result == base::File::FILE_ERROR_INVALID_URL)
      rv = net::ERR_INVALID_URL;
    NotifyStartError(URLRequestStatus(URLRequestStatus::FAILED, rv));
  }

  if (!request_)
    return;

  const filesystem::mojom::DirectoryEntry& entry = entries_[index];
  const base::string16& name = base::FilePath(entry.name).LossyDisplayName();
  data_.append(net::GetDirectoryListingEntry(
      name, std::string(),
      entry.type == filesystem::mojom::FsFileType::DIRECTORY, file_info.size,
      file_info.last_modified));

  if (index < entries_.size() - 1) {
    GetMetadata(index + 1);
  } else {
    set_expected_content_size(data_.size());
    NotifyHeadersComplete();
  }
}

}  // namespace storage
