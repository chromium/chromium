// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/file_transfer/get_desktop_directory.h"

#include "base/logging.h"
#include "base/path_service.h"

namespace remoting {

protocol::FileTransferResult<base::FilePath> GetDesktopDirectory() {
  base::FilePath target_directory;
  if (!base::PathService::Get(base::DIR_USER_DESKTOP, &target_directory)) {
    LOG(ERROR) << "Failed to get DIR_USER_DESKTOP from base::PathService::Get";
    return protocol::MakeFileTransferError(
        FROM_HERE, protocol::FileTransfer_Error_Type_UNEXPECTED_ERROR);
  }
  return target_directory;
}

}  // namespace remoting
