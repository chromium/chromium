// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/file_transfer/fake_file_chooser.h"

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "remoting/protocol/file_transfer_helpers.h"

namespace remoting {

namespace {

FileChooser::Result& StaticResult() {
  static base::NoDestructor<FileChooser::Result> result(
      protocol::MakeFileTransferError(
          FROM_HERE, protocol::FileTransfer_Error_Type_CANCELED));
  return *result;
}

}  // namespace

std::unique_ptr<FileChooser> FileChooser::Create(
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    ResultCallback callback) {
  return std::make_unique<FakeFileChooser>(std::move(callback));
}

FakeFileChooser::FakeFileChooser(FileChooser::ResultCallback callback)
    : callback_(std::move(callback)) {}

FakeFileChooser::~FakeFileChooser() = default;

void FakeFileChooser::Show() {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback_), StaticResult()));
}

void FakeFileChooser::SetResult(FileChooser::Result result) {
  StaticResult() = std::move(result);
}

}  // namespace remoting
