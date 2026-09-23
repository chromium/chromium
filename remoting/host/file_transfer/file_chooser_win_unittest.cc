// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/file_transfer/file_chooser.h"

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "remoting/host/file_transfer/file_chooser_win.h"
#include "remoting/host/mojom/desktop_session.mojom.h"
#include "remoting/protocol/file_transfer_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

using FileChooserResult = ::remoting::FileChooser::Result;

class FakeFileChooser : public mojom::FileChooser {
 public:
  explicit FakeFileChooser(mojo::PendingReceiver<mojom::FileChooser> receiver)
      : receiver_(this, std::move(receiver)) {}

  void SetResult(FileChooserResult result) { result_ = std::move(result); }
  void SetRespondImmediately(bool respond) { respond_immediately_ = respond; }

  void OpenFile(OpenFileCallback callback) override {
    if (respond_immediately_) {
      std::move(callback).Run(result_);
    } else {
      pending_callback_ = std::move(callback);
    }
  }

 private:
  mojo::Receiver<mojom::FileChooser> receiver_;
  FileChooserResult result_;
  bool respond_immediately_ = true;
  OpenFileCallback pending_callback_;
};

}  // namespace

class FileChooserWinTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(FileChooserWinTest, OpenFileSuccess) {
  mojo::Remote<mojom::FileChooser> remote;
  FakeFileChooser server(remote.BindNewPipeAndPassReceiver());

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath test_path = temp_dir.GetPath().AppendASCII("file.txt");
  server.SetResult(FileChooserResult(test_path));

  base::RunLoop run_loop;
  FileChooserResult actual_result;
  remote->OpenFile(base::BindOnce(
      [](base::OnceClosure quit_closure, FileChooserResult* out_result,
         const FileChooserResult& result) {
        *out_result = result;
        std::move(quit_closure).Run();
      },
      run_loop.QuitClosure(), &actual_result));

  run_loop.Run();

  ASSERT_TRUE(actual_result.is_success());
  EXPECT_EQ(actual_result.success(), test_path);
}

TEST_F(FileChooserWinTest, OpenFileCanceled) {
  mojo::Remote<mojom::FileChooser> remote;
  FakeFileChooser server(remote.BindNewPipeAndPassReceiver());

  protocol::FileTransfer_Error error = MakeFileTransferError(
      FROM_HERE, protocol::FileTransfer_Error_Type_CANCELED);
  server.SetResult(FileChooserResult(error));

  base::RunLoop run_loop;
  FileChooserResult actual_result;
  remote->OpenFile(base::BindOnce(
      [](base::OnceClosure quit_closure, FileChooserResult* out_result,
         const FileChooserResult& result) {
        *out_result = result;
        std::move(quit_closure).Run();
      },
      run_loop.QuitClosure(), &actual_result));

  run_loop.Run();

  ASSERT_TRUE(actual_result.is_error());
  EXPECT_EQ(actual_result.error().type(),
            protocol::FileTransfer_Error_Type_CANCELED);
}

TEST_F(FileChooserWinTest, DisconnectHandled) {
  mojo::Remote<mojom::FileChooser> remote;
  auto server =
      std::make_unique<FakeFileChooser>(remote.BindNewPipeAndPassReceiver());

  base::RunLoop run_loop;
  bool disconnected = false;
  remote.set_disconnect_handler(base::BindOnce(
      [](base::OnceClosure quit_closure, bool* out_disconnected) {
        *out_disconnected = true;
        std::move(quit_closure).Run();
      },
      run_loop.QuitClosure(), &disconnected));

  // Simulate process termination/crash before responding.
  server.reset();

  run_loop.Run();

  EXPECT_TRUE(disconnected);
}

