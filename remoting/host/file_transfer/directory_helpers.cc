// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/file_transfer/directory_helpers.h"

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "remoting/protocol/file_transfer_helpers.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/common/chrome_paths.h"
#endif

namespace remoting {

namespace {

#if BUILDFLAG(IS_CHROMEOS)
protocol::FileTransferResult<base::FilePath> GetDownloadDirectory() {
  base::FilePath download_path;
  if (!base::PathService::Get(chrome::DIR_DEFAULT_DOWNLOADS_SAFE,
                              &download_path)) {
    LOG(ERROR)
        << "Failed to get DIR_DEFAULT_DOWNLOADS from base::PathService::Get";
    return protocol::MakeFileTransferError(
        FROM_HERE, protocol::FileTransfer_Error_Type_UNEXPECTED_ERROR);
  }

  return download_path;
}
#endif

}  // namespace

protocol::FileTransferResult<base::FilePath> GetFileUploadDirectory() {
#if BUILDFLAG(IS_CHROMEOS)
  return GetDownloadDirectory();
#else
  base::FilePath target_directory;
  if (!base::PathService::Get(base::DIR_USER_DESKTOP, &target_directory)) {
    LOG(ERROR) << "Failed to get DIR_USER_DESKTOP from base::PathService::Get";
    return protocol::MakeFileTransferError(
        FROM_HERE, protocol::FileTransfer_Error_Type_UNEXPECTED_ERROR);
  }
  return target_directory;
#endif
}

}  // namespace remoting
