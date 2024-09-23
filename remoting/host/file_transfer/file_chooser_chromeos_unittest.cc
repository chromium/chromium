// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/file_transfer/file_chooser_chromeos.h"

#include <algorithm>
#include <memory>

#include "base/check_deref.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "content/public/test/file_system_chooser_test_helpers.h"
#include "remoting/host/chromeos/scoped_fake_ash_proxy.h"
#include "remoting/host/file_transfer/file_chooser.h"
#include "remoting/protocol/file_transfer_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/env.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_dialog_factory.h"

using base::test::TestFuture;

namespace remoting {

namespace {

const base::FilePath kTestFilePath("/path/to/a/test/file.txt");

}  // namespace

class FileChooserChromeOsTest : public testing::Test {
 public:
  FileChooserChromeOsTest() = default;
  FileChooserChromeOsTest(const FileChooserChromeOsTest&) = delete;
  FileChooserChromeOsTest& operator=(const FileChooserChromeOsTest&) = delete;

  // `testing::Test` implementation.
  void TearDown() override { ui::SelectFileDialog::SetFactory(nullptr); }

  void CreateAndShowFileChooser(FileChooser::ResultCallback callback) {
    file_chooser_ = FileChooser::Create(ui_task_runner(), std::move(callback));
    file_chooser_->Show();
  }

  void SetResultFileSelectionFactory(base::FilePath file_path) {
    ui::SelectFileDialog::SetFactory(
        std::make_unique<content::FakeSelectFileDialogFactory>(
            std::vector<base::FilePath>{file_path}, &dialog_params_));
  }

  void SetCancelFileSelectionFactory() {
    ui::SelectFileDialog::SetFactory(
        std::make_unique<content::CancellingSelectFileDialogFactory>(
            &dialog_params_));
  }

  ui::SelectFileDialog::Type GetRequestedDialogType() {
    return dialog_params_.type;
  }

  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner() {
    return task_environment_.GetMainThreadTaskRunner();
  }

 private:
  test::ScopedFakeAshProxy ash_proxy_;
  content::SelectFileDialogParams dialog_params_;
  std::unique_ptr<FileChooser> file_chooser_;
  base::test::SingleThreadTaskEnvironment task_environment_;
};

#if defined(LEAK_SANITIZER)
// TODO(crbug.com/40916564): Fix LeakSanitizer failure.
#define MAYBE_DISABLED(name) DISABLED_##name
#else
#define MAYBE_DISABLED(name) name
#endif

TEST_F(FileChooserChromeOsTest, MAYBE_DISABLED(SingleFileSelection)) {
  base::test::TestFuture<FileChooser::Result> result_future;
  SetResultFileSelectionFactory(kTestFilePath);

  CreateAndShowFileChooser(result_future.GetCallback());

  ASSERT_TRUE(result_future.Get().is_success());
  EXPECT_EQ(result_future.Get().success(), kTestFilePath);
}

TEST_F(FileChooserChromeOsTest, MAYBE_DISABLED(FileCancelationAllowed)) {
  base::test::TestFuture<FileChooser::Result> result_future;
  SetCancelFileSelectionFactory();

  CreateAndShowFileChooser(result_future.GetCallback());

  ASSERT_TRUE(result_future.Get().is_error());
  EXPECT_EQ(result_future.Get().error().type(),
            protocol::FileTransfer_Error_Type_CANCELED);
}

TEST_F(FileChooserChromeOsTest, MAYBE_DISABLED(OnlyAllowSingleFileSelection)) {
  base::test::TestFuture<FileChooser::Result> result_future;
  SetResultFileSelectionFactory(kTestFilePath);

  CreateAndShowFileChooser(result_future.GetCallback());

  ASSERT_TRUE(result_future.Wait());
  EXPECT_EQ(GetRequestedDialogType(),
            ui::SelectFileDialog::Type::SELECT_OPEN_FILE);
}

}  // namespace remoting
