// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/file_system_chooser_test_helpers.h"

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_dialog_factory.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "ui/shell_dialogs/selected_file_info.h"

namespace content {

namespace {

class TestSelectFileDialogListener : public ui::SelectFileDialog::Listener {
 public:
  void FileSelected(const ui::SelectedFileInfo& file, int index) override {
    selected_files_.push_back(file.path());
  }

  void MultiFilesSelected(
      const std::vector<ui::SelectedFileInfo>& files) override {
    for (const auto& file : files) {
      selected_files_.push_back(file.path());
    }
  }

  void FileSelectionCanceled() override { canceled_ = true; }

  const std::vector<base::FilePath>& selected_files() const {
    return selected_files_;
  }
  bool canceled() const { return canceled_; }

  void Reset() {
    selected_files_.clear();
    canceled_ = false;
  }

 private:
  std::vector<base::FilePath> selected_files_;
  bool canceled_ = false;
};

void OpenFileDialog(ui::SelectFileDialog::Listener* listener) {
  scoped_refptr<ui::SelectFileDialog> dialog =
      ui::SelectFileDialog::Create(listener, nullptr);
  if (dialog) {
    dialog->SelectFile(ui::SelectFileDialog::SELECT_OPEN_FILE,
                       /*title=*/std::u16string(),
                       /*default_path=*/base::FilePath(),
                       /*file_types=*/nullptr,
                       /*file_type_index=*/0,
                       /*default_extension=*/base::FilePath::StringType(),
                       /*owning_window=*/{},
                       /*caller=*/nullptr);
  }
}

}  // namespace

class ScopedSelectFileDialogFactoryTest : public testing::Test {
 public:
  void TearDown() override { ui::SelectFileDialog::SetFactory(nullptr); }
};

TEST_F(ScopedSelectFileDialogFactoryTest,
       PersistsConfiguredFilesAcrossMultipleDialogs) {
  base::FilePath test_file(FILE_PATH_LITERAL("/test/path/file.txt"));
  TestSelectFileDialogListener listener;

  ScopedSelectFileDialogFactory factory(std::vector<base::FilePath>{test_file});

  OpenFileDialog(&listener);
  EXPECT_FALSE(listener.canceled());
  EXPECT_THAT(listener.selected_files(), testing::ElementsAre(test_file));

  listener.Reset();
  OpenFileDialog(&listener);
  EXPECT_FALSE(listener.canceled());
  EXPECT_THAT(listener.selected_files(), testing::ElementsAre(test_file));
}

TEST_F(ScopedSelectFileDialogFactoryTest, NestedScopesRestoration) {
  base::FilePath outer_file(FILE_PATH_LITERAL("/test/path/outer.txt"));
  base::FilePath inner_file(FILE_PATH_LITERAL("/test/path/inner.txt"));
  TestSelectFileDialogListener listener;

  ScopedSelectFileDialogFactory outer_factory(
      std::vector<base::FilePath>{outer_file});

  // Outer scope before nesting: should select outer_file.
  OpenFileDialog(&listener);
  EXPECT_FALSE(listener.canceled());
  EXPECT_THAT(listener.selected_files(), testing::ElementsAre(outer_file));

  {
    ScopedSelectFileDialogFactory inner_factory(
        std::vector<base::FilePath>{inner_file});

    // Inner scope: should select inner_file.
    listener.Reset();
    OpenFileDialog(&listener);
    EXPECT_FALSE(listener.canceled());
    EXPECT_THAT(listener.selected_files(), testing::ElementsAre(inner_file));
  }

  // After inner scope exits: outer_file should be restored.
  listener.Reset();
  OpenFileDialog(&listener);
  EXPECT_FALSE(listener.canceled());
  EXPECT_THAT(listener.selected_files(), testing::ElementsAre(outer_file));
}

TEST_F(ScopedSelectFileDialogFactoryTest, CancellingWhenNoFilesConfigured) {
  TestSelectFileDialogListener listener;
  ScopedSelectFileDialogFactory factory;

  OpenFileDialog(&listener);
  EXPECT_TRUE(listener.canceled());
  EXPECT_THAT(listener.selected_files(), testing::IsEmpty());
}

TEST_F(ScopedSelectFileDialogFactoryTest, RestoresExternalFactory) {
  class CustomFactory : public ui::SelectFileDialogFactory {
   public:
    explicit CustomFactory(bool* called) : called_(called) {}
    ui::SelectFileDialog* Create(
        ui::SelectFileDialog::Listener* listener,
        std::unique_ptr<ui::SelectFilePolicy> policy) override {
      if (called_) {
        *called_ = true;
      }
      return nullptr;
    }

   private:
    raw_ptr<bool> called_;
  };

  bool custom_factory_called = false;
  ui::SelectFileDialog::SetFactory(
      std::make_unique<CustomFactory>(&custom_factory_called));

  base::FilePath test_file(FILE_PATH_LITERAL("/test/path/file.txt"));
  TestSelectFileDialogListener listener;
  {
    ScopedSelectFileDialogFactory scoped_factory(
        std::vector<base::FilePath>{test_file});

    OpenFileDialog(&listener);
    EXPECT_FALSE(listener.canceled());
    EXPECT_THAT(listener.selected_files(), testing::ElementsAre(test_file));
    EXPECT_FALSE(custom_factory_called);
  }

  // After scoped_factory is destroyed, the custom factory should be restored.
  ui::SelectFileDialog::Create(nullptr, nullptr);
  EXPECT_TRUE(custom_factory_called);
}

}  // namespace content
