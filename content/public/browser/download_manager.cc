// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/download_manager.h"

#include "base/task/sequenced_task_runner.h"
#include "components/download/public/common/download_task_runner.h"

namespace content {

// static
scoped_refptr<base::SequencedTaskRunner> DownloadManager::GetTaskRunner() {
  return download::GetDownloadTaskRunner();
}

}  // namespace content
