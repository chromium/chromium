// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/file_transfer/file_chooser.h"

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
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

  void OpenFile(OpenFileCallback callback) override {
    std::move(callback).Run(result_);
  }

 private:
  mojo::Receiver<mojom::FileChooser> receiver_;
  FileChooserResult result_;
};

}  // namespace

class FileChooserWinTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(FileChooserWinTest, OpenFileSuccess) {
  mojo::Remote<mojom::FileChooser> remote;
  FakeFileChooser server(remote.BindNewPipeAndPassReceiver());

  base::FilePath test_path(FILE_PATH_LITERAL("C:\\test\\file.txt"));
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

}  // namespace remoting
