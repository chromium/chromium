// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/file_opener_for_upload.h"

#include "base/task/thread_pool.h"
#include "net/base/net_errors.h"
#include "services/network/public/mojom/network_context_client.mojom.h"

namespace network {

namespace {

// `opened_files` need to be closed on a blocking task runner, so move the
// `opened_files` vector onto a sequence that can block so it gets destroyed
// there.
void PostCloseFiles(std::vector<base::File> opened_files) {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::DoNothingWithBoundArgs(std::move(opened_files)));
}

}  // namespace

FileOpenerForUpload::FileOpenerForUpload(
    std::vector<base::FilePath> paths,
    const GURL& url,
    int32_t process_id,
    mojom::NetworkContextClient* const network_context_client,
    SetUpUploadCallback set_up_upload_callback)
    : paths_(std::move(paths)),
      url_(url),
      process_id_(process_id),
      network_context_client_(network_context_client),
      set_up_upload_callback_(std::move(set_up_upload_callback)) {}

FileOpenerForUpload::~FileOpenerForUpload() {
  if (!opened_files_.empty()) {
    PostCloseFiles(std::move(opened_files_));
  }
}

void FileOpenerForUpload::Start() {
  opened_files_.reserve(paths_.size());
  StartOpeningNextBatch();
}

// static
void FileOpenerForUpload::OnFilesForUploadOpened(
    base::WeakPtr<FileOpenerForUpload> file_opener,
    size_t num_files_requested,
    int error_code,
    std::vector<base::File> opened_files) {
  if (!file_opener) {
    PostCloseFiles(std::move(opened_files));
    return;
  }

  if (error_code == net::OK && num_files_requested != opened_files.size()) {
    error_code = net::ERR_FAILED;
  }

  if (error_code != net::OK) {
    PostCloseFiles(std::move(opened_files));
    file_opener->FilesForUploadOpenedDone(error_code);
    return;
  }
  file_opener->opened_files_.insert(
      file_opener->opened_files_.end(),
      std::make_move_iterator(opened_files.begin()),
      std::make_move_iterator(opened_files.end()));

  if (file_opener->opened_files_.size() < file_opener->paths_.size()) {
    file_opener->StartOpeningNextBatch();
    return;
  }

  file_opener->FilesForUploadOpenedDone(net::OK);
}

void FileOpenerForUpload::StartOpeningNextBatch() {
  size_t num_files_to_request = std::min(paths_.size() - opened_files_.size(),
                                         kMaxFileUploadRequestsPerBatch);
  std::vector<base::FilePath> batch_paths(
      paths_.begin() + opened_files_.size(),
      paths_.begin() + opened_files_.size() + num_files_to_request);

  network_context_client_->OnFileUploadRequested(
      process_id_, /*async=*/true, batch_paths, url_.get(),
      base::BindOnce(&FileOpenerForUpload::OnFilesForUploadOpened,
                     weak_ptr_factory_.GetWeakPtr(), num_files_to_request));
}

void FileOpenerForUpload::FilesForUploadOpenedDone(int error_code) {
  if (error_code == net::OK) {
    std::move(set_up_upload_callback_).Run(std::move(opened_files_));
  } else {
    std::move(set_up_upload_callback_)
        .Run(base::unexpected(static_cast<net::Error>(error_code)));
  }
}

}  // namespace network
