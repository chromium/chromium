// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NATIVE_FILE_SYSTEM_FILE_SYSTEM_CHOOSER_TEST_HELPERS_H_
#define CONTENT_BROWSER_NATIVE_FILE_SYSTEM_FILE_SYSTEM_CHOOSER_TEST_HELPERS_H_

#include "base/optional.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_dialog_factory.h"
#include "ui/shell_dialogs/select_file_policy.h"

namespace content {

// Struct used to report what parameters one of the (fake) SelectFileDialog
// implementations below was created with.
struct SelectFileDialogParams {
  SelectFileDialogParams();
  ~SelectFileDialogParams();

  ui::SelectFileDialog::Type type = ui::SelectFileDialog::SELECT_NONE;
  base::Optional<ui::SelectFileDialog::FileTypeInfo> file_types;
  gfx::NativeWindow owning_window = {};
};

// A fake ui::SelectFileDialog, which will cancel the file selection instead of
// selecting a file.
class CancellingSelectFileDialogFactory : public ui::SelectFileDialogFactory {
 public:
  explicit CancellingSelectFileDialogFactory(
      SelectFileDialogParams* out_params = nullptr);
  ~CancellingSelectFileDialogFactory() override;

  ui::SelectFileDialog* Create(
      ui::SelectFileDialog::Listener* listener,
      std::unique_ptr<ui::SelectFilePolicy> policy) override;

 private:
  SelectFileDialogParams* out_params_;
};

// A fake ui::SelectFileDialog, which will select one or more pre-determined
// files.
class FakeSelectFileDialogFactory : public ui::SelectFileDialogFactory {
 public:
  FakeSelectFileDialogFactory(std::vector<base::FilePath> result,
                              SelectFileDialogParams* out_params = nullptr);
  ~FakeSelectFileDialogFactory() override;

  ui::SelectFileDialog* Create(
      ui::SelectFileDialog::Listener* listener,
      std::unique_ptr<ui::SelectFilePolicy> policy) override;

 private:
  std::vector<base::FilePath> result_;
  SelectFileDialogParams* out_params_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_NATIVE_FILE_SYSTEM_FILE_SYSTEM_CHOOSER_TEST_HELPERS_H_