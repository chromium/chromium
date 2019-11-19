// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/it2me/it2me_confirmation_dialog_proxy.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/threading/thread_task_runner_handle.h"

namespace remoting {

class It2MeConfirmationDialogProxy::Core {
 public:
  Core(scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
       scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
       base::WeakPtr<It2MeConfirmationDialogProxy> parent,
       std::unique_ptr<It2MeConfirmationDialog> dialog);
  ~Core();

  // Shows the wrapped dialog. Must be called on the UI thread.
  void Show(const std::string& remote_user_email);

  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner() {
    return ui_task_runner_;
  }

  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner() {
    return caller_task_runner_;
  }

 private:
  // Reports the dialog result on the caller's thread.
  void ReportResult(It2MeConfirmationDialog::Result result);

  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;
  base::WeakPtr<It2MeConfirmationDialogProxy> parent_;
  std::unique_ptr<It2MeConfirmationDialog> dialog_;

  DISALLOW_COPY_AND_ASSIGN(Core);
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

  dialog_->Show(remote_user_email,
                base::Bind(&It2MeConfirmationDialogProxy::Core::ReportResult,
                           base::Unretained(this)));
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
  core_.reset(new Core(ui_task_runner, base::ThreadTaskRunnerHandle::Get(),
                       weak_factory_.GetWeakPtr(), std::move(dialog)));
}

It2MeConfirmationDialogProxy::~It2MeConfirmationDialogProxy() {
  DCHECK(core_->caller_task_runner()->BelongsToCurrentThread());

  auto ui_task_runner = core_->ui_task_runner();
  ui_task_runner->DeleteSoon(FROM_HERE, core_.release());
}

void It2MeConfirmationDialogProxy::Show(
    const std::string& remote_user_email,
    const It2MeConfirmationDialog::ResultCallback& callback) {
  DCHECK(core_->caller_task_runner()->BelongsToCurrentThread());

  callback_ = callback;
  core_->ui_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&Core::Show, base::Unretained(core_.get()),
                                remote_user_email));
}

void It2MeConfirmationDialogProxy::ReportResult(
    It2MeConfirmationDialog::Result result) {
  DCHECK(core_->caller_task_runner()->BelongsToCurrentThread());
  std::move(callback_).Run(result);
}

}  // namespace remoting
