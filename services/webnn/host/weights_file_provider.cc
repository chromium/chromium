// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/host/weights_file_provider.h"

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/task/thread_pool.h"

namespace webnn {

namespace {

// Create a temporary file that will be passed to the GPU process and deleted on
// close.
base::File CreateTemporaryFile() {
  base::FilePath temp_dir;
  if (!base::GetTempDir(&temp_dir)) {
    return base::File();
  }

  // The file may be passed to an untrusted process, so set
  // `FLAG_WIN_NO_EXECUTE` to match the flags added by
  // `base::File::AddFlagsForPassingToUntrustedProcess()` for read/write files.
  base::FilePath path;
  base::File weights_file = base::CreateAndOpenTemporaryFileInDir(
      temp_dir, &path,
      base::File::FLAG_WIN_TEMPORARY | base::File::FLAG_WIN_NO_EXECUTE);
  if (weights_file.IsValid()) {
    // On POSIX platforms we can just call unlink(2) immediately and the file
    // will deleted when the FD is closed but on Windows instead set this up
    // explicitly.
#if BUILDFLAG(IS_WIN)
    weights_file.DeleteOnClose(true);
#else
    base::DeleteFile(path);
#endif
  }

  return weights_file;
}

}  // namespace

void CreateWeightsFile(CreateWeightsFileCallback callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN, base::MayBlock()},
      base::BindOnce(&CreateTemporaryFile), std::move(callback));
}

}  // namespace webnn
