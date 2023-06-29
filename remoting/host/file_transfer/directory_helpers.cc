// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/file_transfer/directory_helpers.h"

#include "base/check_is_test.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "remoting/protocol/file_transfer_helpers.h"

#if BUILDFLAG(IS_CHROMEOS)
// `nogncheck` prevents false positive 'missing dependency' errors on
// non-chromeos builds.
#include "chrome/common/chrome_paths.h"  // nogncheck
#endif

namespace remoting {

namespace {

static base::FilePath* g_upload_directory_for_testing = nullptr;

protocol::FileTransferResult<base::FilePath> GetDirectory(int path_key) {
  base::FilePath directory_path;
  if (!base::PathService::Get(path_key, &directory_path)) {
    LOG(ERROR) << "Failed to get path from base::PathService::Get";
    return protocol::MakeFileTransferError(
        FROM_HERE, protocol::FileTransfer_Error_Type_UNEXPECTED_ERROR);
  }

  return directory_path;
}

}  // namespace

protocol::FileTransferResult<base::FilePath> GetFileUploadDirectory() {
  if (g_upload_directory_for_testing) {
    CHECK_IS_TEST();
    return *g_upload_directory_for_testing;
  }
#if BUILDFLAG(IS_CHROMEOS)
  return GetDirectory(chrome::DIR_DEFAULT_DOWNLOADS_SAFE);
#else
  return GetDirectory(base::DIR_USER_DESKTOP);
#endif
}

void SetFileUploadDirectoryForTesting(base::FilePath dir) {
  if (g_upload_directory_for_testing) {
    delete g_upload_directory_for_testing;
  }
  g_upload_directory_for_testing = new base::FilePath(dir);
}

}  // namespace remoting
