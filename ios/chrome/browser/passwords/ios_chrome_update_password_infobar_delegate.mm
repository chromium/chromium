// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/passwords/ios_chrome_update_password_infobar_delegate.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/password_manager/core/browser/password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/password_form_metrics_recorder.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/infobars/infobar.h"
#import "ios/chrome/browser/passwords/update_password_infobar_controller.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using password_manager::PasswordFormManagerForUI;

// static
void IOSChromeUpdatePasswordInfoBarDelegate::Create(
    bool is_sync_user,
    infobars::InfoBarManager* infobar_manager,
    std::unique_ptr<PasswordFormManagerForUI> form_manager,
    UIViewController* baseViewController,
    id<ApplicationCommands> dispatcher) {
  DCHECK(infobar_manager);
  auto delegate = base::WrapUnique(new IOSChromeUpdatePasswordInfoBarDelegate(
      is_sync_user, std::move(form_manager)));
  delegate->set_dispatcher(dispatcher);

  UpdatePasswordInfoBarController* controller =
      [[UpdatePasswordInfoBarController alloc]
          initWithBaseViewController:baseViewController
                     infoBarDelegate:delegate.get()];
  infobar_manager->AddInfoBar(
      std::make_unique<InfoBarIOS>(controller, std::move(delegate)));
}

IOSChromeUpdatePasswordInfoBarDelegate::
    ~IOSChromeUpdatePasswordInfoBarDelegate() {
  password_manager::metrics_util::LogUpdateUIDismissalReason(
      infobar_response());
  form_to_save()->GetMetricsRecorder()->RecordUIDismissalReason(
      infobar_response());
}

IOSChromeUpdatePasswordInfoBarDelegate::IOSChromeUpdatePasswordInfoBarDelegate(
    bool is_sync_user,
    std::unique_ptr<PasswordFormManagerForUI> form_manager)
    : IOSChromePasswordManagerInfoBarDelegate(is_sync_user,
                                              std::move(form_manager)) {
  selected_account_ = form_to_save()->GetPendingCredentials().username_value;
  form_to_save()->GetMetricsRecorder()->RecordPasswordBubbleShown(
      form_to_save()->GetCredentialSource(),
      password_manager::metrics_util::AUTOMATIC_WITH_PASSWORD_PENDING_UPDATE);
  password_manager::metrics_util::LogUIDisplayDisposition(
      password_manager::metrics_util::AUTOMATIC_WITH_PASSWORD_PENDING_UPDATE);
}

bool IOSChromeUpdatePasswordInfoBarDelegate::ShowMultipleAccounts() const {
  return form_to_save()->GetBestMatches().size() > 1;
}

NSArray* IOSChromeUpdatePasswordInfoBarDelegate::GetAccounts() const {
  NSMutableArray* usernames = [NSMutableArray array];
  for (const auto* match : form_to_save()->GetBestMatches()) {
    [usernames addObject:base::SysUTF16ToNSString(match->username_value)];
  }
  return usernames;
}

infobars::InfoBarDelegate::InfoBarIdentifier
IOSChromeUpdatePasswordInfoBarDelegate::GetIdentifier() const {
  return UPDATE_PASSWORD_INFOBAR_DELEGATE_MOBILE;
}

base::string16 IOSChromeUpdatePasswordInfoBarDelegate::GetMessageText() const {
  return selected_account_.length() > 0
             ? l10n_util::GetStringFUTF16(
                   IDS_IOS_PASSWORD_MANAGER_UPDATE_PASSWORD_FOR_ACCOUNT,
                   selected_account_)
             : l10n_util::GetStringUTF16(
                   IDS_IOS_PASSWORD_MANAGER_UPDATE_PASSWORD);
}

int IOSChromeUpdatePasswordInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}

base::string16 IOSChromeUpdatePasswordInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  DCHECK_EQ(BUTTON_OK, button);
  return l10n_util::GetStringUTF16(IDS_IOS_PASSWORD_MANAGER_UPDATE_BUTTON);
}

bool IOSChromeUpdatePasswordInfoBarDelegate::Accept() {
  DCHECK(form_to_save());
  UpdatePasswordFormUsernameAndPassword(
      selected_account_, form_to_save()->GetPendingCredentials().password_value,
      form_to_save());
  form_to_save()->Save();
  set_infobar_response(password_manager::metrics_util::CLICKED_SAVE);
  return true;
}

void IOSChromeUpdatePasswordInfoBarDelegate::InfoBarDismissed() {
  DCHECK(form_to_save());
  set_infobar_response(password_manager::metrics_util::CLICKED_CANCEL);
}

base::string16 IOSChromeUpdatePasswordInfoBarDelegate::GetLinkText() const {
  return ShowMultipleAccounts() ? selected_account_ : base::string16();
}
