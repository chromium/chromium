// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_FILE_TRANSFER_FAKE_FILE_CHOOSER_H_
#define REMOTING_HOST_FILE_TRANSFER_FAKE_FILE_CHOOSER_H_

#include "remoting/host/file_transfer/file_chooser.h"

namespace remoting {

class FakeFileChooser : public FileChooser {
 public:
  explicit FakeFileChooser(ResultCallback callback);

  FakeFileChooser(const FakeFileChooser&) = delete;
  FakeFileChooser& operator=(const FakeFileChooser&) = delete;

  ~FakeFileChooser() override;

  // FileChooser implementation.
  void Show() override;

  // The result that usages of FakeFileChooser should return.
  static void SetResult(FileChooser::Result result);

 private:
  ResultCallback callback_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_FILE_TRANSFER_FAKE_FILE_CHOOSER_H_
