// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/ios_chrome_save_password_infobar_delegate.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/password_manager/core/browser/password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/password_form_metrics_recorder.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/infobars/infobar.h"
#import "ios/chrome/browser/passwords/ios_password_infobar_controller.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using password_manager::PasswordFormManagerForUI;

// static
void IOSChromeSavePasswordInfoBarDelegate::Create(
    bool is_sync_user,
    infobars::InfoBarManager* infobar_manager,
    std::unique_ptr<PasswordFormManagerForUI> form_to_save,
    id<ApplicationCommands> dispatcher) {
  DCHECK(infobar_manager);
  auto delegate = base::WrapUnique(new IOSChromeSavePasswordInfoBarDelegate(
      is_sync_user, std::move(form_to_save)));
  delegate->set_dispatcher(dispatcher);
  IOSPasswordInfoBarController* controller =
      [[IOSPasswordInfoBarController alloc]
          initWithInfoBarDelegate:delegate.get()];
  infobar_manager->AddInfoBar(
      std::make_unique<InfoBarIOS>(controller, std::move(delegate)));
}

IOSChromeSavePasswordInfoBarDelegate::~IOSChromeSavePasswordInfoBarDelegate() {
  password_manager::metrics_util::LogSaveUIDismissalReason(infobar_response());
  form_to_save()->GetMetricsRecorder()->RecordUIDismissalReason(
      infobar_response());
}

IOSChromeSavePasswordInfoBarDelegate::IOSChromeSavePasswordInfoBarDelegate(
    bool is_sync_user,
    std::unique_ptr<PasswordFormManagerForUI> form_manager)
    : IOSChromePasswordManagerInfoBarDelegate(is_sync_user,
                                              std::move(form_manager)) {
  form_to_save()->GetMetricsRecorder()->RecordPasswordBubbleShown(
      form_to_save()->GetCredentialSource(),
      password_manager::metrics_util::AUTOMATIC_WITH_PASSWORD_PENDING);
}

infobars::InfoBarDelegate::InfoBarIdentifier
IOSChromeSavePasswordInfoBarDelegate::GetIdentifier() const {
  return SAVE_PASSWORD_INFOBAR_DELEGATE_MOBILE;
}

base::string16 IOSChromeSavePasswordInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(
      IDS_IOS_PASSWORD_MANAGER_SAVE_PASSWORD_PROMPT);
}

base::string16 IOSChromeSavePasswordInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16(
      (button == BUTTON_OK) ? IDS_IOS_PASSWORD_MANAGER_SAVE_BUTTON
                            : IDS_IOS_PASSWORD_MANAGER_BLACKLIST_BUTTON);
}

bool IOSChromeSavePasswordInfoBarDelegate::Accept() {
  DCHECK(form_to_save());
  form_to_save()->Save();
  set_infobar_response(password_manager::metrics_util::CLICKED_SAVE);
  return true;
}

bool IOSChromeSavePasswordInfoBarDelegate::Cancel() {
  DCHECK(form_to_save());
  form_to_save()->PermanentlyBlacklist();
  set_infobar_response(password_manager::metrics_util::CLICKED_NEVER);
  return true;
}

bool IOSChromeSavePasswordInfoBarDelegate::ShouldExpire(
    const NavigationDetails& details) const {
  return !details.is_redirect && ConfirmInfoBarDelegate::ShouldExpire(details);
}
