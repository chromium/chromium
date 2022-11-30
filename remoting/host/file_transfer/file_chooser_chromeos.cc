// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "remoting/host/file_transfer/file_chooser.h"

namespace remoting {

std::unique_ptr<FileChooser> FileChooser::Create(
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    ResultCallback callback) {
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace remoting
