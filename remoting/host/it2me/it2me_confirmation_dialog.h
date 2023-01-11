// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_IT2ME_IT2ME_CONFIRMATION_DIALOG_H_
#define REMOTING_HOST_IT2ME_IT2ME_CONFIRMATION_DIALOG_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"

namespace remoting {

// Interface for a dialog to confirm an It2Me session with the user.
// All methods, with the exception of the constructor, are guaranteed to be
// called on the UI thread.
class It2MeConfirmationDialog {
 public:
  enum class Result { OK, CANCEL };
  enum class DialogStyle {
    kEnterprise,
    kConsumer,
  };

  typedef base::OnceCallback<void(Result)> ResultCallback;

  virtual ~It2MeConfirmationDialog() = default;

  // Shows the dialog. |callback| will be called with the user's selection.
  // |callback| will not be called if the dialog is destroyed.
  virtual void Show(const std::string& remote_user_email,
                    ResultCallback callback) = 0;
};

class It2MeConfirmationDialogFactory {
 public:
  explicit It2MeConfirmationDialogFactory(
      It2MeConfirmationDialog::DialogStyle dialog_style)
      : dialog_style_(dialog_style) {}

  It2MeConfirmationDialogFactory(const It2MeConfirmationDialogFactory&) =
      delete;
  It2MeConfirmationDialogFactory& operator=(
      const It2MeConfirmationDialogFactory&) = delete;

  virtual ~It2MeConfirmationDialogFactory() = default;

  virtual std::unique_ptr<It2MeConfirmationDialog> Create();

 private:
  // This field is only used on ChromeOS.
  [[maybe_unused]] It2MeConfirmationDialog::DialogStyle dialog_style_ =
      It2MeConfirmationDialog::DialogStyle::kConsumer;
};

}  // namespace remoting

#endif  // REMOTING_HOST_IT2ME_IT2ME_CONFIRMATION_DIALOG_H_
