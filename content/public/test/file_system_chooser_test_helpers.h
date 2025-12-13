// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_FILE_SYSTEM_CHOOSER_TEST_HELPERS_H_
#define CONTENT_PUBLIC_TEST_FILE_SYSTEM_CHOOSER_TEST_HELPERS_H_

#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_dialog_factory.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "url/gurl.h"

namespace content {

// Struct used to report what parameters one of the (fake) SelectFileDialog
// implementations below was created with.
struct SelectFileDialogParams {
  SelectFileDialogParams();
  ~SelectFileDialogParams();

  ui::SelectFileDialog::Type type = ui::SelectFileDialog::SELECT_NONE;
  std::optional<ui::SelectFileDialog::FileTypeInfo> file_types;
  gfx::NativeWindow owning_window = {};
  int file_type_index = -1;
  base::FilePath default_path;
  std::u16string title;
  std::optional<GURL> caller;
#if BUILDFLAG(IS_ANDROID)
  std::vector<std::u16string> accept_types;
#endif
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
  raw_ptr<SelectFileDialogParams> out_params_;
};

// A fake ui::SelectFileDialog, which will select one or more pre-determined
// files.
class FakeSelectFileDialogFactory : public ui::SelectFileDialogFactory {
 public:
  explicit FakeSelectFileDialogFactory(
      std::vector<base::FilePath> result,
      SelectFileDialogParams* out_params = nullptr);
  explicit FakeSelectFileDialogFactory(
      std::vector<ui::SelectedFileInfo> result,
      SelectFileDialogParams* out_params = nullptr);
  ~FakeSelectFileDialogFactory() override;

  ui::SelectFileDialog* Create(
      ui::SelectFileDialog::Listener* listener,
      std::unique_ptr<ui::SelectFilePolicy> policy) override;

 private:
  std::vector<ui::SelectedFileInfo> result_;
  raw_ptr<SelectFileDialogParams, DanglingUntriaged> out_params_;
};

// A dialog that signals when it is created and/or destroyed to an observer.
class ObservableSelectFileDialogFactory : public ui::SelectFileDialogFactory {
 public:
  class Observer {
   public:
    virtual void WasCreated() {}
    virtual void WasDestroyed() {}
  };
  explicit ObservableSelectFileDialogFactory(base::WeakPtr<Observer>);
  ~ObservableSelectFileDialogFactory() override;

  ui::SelectFileDialog* Create(
      ui::SelectFileDialog::Listener* listener,
      std::unique_ptr<ui::SelectFilePolicy> policy) override;

 private:
  const base::WeakPtr<Observer> observer_;
};

// Tracks the state of the dialog.
class SelectFileDialogRecorder
    : public ObservableSelectFileDialogFactory::Observer {
 public:
  SelectFileDialogRecorder();
  ~SelectFileDialogRecorder();

  enum State {
    kNotCreated,
    kCreated,
    kDestroyed,
  };
  void WasCreated() override;
  void WasDestroyed() override;
  base::WeakPtr<SelectFileDialogRecorder> GetWeakPtr();

  State state = kNotCreated;

 private:
  base::WeakPtrFactory<SelectFileDialogRecorder> weak_factory_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_FILE_SYSTEM_CHOOSER_TEST_HELPERS_H_
