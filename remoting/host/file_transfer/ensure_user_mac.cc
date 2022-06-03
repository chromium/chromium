// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/file_transfer/ensure_user.h"

#include <unistd.h>

#include "base/logging.h"

namespace remoting {

protocol::FileTransferResult<Monostate> EnsureUserContext() {
  // Make sure we're not on the log-in screen.
  if (getuid() == 0) {
    LOG(ERROR) << "Cannot transfer files on log-in screen.";
    return protocol::MakeFileTransferError(
        FROM_HERE, protocol::FileTransfer_Error_Type_NOT_LOGGED_IN);
  }
  return kSuccessTag;
}

}  // namespace remoting
