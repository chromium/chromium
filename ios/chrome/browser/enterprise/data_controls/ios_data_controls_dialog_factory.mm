// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_controls/ios_data_controls_dialog_factory.h"

#import <map>

#import "base/check.h"
#import "base/functional/callback.h"
#import "base/no_destructor.h"
#import "ios/chrome/browser/enterprise/data_controls/ios_data_controls_dialog.h"
#import "ios/web/public/web_state.h"

namespace data_controls {

namespace {

// Helper that keeps track of dialogs currently showing for given
// WebState-type pair.  These are used to determine if a call to
// `IOSDataControlsDialog::Show` is redundant or not. Keyed with `void*` instead
// of `web::WebState*` to avoid accidental bugs from dereferencing that pointer.
std::map<std::pair<void*, DataControlsDialog::Type>, IOSDataControlsDialog*>&
CurrentDialogsStorage() {
  static base::NoDestructor<std::map<std::pair<void*, DataControlsDialog::Type>,
                                     IOSDataControlsDialog*>>
      dialogs;
  return *dialogs;
}

// Returns null if no dialog is currently shown on `web_state` for `type`.
IOSDataControlsDialog* GetCurrentDialog(web::WebState* web_state,
                                        DataControlsDialog::Type type) {
  if (CurrentDialogsStorage().count({web_state, type})) {
    return CurrentDialogsStorage().at({web_state, type});
  }
  return nullptr;
}

}  // namespace

// static
IOSDataControlsDialogFactory* IOSDataControlsDialogFactory::GetInstance() {
  static base::NoDestructor<IOSDataControlsDialogFactory> instance;
  return instance.get();
}

IOSDataControlsDialogFactory::IOSDataControlsDialogFactory() = default;
IOSDataControlsDialogFactory::~IOSDataControlsDialogFactory() = default;

void IOSDataControlsDialogFactory::ShowDialogIfNeeded(
    web::WebState* web_state,
    DataControlsDialog::Type type) {
  ShowDialogIfNeeded(web_state, type,
                     base::OnceCallback<void(bool bypassed)>());
}

void IOSDataControlsDialogFactory::ShowDialogIfNeeded(
    web::WebState* web_state,
    DataControlsDialog::Type type,
    base::OnceCallback<void(bool bypassed)> callback) {
  DCHECK(web_state);

  // Don't show a new dialog if there is already an existing dialog of the same
  // type showing in `web_state` already. If `callback` is non-null, we add
  // it to the currently showing dialog.
  if (auto* dialog = GetCurrentDialog(web_state, type)) {
    if (callback) {
      dialog->callbacks_.push_back(std::move(callback));
    }
    return;
  }

  auto* dialog = CreateDialog(type, web_state, std::move(callback));

  std::pair<void*, DataControlsDialog::Type> key = {web_state, type};
  CurrentDialogsStorage()[key] = dialog;
  dialog->Show(base::BindOnce(
      [](std::pair<void*, DataControlsDialog::Type> key) {
        CurrentDialogsStorage().erase(key);
      },
      key));
}

IOSDataControlsDialog* IOSDataControlsDialogFactory::CreateDialog(
    DataControlsDialog::Type type,
    web::WebState* web_state,
    base::OnceCallback<void(bool bypassed)> callback) {
  return new IOSDataControlsDialog(type, web_state, std::move(callback));
}

}  // namespace data_controls
