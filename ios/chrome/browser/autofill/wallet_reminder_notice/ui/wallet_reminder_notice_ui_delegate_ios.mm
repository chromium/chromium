// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/wallet_reminder_notice/ui/wallet_reminder_notice_ui_delegate_ios.h"

#import <utility>

#import "ios/chrome/browser/shared/public/commands/autofill_commands.h"
#import "ios/web/public/web_state.h"

namespace autofill {
namespace payments {

WalletReminderNoticeUiDelegateIOS::WalletReminderNoticeUiDelegateIOS(
    web::WebState* web_state,
    id<AutofillCommands> commands_handler)
    : web_state_(web_state ? web_state->GetWeakPtr() : nullptr),
      commands_handler_(commands_handler) {}

WalletReminderNoticeUiDelegateIOS::~WalletReminderNoticeUiDelegateIOS() =
    default;

void WalletReminderNoticeUiDelegateIOS::ShowWalletReminderNotice(
    LegalMessageLines legal_message_lines) {
  if (commands_handler_ && web_state_) {
    [commands_handler_
        showWalletReminderNoticeOnOriginWebState:web_state_.get()
                               legalMessageLines:std::move(
                                                     legal_message_lines)];
  }
}

}  // namespace payments
}  // namespace autofill
