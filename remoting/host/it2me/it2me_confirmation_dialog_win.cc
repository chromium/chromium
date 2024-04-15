// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/it2me/it2me_confirmation_dialog.h"

#include <windows.h>

#include <commctrl.h>

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/message_formatter.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "remoting/host/win/core_resource.h"
#include "remoting/host/win/simple_task_dialog.h"

namespace remoting {

namespace {

// Time to wait before closing the dialog and cancelling the connection.
constexpr base::TimeDelta kDialogTimeout = base::Minutes(1);

class It2MeConfirmationDialogWin : public It2MeConfirmationDialog {
 public:
  It2MeConfirmationDialogWin();

  It2MeConfirmationDialogWin(const It2MeConfirmationDialogWin&) = delete;
  It2MeConfirmationDialogWin& operator=(const It2MeConfirmationDialogWin&) =
      delete;

  ~It2MeConfirmationDialogWin() override;

  // It2MeConfirmationDialog implementation.
  void Show(const std::string& remote_user_email,
            ResultCallback callback) override;
};

It2MeConfirmationDialogWin::It2MeConfirmationDialogWin() = default;

It2MeConfirmationDialogWin::~It2MeConfirmationDialogWin() = default;

void It2MeConfirmationDialogWin::Show(const std::string& remote_user_email,
                                      ResultCallback callback) {
  DCHECK(!remote_user_email.empty());
  DCHECK(callback);

  // Default to a cancelled state.  We only accept the connection if the user
  // explicitly allows it.
  Result result = Result::CANCEL;

  // |resource_module| does not need to be freed as GetModuleHandle() does not
  // increment the refcount for the module.  This DLL is not unloaded until the
  // process exits so using a stored handle is safe.
  HMODULE resource_module = GetModuleHandle(L"remoting_core.dll");
  if (resource_module == nullptr) {
    PLOG(ERROR) << "GetModuleHandle() failed";
    std::move(callback).Run(result);
    return;
  }

  SimpleTaskDialog task_dialog(resource_module);
  if (!task_dialog.SetTitleTextWithStringId(IDS_PRODUCT_NAME) ||
      !task_dialog.AppendButtonWithStringId(IDYES,
                                            IDS_SHARE_CONFIRM_DIALOG_CONFIRM) ||
      !task_dialog.AppendButtonWithStringId(IDNO,
                                            IDS_SHARE_CONFIRM_DIALOG_DECLINE)) {
    LOG(ERROR) << "Failed to load text for confirmation dialog.";
    std::move(callback).Run(result);
    return;
  }

  const wchar_t* message_stringw = nullptr;
  int string_length = LoadStringW(
      resource_module, IDS_SHARE_CONFIRM_DIALOG_MESSAGE_WITH_USERNAME,
      reinterpret_cast<wchar_t*>(&message_stringw),
      /*cchBufferMax=*/0);
  if (string_length <= 0) {
    PLOG(ERROR) << "Failed to load message text for confirmation dialog.";
    std::move(callback).Run(result);
    return;
  }
  std::wstring message_text =
      base::AsWString(base::i18n::MessageFormatter::FormatWithNumberedArgs(
          base::AsStringPiece16(
              std::wstring_view(message_stringw, string_length)),
          base::UTF8ToUTF16(remote_user_email)));

  task_dialog.set_message_text(message_text);
  task_dialog.set_default_button(IDNO);
  task_dialog.set_dialog_timeout(kDialogTimeout);
  std::optional<int> button_result = task_dialog.Show();

  if (!button_result.has_value()) {
    std::move(callback).Run(result);
    return;
  }

  if (*button_result == IDYES) {
    // Only accept the connection if the user chose 'share'.
    result = Result::OK;
  }

  std::move(callback).Run(result);
}

}  // namespace

std::unique_ptr<It2MeConfirmationDialog>
It2MeConfirmationDialogFactory::Create() {
  return std::make_unique<It2MeConfirmationDialogWin>();
}

}  // namespace remoting
