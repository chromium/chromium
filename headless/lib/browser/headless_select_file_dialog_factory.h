// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_HEADLESS_SELECT_FILE_DIALOG_FACTORY_H_
#define HEADLESS_LIB_BROWSER_HEADLESS_SELECT_FILE_DIALOG_FACTORY_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "headless/public/headless_export.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_dialog_factory.h"
#include "ui/shell_dialogs/select_file_policy.h"

namespace headless {

using SelectFileDialogCallback =
    base::OnceCallback<void(ui::SelectFileDialog::Type type)>;

class HEADLESS_EXPORT HeadlessSelectFileDialogFactory
    : public ui::SelectFileDialogFactory {
 public:
  HeadlessSelectFileDialogFactory(const HeadlessSelectFileDialogFactory&) =
      delete;
  HeadlessSelectFileDialogFactory& operator=(
      const HeadlessSelectFileDialogFactory&) = delete;

  // Creates the factory and sets it into ui::SelectFileDialog.
  static void SetUp();

  // Registers a one time callback that would be called when the next Select
  // File Dialog comes up. This can only be called after SetUp().
  static void SetSelectFileDialogOnceCallbackForTests(
      SelectFileDialogCallback callback);

 private:
  friend class HeadlessSelectFileDialog;

  // ui::SelectFileDialogFactory
  ui::SelectFileDialog* Create(
      ui::SelectFileDialog::Listener* listener,
      std::unique_ptr<ui::SelectFilePolicy> policy) override;

  HeadlessSelectFileDialogFactory();
  ~HeadlessSelectFileDialogFactory() override;

  SelectFileDialogCallback callback_;
};

}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_HEADLESS_SELECT_FILE_DIALOG_FACTORY_H_
