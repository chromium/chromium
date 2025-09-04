// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_controls/ios_data_controls_dialog.h"

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"

namespace data_controls {

IOSDataControlsDialog::IOSDataControlsDialog(
    Type type,
    web::WebState* web_state,
    base::OnceCallback<void(bool)> callback)
    : DataControlsDialog(type, std::move(callback)), web_state_(web_state) {
  DCHECK(web_state_);
  web_state_->AddObserver(this);
}

IOSDataControlsDialog::~IOSDataControlsDialog() {
  if (web_state_) {
    web_state_->RemoveObserver(this);
  }
  web_state_ = nullptr;
}

void IOSDataControlsDialog::Show(base::OnceClosure on_destructed) {
  // TODO(crbug.com/438202192): This is a placeholder for the displaying IOS
  // enterprise copy/paste warning dialogs.
}

std::u16string IOSDataControlsDialog::GetDialogTitle() const {
  // TODO(crbug.com/438202192): Placeholder for the getting the dialog title.
  return std::u16string();
}

std::u16string IOSDataControlsDialog::GetDialogLabel() const {
  // TODO(crbug.com/438202192): Placeholder for the getting the dialog label.
  return std::u16string();
}

}  // namespace data_controls
