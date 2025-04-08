// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_FILE_OPENER_FOR_UPLOAD_H_
#define SERVICES_NETWORK_FILE_OPENER_FOR_UPLOAD_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/files/file.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "net/base/net_errors.h"

class GURL;

namespace network {
namespace mojom {
class NetworkContextClient;
}  // namespace mojom

// Manages opening a list of files specified by `paths_`. This is used by
// URLLoader to prepare files listed in a ResourceRequestBody for upload. It
// interacts with the NetworkContextClient to request file access and opens
// files in batches to avoid overwhelming the system.
class FileOpenerForUpload {
 public:
  // Callback type invoked when all files have been processed (successfully or
  // with an error).
  using SetUpUploadCallback = base::OnceCallback<void(
      base::expected<std::vector<base::File>, net::Error>)>;

  // Maximum number of file open requests sent to the NetworkContextClient
  // in a single batch via OnFileUploadRequested.
  static constexpr size_t kMaxFileUploadRequestsPerBatch = 64;

  // Constructor initiates the file opening process.
  FileOpenerForUpload(std::vector<base::FilePath> paths,
                      const GURL& url,
                      int32_t process_id,
                      mojom::NetworkContextClient* const network_context_client,
                      SetUpUploadCallback set_up_upload_callback);

  FileOpenerForUpload(const FileOpenerForUpload&) = delete;
  FileOpenerForUpload& operator=(const FileOpenerForUpload&) = delete;

  ~FileOpenerForUpload();

  // Starts the file opening operations.
  void Start();

 private:
  static void OnFilesForUploadOpened(
      base::WeakPtr<FileOpenerForUpload> file_opener,
      size_t num_files_requested,
      int error_code,
      std::vector<base::File> opened_files);

  // Initiates the process of opening the next batch of files from `paths_`.
  void StartOpeningNextBatch();

  // Called when all batches have been processed or an error occurred.
  // Invokes the final `set_up_upload_callback_`.
  void FilesForUploadOpenedDone(int error_code);

  // The list of file paths requested to be opened.
  const std::vector<base::FilePath> paths_;
  // The URL associated with the upload request (used in OnFileUploadRequested).
  // Stored as a raw_ref as the GURL's lifetime is expected to exceed this
  // object.
  const raw_ref<const GURL> url_;
  const int32_t process_id_;
  const raw_ptr<mojom::NetworkContextClient> network_context_client_;
  SetUpUploadCallback set_up_upload_callback_;
  // The files opened so far.
  std::vector<base::File> opened_files_;

  base::WeakPtrFactory<FileOpenerForUpload> weak_ptr_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_FILE_OPENER_FOR_UPLOAD_H_
