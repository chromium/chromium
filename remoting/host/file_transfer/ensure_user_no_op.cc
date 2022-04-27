// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/file_transfer/ensure_user.h"

namespace remoting {

protocol::FileTransferResult<absl::monostate> EnsureUserContext() {
  return kSuccessTag;
}

}  // namespace remoting
