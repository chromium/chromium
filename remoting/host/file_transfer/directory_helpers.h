// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_FILE_TRANSFER_DIRECTORY_HELPERS_H_
#define REMOTING_HOST_FILE_TRANSFER_DIRECTORY_HELPERS_H_

#include "base/files/file_path.h"
#include "remoting/protocol/file_transfer_helpers.h"

namespace remoting {

// Retrieves the exact path to the user's upload folder which is platform
// dependent. This should be run in the context of the user, which on Windows
// means it must be run on the same dedicated thread on which EnsureUser was
// called.
protocol::FileTransferResult<base::FilePath> GetFileUploadDirectory();

void SetFileUploadDirectoryForTesting(base::FilePath dir);

}  // namespace remoting

#endif  // REMOTING_HOST_FILE_TRANSFER_DIRECTORY_HELPERS_H_
