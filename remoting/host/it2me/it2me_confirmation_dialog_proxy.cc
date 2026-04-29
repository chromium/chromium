// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/it2me/it2me_confirmation_dialog_proxy.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"

namespace remoting {

class It2MeConfirmationDialogProxy::Core {
 public:
  Core(scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
       scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
       base::WeakPtr<It2MeConfirmationDialogProxy> parent,
       std::unique_ptr<It2MeConfirmationDialog> dialog);

  Core(const Core&) = delete;
  Core& operator=(const Core&) = delete;

  ~Core();

  // Shows the wrapped dialog. Must be called on the UI thread.
  void Show(const std::string& remote_user_email);

  // Sets whether the wrapped dialog's inputs are disabled. Must be called on
  // the UI thread.
  void SetDisableInputs(bool disable);

  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner() {
    return ui_task_runner_;
  }

  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner() {
    return caller_task_runner_;
  }

 private:
  // Reports the dialog result on the caller's thread.
  void ReportResult(It2MeConfirmationDialog::Result result);

  // Shows the wrapped dialog. Must be called on the UI thread.
  void ShowAfterDrain(const std::string& remote_user_email);

  // Updates the wrapped dialog's inputs state based on |is_disabled_by_caller_|
  // and |is_disabled_for_drain_|.
  void UpdateDialogInputs();

  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;
  base::WeakPtr<It2MeConfirmationDialogProxy> parent_;
  std::unique_ptr<It2MeConfirmationDialog> dialog_;

  bool is_disabled_by_caller_ = false;
  bool is_disabled_for_drain_ = false;

  base::WeakPtrFactory<Core> weak_factory_{this};
};

It2MeConfirmationDialogProxy::Core::Core(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    base::WeakPtr<It2MeConfirmationDialogProxy> parent,
    std::unique_ptr<It2MeConfirmationDialog> dialog)
    : ui_task_runner_(ui_task_runner),
      caller_task_runner_(caller_task_runner),
      parent_(parent),
      dialog_(std::move(dialog)) {}

It2MeConfirmationDialogProxy::Core::~Core() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
}

void It2MeConfirmationDialogProxy::Core::Show(
    const std::string& remote_user_email) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  // Set inputs to disabled before showing the dialog to avoid accidental
  // clicks.
  is_disabled_for_drain_ = true;
  UpdateDialogInputs();

  // Post a task to actually show the dialog. This allows any pending events in
  // the queue to be processed before the dialog is shown.
  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&It2MeConfirmationDialogProxy::Core::ShowAfterDrain,
                     weak_factory_.GetWeakPtr(), remote_user_email));
}

void It2MeConfirmationDialogProxy::Core::ShowAfterDrain(
    const std::string& remote_user_email) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  dialog_->Show(
      remote_user_email,
      base::BindOnce(&It2MeConfirmationDialogProxy::Core::ReportResult,
                     weak_factory_.GetWeakPtr()));

  // Re-enable inputs. Note that some platform implementations may choose to
  // delay enabling inputs further (e.g. via a timer).
  is_disabled_for_drain_ = false;
  UpdateDialogInputs();
}

void It2MeConfirmationDialogProxy::Core::SetDisableInputs(bool disable) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  is_disabled_by_caller_ = disable;
  UpdateDialogInputs();
}

void It2MeConfirmationDialogProxy::Core::UpdateDialogInputs() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  dialog_->SetDisableInputs(is_disabled_by_caller_ || is_disabled_for_drain_);
}

void It2MeConfirmationDialogProxy::Core::ReportResult(
    It2MeConfirmationDialog::Result result) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  caller_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&It2MeConfirmationDialogProxy::ReportResult,
                                parent_, result));
}

It2MeConfirmationDialogProxy::It2MeConfirmationDialogProxy(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    std::unique_ptr<It2MeConfirmationDialog> dialog) {
  core_ = std::make_unique<Core>(
      ui_task_runner, base::SingleThreadTaskRunner::GetCurrentDefault(),
      weak_factory_.GetWeakPtr(), std::move(dialog));
}

It2MeConfirmationDialogProxy::~It2MeConfirmationDialogProxy() {
  DCHECK(core_->caller_task_runner()->BelongsToCurrentThread());

  auto ui_task_runner = core_->ui_task_runner();
  ui_task_runner->DeleteSoon(FROM_HERE, core_.release());
}

void It2MeConfirmationDialogProxy::Show(
    const std::string& remote_user_email,
    It2MeConfirmationDialog::ResultCallback callback) {
  DCHECK(core_->caller_task_runner()->BelongsToCurrentThread());

  callback_ = std::move(callback);
  core_->ui_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&Core::Show, base::Unretained(core_.get()),
                                remote_user_email));
}

void It2MeConfirmationDialogProxy::SetDisableInputs(bool disable) {
  DCHECK(core_->caller_task_runner()->BelongsToCurrentThread());

  core_->ui_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&Core::SetDisableInputs,
                                base::Unretained(core_.get()), disable));
}

void It2MeConfirmationDialogProxy::ReportResult(
    It2MeConfirmationDialog::Result result) {
  DCHECK(core_->caller_task_runner()->BelongsToCurrentThread());
  std::move(callback_).Run(result);
}

}  // namespace remoting
