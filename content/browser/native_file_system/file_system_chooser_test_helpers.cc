// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/native_file_system/file_system_chooser_test_helpers.h"

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
                      const base::string16& title,
                      const base::FilePath& default_path,
                      const FileTypeInfo* file_types,
                      int file_type_index,
                      const base::FilePath::StringType& default_extension,
                      gfx::NativeWindow owning_window,
                      void* params) override {
    if (out_params_) {
      out_params_->type = type;
      if (file_types)
        out_params_->file_types = *file_types;
      else
        out_params_->file_types = base::nullopt;
      out_params_->owning_window = owning_window;
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
  SelectFileDialogParams* out_params_;
};

class FakeSelectFileDialog : public ui::SelectFileDialog {
 public:
  FakeSelectFileDialog(std::vector<base::FilePath> result,
                       SelectFileDialogParams* out_params,
                       Listener* listener,
                       std::unique_ptr<ui::SelectFilePolicy> policy)
      : ui::SelectFileDialog(listener, std::move(policy)),
        result_(std::move(result)),
        out_params_(out_params) {}

 protected:
  void SelectFileImpl(Type type,
                      const base::string16& title,
                      const base::FilePath& default_path,
                      const FileTypeInfo* file_types,
                      int file_type_index,
                      const base::FilePath::StringType& default_extension,
                      gfx::NativeWindow owning_window,
                      void* params) override {
    if (out_params_) {
      out_params_->type = type;
      if (file_types)
        out_params_->file_types = *file_types;
      else
        out_params_->file_types = base::nullopt;
      out_params_->owning_window = owning_window;
    }
    if (result_.size() == 1)
      listener_->FileSelected(result_[0], 0, params);
    else
      listener_->MultiFilesSelected(result_, params);
  }

  bool IsRunning(gfx::NativeWindow owning_window) const override {
    return false;
  }
  void ListenerDestroyed() override {}
  bool HasMultipleFileTypeChoicesImpl() override { return false; }

 private:
  ~FakeSelectFileDialog() override = default;
  std::vector<base::FilePath> result_;
  SelectFileDialogParams* out_params_;
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
    : result_(std::move(result)), out_params_(out_params) {}

FakeSelectFileDialogFactory::~FakeSelectFileDialogFactory() = default;

ui::SelectFileDialog* FakeSelectFileDialogFactory::Create(
    ui::SelectFileDialog::Listener* listener,
    std::unique_ptr<ui::SelectFilePolicy> policy) {
  return new FakeSelectFileDialog(result_, out_params_, listener,
                                  std::move(policy));
}

}  // namespace content