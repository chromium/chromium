// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_FILE_TRANSFER_FILE_CHOOSER_H_
#define REMOTING_HOST_FILE_TRANSFER_FILE_CHOOSER_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "remoting/protocol/file_transfer_helpers.h"

namespace remoting {

class FileChooser {
 public:
  typedef protocol::FileTransferResult<base::FilePath> Result;
  typedef base::OnceCallback<void(Result)> ResultCallback;

  FileChooser() = default;

  FileChooser(const FileChooser&) = delete;
  FileChooser& operator=(const FileChooser&) = delete;

  // If the file dialog is currently displayed, destroys it without invoking
  // |callback|.
  virtual ~FileChooser() = default;

  // Shows the file chooser dialog. May only be called once.
  virtual void Show() = 0;

  // Creates a file chooser on the provided UI thread. |callback| will be passed
  // the chosen file, if any, or an error. If the user cancels the dialog, the
  // error type will be CANCELED. An error of a different type signifies a
  // problem showing the dialog.
  static std::unique_ptr<FileChooser> Create(
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
      ResultCallback callback);
};

}  // namespace remoting

#endif  // REMOTING_HOST_FILE_TRANSFER_FILE_CHOOSER_H_
