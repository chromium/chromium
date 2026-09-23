// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_FILE_TRANSFER_FILE_CHOOSER_WIN_H_
#define REMOTING_HOST_FILE_TRANSFER_FILE_CHOOSER_WIN_H_

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "remoting/host/file_transfer/file_chooser.h"
#include "remoting/host/mojom/desktop_session.mojom.h"
#include "remoting/protocol/file_transfer_helpers.h"

namespace remoting {

class FileChooserWindows : public FileChooser {
 public:
  struct LaunchResult {
    LaunchResult();
    LaunchResult(LaunchResult&&);
    LaunchResult& operator=(LaunchResult&&);
    ~LaunchResult();

    protocol::FileTransferResult<std::monostate> status = kSuccessTag;
    base::Process process;
    mojo::PendingRemote<mojom::FileChooser> pending_remote;
  };

  using LaunchProcessCallback = base::RepeatingCallback<LaunchResult()>;

  FileChooserWindows(scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
                     ResultCallback callback);

  FileChooserWindows(const FileChooserWindows&) = delete;
  FileChooserWindows& operator=(const FileChooserWindows&) = delete;

  ~FileChooserWindows() override;

  // FileChooser implementation.
  void Show() override;

  // Allows injecting a custom process launcher for unit testing.
  void SetLauncherForTesting(LaunchProcessCallback launcher);

 private:
  static LaunchResult LaunchChooserProcess();
  void OnProcessLaunched(LaunchResult result);
  void OnFileChooserResult(const FileChooser::Result& result);
  void OnDisconnected();

  SEQUENCE_CHECKER(sequence_checker_);

  ResultCallback callback_;
  base::Process process_;
  mojo::Remote<mojom::FileChooser> file_chooser_;
  LaunchProcessCallback launcher_for_testing_;
  base::WeakPtrFactory<FileChooserWindows> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_FILE_TRANSFER_FILE_CHOOSER_WIN_H_
