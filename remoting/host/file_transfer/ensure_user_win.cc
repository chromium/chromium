// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/file_transfer/ensure_user.h"

#include <Windows.h>
#include <WtsApi32.h>

#include "base/logging.h"
#include "base/win/scoped_handle.h"

namespace remoting {

protocol::FileTransferResult<absl::monostate> EnsureUserContext() {
  // Impersonate the currently logged-in user, or fail if there is none.
  HANDLE user_token = nullptr;
  if (!WTSQueryUserToken(WTS_CURRENT_SESSION, &user_token)) {
    PLOG(ERROR) << "Failed to get current user token";
    return protocol::MakeFileTransferError(
        FROM_HERE,
        GetLastError() == ERROR_NO_TOKEN
            ? protocol::FileTransfer_Error_Type_NOT_LOGGED_IN
            : protocol::FileTransfer_Error_Type_UNEXPECTED_ERROR,
        GetLastError());
  }
  base::win::ScopedHandle scoped_user_token(user_token);
  if (!ImpersonateLoggedOnUser(scoped_user_token.Get())) {
    PLOG(ERROR) << "Failed to impersonate user";
    return protocol::MakeFileTransferError(
        FROM_HERE, protocol::FileTransfer_Error_Type_UNEXPECTED_ERROR,
        GetLastError());
  }
  return kSuccessTag;
}

}  // namespace remoting
