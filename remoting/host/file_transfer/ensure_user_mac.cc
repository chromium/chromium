// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/file_transfer/ensure_user.h"

#include <unistd.h>

#include "base/check_is_test.h"
#include "base/logging.h"

namespace remoting {

namespace {

static bool g_disable_user_context_check_for_testing = false;

}  // namespace

protocol::FileTransferResult<absl::monostate> EnsureUserContext() {
  if (g_disable_user_context_check_for_testing) {
    CHECK_IS_TEST();
    return kSuccessTag;
  }
  // Make sure we're not on the log-in screen.
  if (getuid() == 0) {
    LOG(ERROR) << "Cannot transfer files on log-in screen.";
    return protocol::MakeFileTransferError(
        FROM_HERE, protocol::FileTransfer_Error_Type_NOT_LOGGED_IN);
  }
  return kSuccessTag;
}

void DisableUserContextCheckForTesting() {
  g_disable_user_context_check_for_testing = true;
}

}  // namespace remoting
