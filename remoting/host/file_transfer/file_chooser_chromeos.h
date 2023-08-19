// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_FILE_TRANSFER_FILE_CHOOSER_CHROMEOS_H_
#define REMOTING_HOST_FILE_TRANSFER_FILE_CHOOSER_CHROMEOS_H_

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "remoting/host/file_transfer/file_chooser.h"

namespace remoting {

class FileChooserChromeOs : public FileChooser {
 public:
  FileChooserChromeOs(scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
                      ResultCallback callback);

  FileChooserChromeOs(const FileChooserChromeOs&) = delete;
  FileChooserChromeOs& operator=(const FileChooserChromeOs&) = delete;

  ~FileChooserChromeOs() override;

  // `FileChooser` implementation.
  void Show() override;

 private:
  class Core;

  base::SequenceBound<Core> file_chooser_core_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_FILE_TRANSFER_FILE_CHOOSER_CHROMEOS_H_