TEST_F(FileChooserWinTest, FileChooserWindows_Success) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath test_path = temp_dir.GetPath().AppendASCII("file.txt");
  std::unique_ptr<FakeFileChooser> server;

  base::test::TestFuture<FileChooserResult> future;
  auto file_chooser = std::make_unique<FileChooserWindows>(
      task_environment_.GetMainThreadTaskRunner(), future.GetCallback());

  file_chooser->SetLauncherForTesting(base::BindLambdaForTesting([&]() {
    FileChooserWindows::LaunchResult launch_result;
    mojo::PendingReceiver<mojom::FileChooser> receiver =
        launch_result.pending_remote.InitWithNewPipeAndPassReceiver();
    server = std::make_unique<FakeFileChooser>(std::move(receiver));
    server->SetResult(FileChooserResult(test_path));
    return launch_result;
  }));

  file_chooser->Show();

  FileChooserResult result = future.Get();
  ASSERT_TRUE(result.is_success());
  EXPECT_EQ(result.success(), test_path);
}

TEST_F(FileChooserWinTest, FileChooserWindows_Canceled) {
  std::unique_ptr<FakeFileChooser> server;
  base::test::TestFuture<FileChooserResult> future;
  auto file_chooser = std::make_unique<FileChooserWindows>(
      task_environment_.GetMainThreadTaskRunner(), future.GetCallback());

  file_chooser->SetLauncherForTesting(base::BindLambdaForTesting([&]() {
    FileChooserWindows::LaunchResult launch_result;
    mojo::PendingReceiver<mojom::FileChooser> receiver =
        launch_result.pending_remote.InitWithNewPipeAndPassReceiver();
    server = std::make_unique<FakeFileChooser>(std::move(receiver));
    server->SetResult(FileChooserResult(MakeFileTransferError(
        FROM_HERE, protocol::FileTransfer_Error_Type_CANCELED)));
    return launch_result;
  }));

  file_chooser->Show();

  FileChooserResult result = future.Get();
  ASSERT_TRUE(result.is_error());
  EXPECT_EQ(result.error().type(), protocol::FileTransfer_Error_Type_CANCELED);
}

TEST_F(FileChooserWinTest, FileChooserWindows_LaunchFailed) {
  base::test::TestFuture<FileChooserResult> future;
  auto file_chooser = std::make_unique<FileChooserWindows>(
      task_environment_.GetMainThreadTaskRunner(), future.GetCallback());

  file_chooser->SetLauncherForTesting(base::BindLambdaForTesting([]() {
    FileChooserWindows::LaunchResult launch_result;
    launch_result.status = MakeFileTransferError(
        FROM_HERE, protocol::FileTransfer_Error_Type_NOT_LOGGED_IN);
    return launch_result;
  }));

  file_chooser->Show();

  FileChooserResult result = future.Get();
  ASSERT_TRUE(result.is_error());
  EXPECT_EQ(result.error().type(),
            protocol::FileTransfer_Error_Type_NOT_LOGGED_IN);
}

TEST_F(FileChooserWinTest, FileChooserWindows_DisconnectedWhilePending) {
  std::unique_ptr<FakeFileChooser> server;
  base::test::TestFuture<FileChooserResult> future;
  auto file_chooser = std::make_unique<FileChooserWindows>(
      task_environment_.GetMainThreadTaskRunner(), future.GetCallback());

  file_chooser->SetLauncherForTesting(base::BindLambdaForTesting([&]() {
    FileChooserWindows::LaunchResult launch_result;
    mojo::PendingReceiver<mojom::FileChooser> receiver =
        launch_result.pending_remote.InitWithNewPipeAndPassReceiver();
    server = std::make_unique<FakeFileChooser>(std::move(receiver));
    server->SetRespondImmediately(false);
    return launch_result;
  }));

  file_chooser->Show();

  // Reset the server to trigger a Mojo disconnection while waiting for the
  // response.
  server.reset();

  FileChooserResult result = future.Get();
  ASSERT_TRUE(result.is_error());
  EXPECT_EQ(result.error().type(),
            protocol::FileTransfer_Error_Type_UNEXPECTED_ERROR);
}

}  // namespace remoting
