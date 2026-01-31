// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/host/weights_file_provider.h"

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/task/thread_pool.h"

namespace webnn {

namespace {

// Create a temporary file that will be passed to the gpu service and deleted on
// close.
base::File CreateTemporaryFile() {
  base::FilePath temp_path;
  if (!base::CreateTemporaryFile(&temp_path)) {
    return base::File();
  }

  base::File weights_file;
  weights_file.Initialize(
      temp_path, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_READ |
                     base::File::FLAG_WRITE | base::File::FLAG_WIN_TEMPORARY |
                     base::File::FLAG_WIN_NO_EXECUTE |
                     base::File::FLAG_DELETE_ON_CLOSE);
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
