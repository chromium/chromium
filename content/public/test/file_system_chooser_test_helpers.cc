// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/file_system_chooser_test_helpers.h"

#include "base/memory/raw_ptr.h"
#include "ui/shell_dialogs/selected_file_info.h"
#include "url/gurl.h"

namespace content {

namespace {

class CancellingSelectFileDialog : public ui::SelectFileDialog {
 public:
  CancellingSelectFileDialog(SelectFileDialogParams* out_params,
                             Listener* listener,
                             std::unique_ptr<ui::SelectFilePolicy> policy)
      : ui::SelectFileDialog(listener, std::move(policy)),
        out_params_(out_params) {}

 protected:
  void SelectFileImpl(Type type,
                      const std::u16string& title,
                      const base::FilePath& default_path,
                      const FileTypeInfo* file_types,
                      int file_type_index,
                      const base::FilePath::StringType& default_extension,
                      gfx::NativeWindow owning_window,
                      void* params,
                      const GURL* caller) override {
    if (out_params_) {
      out_params_->type = type;
      if (file_types) {
        out_params_->file_types = *file_types;
      } else {
        out_params_->file_types = absl::nullopt;
      }
      out_params_->owning_window = owning_window;
      out_params_->file_type_index = file_type_index;
      out_params_->default_path = default_path;
      out_params_->title = title;
      if (caller) {
        out_params_->caller = *caller;
      } else {
        out_params_->caller = absl::nullopt;
      }
    }
    listener_->FileSelectionCanceled(params);
  }

  bool IsRunning(gfx::NativeWindow owning_window) const override {
    return false;
  }
  void ListenerDestroyed() override {}
  bool HasMultipleFileTypeChoicesImpl() override { return false; }

 private:
  ~CancellingSelectFileDialog() override = default;
  raw_ptr<SelectFileDialogParams, LeakedDanglingUntriaged> out_params_;
};

class FakeSelectFileDialog : public ui::SelectFileDialog {
 public:
  FakeSelectFileDialog(std::vector<ui::SelectedFileInfo> result,
                       SelectFileDialogParams* out_params,
                       Listener* listener,
                       std::unique_ptr<ui::SelectFilePolicy> policy)
      : ui::SelectFileDialog(listener, std::move(policy)),
        result_(std::move(result)),
        out_params_(out_params) {}

 protected:
  void SelectFileImpl(Type type,
                      const std::u16string& title,
                      const base::FilePath& default_path,
                      const FileTypeInfo* file_types,
                      int file_type_index,
                      const base::FilePath::StringType& default_extension,
                      gfx::NativeWindow owning_window,
                      void* params,
                      const GURL* caller) override {
    if (out_params_) {
      out_params_->type = type;
      if (file_types) {
        out_params_->file_types = *file_types;
      } else {
        out_params_->file_types = absl::nullopt;
      }
      out_params_->owning_window = owning_window;
      out_params_->file_type_index = file_type_index;
      out_params_->default_path = default_path;
      out_params_->title = title;
      if (caller) {
        out_params_->caller = *caller;
      } else {
        out_params_->caller = absl::nullopt;
      }
    }
    // The selected files are passed by reference to the listener. Ensure they
    // outlive the dialog if it is immediately deleted by the listener.
    std::vector<ui::SelectedFileInfo> result = std::move(result_);
    result_.clear();
    if (result.size() == 1) {
      listener_->FileSelectedWithExtraInfo(result[0], 0, params);
    } else {
      listener_->MultiFilesSelectedWithExtraInfo(result, params);
    }
  }

  bool IsRunning(gfx::NativeWindow owning_window) const override {
    return false;
  }
  void ListenerDestroyed() override {}
  bool HasMultipleFileTypeChoicesImpl() override { return false; }

 private:
  ~FakeSelectFileDialog() override = default;
  std::vector<ui::SelectedFileInfo> result_;
  raw_ptr<SelectFileDialogParams, LeakedDanglingUntriaged> out_params_;
};

}  // namespace

SelectFileDialogParams::SelectFileDialogParams() = default;
SelectFileDialogParams::~SelectFileDialogParams() = default;

CancellingSelectFileDialogFactory::CancellingSelectFileDialogFactory(
    SelectFileDialogParams* out_params)
    : out_params_(out_params) {}

CancellingSelectFileDialogFactory::~CancellingSelectFileDialogFactory() =
    default;

ui::SelectFileDialog* CancellingSelectFileDialogFactory::Create(
    ui::SelectFileDialog::Listener* listener,
    std::unique_ptr<ui::SelectFilePolicy> policy) {
  return new CancellingSelectFileDialog(out_params_, listener,
                                        std::move(policy));
}

FakeSelectFileDialogFactory::FakeSelectFileDialogFactory(
    std::vector<base::FilePath> result,
    SelectFileDialogParams* out_params)
    : FakeSelectFileDialogFactory(
          ui::FilePathListToSelectedFileInfoList(result),
          out_params) {}

FakeSelectFileDialogFactory::FakeSelectFileDialogFactory(
    std::vector<ui::SelectedFileInfo> result,
    SelectFileDialogParams* out_params)
    : result_(std::move(result)), out_params_(out_params) {}

FakeSelectFileDialogFactory::~FakeSelectFileDialogFactory() = default;

ui::SelectFileDialog* FakeSelectFileDialogFactory::Create(
    ui::SelectFileDialog::Listener* listener,
    std::unique_ptr<ui::SelectFilePolicy> policy) {
  return new FakeSelectFileDialog(result_, out_params_, listener,
                                  std::move(policy));
}

}  // namespace content
