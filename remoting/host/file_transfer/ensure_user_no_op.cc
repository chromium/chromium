// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/file_transfer/ensure_user.h"

namespace remoting {

protocol::FileTransferResult<absl::monostate> EnsureUserContext() {
  return kSuccessTag;
}

void DisableUserContextCheckForTesting() {
  // Nothing to do here since user checking is already a no-op.
}

}  // namespace remoting
