// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "remoting/host/file_transfer/file_chooser.h"
#include "remoting/host/file_transfer/file_chooser_win.h"

namespace remoting {

std::unique_ptr<FileChooser> FileChooser::Create(
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    ResultCallback callback) {
  return std::make_unique<FileChooserWindows>(std::move(ui_task_runner),
                                              std::move(callback));
}

}  // namespace remoting
