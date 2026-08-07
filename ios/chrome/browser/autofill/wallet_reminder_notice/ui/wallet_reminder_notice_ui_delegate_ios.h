// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_WALLET_REMINDER_NOTICE_UI_WALLET_REMINDER_NOTICE_UI_DELEGATE_IOS_H_
#define IOS_CHROME_BROWSER_AUTOFILL_WALLET_REMINDER_NOTICE_UI_WALLET_REMINDER_NOTICE_UI_DELEGATE_IOS_H_

#import <Foundation/Foundation.h>

#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/ui/payments/wallet_reminder_notice_ui_delegate.h"

@protocol AutofillCommands;

namespace web {
class WebState;
}  // namespace web

namespace autofill {
namespace payments {

// iOS implementation of WalletReminderNoticeUiDelegate. Dispatches UI display
// requests directly to `commands_handler_`.
class WalletReminderNoticeUiDelegateIOS
    : public WalletReminderNoticeUiDelegate {
 public:
  WalletReminderNoticeUiDelegateIOS(web::WebState* web_state,
                                    id<AutofillCommands> commands_handler);
  ~WalletReminderNoticeUiDelegateIOS() override;

  WalletReminderNoticeUiDelegateIOS(const WalletReminderNoticeUiDelegateIOS&) =
      delete;
  WalletReminderNoticeUiDelegateIOS& operator=(
      const WalletReminderNoticeUiDelegateIOS&) = delete;

  // WalletReminderNoticeUiDelegate implementation.
  void ShowWalletReminderNotice(LegalMessageLines legal_message_lines) override;

 private:
  base::WeakPtr<web::WebState> web_state_ = nullptr;
  __weak id<AutofillCommands> commands_handler_ = nil;
};

}  // namespace payments
}  // namespace autofill

#endif  // IOS_CHROME_BROWSER_AUTOFILL_WALLET_REMINDER_NOTICE_UI_WALLET_REMINDER_NOTICE_UI_DELEGATE_IOS_H_
