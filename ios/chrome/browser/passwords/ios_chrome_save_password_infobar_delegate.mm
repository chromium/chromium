// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/ios_chrome_save_password_infobar_delegate.h"

#import <utility>

#import "base/memory/ptr_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/infobars/core/infobar.h"
#import "components/infobars/core/infobar_manager.h"
#import "components/password_manager/core/browser/password_form_manager_for_ui.h"
#import "components/password_manager/core/browser/password_form_metrics_recorder.h"
#import "components/password_manager/core/browser/password_manager_constants.h"
#import "components/password_manager/core/browser/password_ui_utils.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Records Presentation Metrics for the Infobar Delegate.
// `current_password_saved` is true if the Infobar is on read-only mode after a
// Save/Update action has occured.
// `update_infobar` is YES if presenting an Update Infobar, NO if presenting a
// Save Infobar.
// `automatic` is YES the Infobar was presented automatically(e.g. The banner
// was presented), NO if the user triggered it  (e.g. Tapped onthe badge).
void RecordPresentationMetrics(
    password_manager::PasswordFormManagerForUI* form_to_save,
    bool current_password_saved,
    bool update_infobar,
    bool automatic) {
  if (current_password_saved) {
    // Password was already saved or updated.
    form_to_save->GetMetricsRecorder()->RecordPasswordBubbleShown(
        form_to_save->GetCredentialSource(),
        password_manager::metrics_util::MANUAL_MANAGE_PASSWORDS);
    password_manager::metrics_util::LogUIDisplayDisposition(
        password_manager::metrics_util::MANUAL_MANAGE_PASSWORDS);
    return;
  }

  if (update_infobar) {
    // Update Password.
    if (automatic) {
      form_to_save->GetMetricsRecorder()->RecordPasswordBubbleShown(
          form_to_save->GetCredentialSource(),
          password_manager::metrics_util::
              AUTOMATIC_WITH_PASSWORD_PENDING_UPDATE);
      password_manager::metrics_util::LogUIDisplayDisposition(
          password_manager::metrics_util::
              AUTOMATIC_WITH_PASSWORD_PENDING_UPDATE);
    } else {
      form_to_save->GetMetricsRecorder()->RecordPasswordBubbleShown(
          form_to_save->GetCredentialSource(),
          password_manager::metrics_util::MANUAL_WITH_PASSWORD_PENDING_UPDATE);
      password_manager::metrics_util::LogUIDisplayDisposition(
          password_manager::metrics_util::MANUAL_WITH_PASSWORD_PENDING_UPDATE);
    }
  } else {
    // Save Password.
    if (automatic) {
      form_to_save->GetMetricsRecorder()->RecordPasswordBubbleShown(
          form_to_save->GetCredentialSource(),
          password_manager::metrics_util::AUTOMATIC_WITH_PASSWORD_PENDING);
      password_manager::metrics_util::LogUIDisplayDisposition(
          password_manager::metrics_util::AUTOMATIC_WITH_PASSWORD_PENDING);
    } else {
      form_to_save->GetMetricsRecorder()->RecordPasswordBubbleShown(
          form_to_save->GetCredentialSource(),
          password_manager::metrics_util::MANUAL_WITH_PASSWORD_PENDING);
      password_manager::metrics_util::LogUIDisplayDisposition(
          password_manager::metrics_util::MANUAL_WITH_PASSWORD_PENDING);
    }
  }
}

// Records Dismissal Metrics for the Infobar Delegate.
// `infobar_response` is the action that was taken in order to dismiss the
// Infobar.
// `update_infobar` is YES if presenting an Update Infobar, NO if presenting a
// Save Infobar.
void RecordDismissalMetrics(
    password_manager::PasswordFormManagerForUI* form_to_save,
    password_manager::metrics_util::UIDismissalReason infobar_response,
    bool update_infobar) {
  form_to_save->GetMetricsRecorder()->RecordUIDismissalReason(infobar_response);

  if (update_infobar) {
    password_manager::metrics_util::LogUpdateUIDismissalReason(
        infobar_response,
        form_to_save->GetPendingCredentials().submission_event);
  } else {
    password_manager::metrics_util::LogSaveUIDismissalReason(
        infobar_response,
        form_to_save->GetPendingCredentials().submission_event,
        /*user_state=*/absl::nullopt);
  }
}

bool IsUpdateInfobar(PasswordInfobarType infobar_type) {
  switch (infobar_type) {
    case PasswordInfobarType::kPasswordInfobarTypeUpdate: {
      return YES;
    }
    case PasswordInfobarType::kPasswordInfobarTypeSave:
      return NO;
  }
}

}  // namespace

using password_manager::PasswordFormManagerForUI;

// static
IOSChromeSavePasswordInfoBarDelegate*
IOSChromeSavePasswordInfoBarDelegate::FromInfobarDelegate(
    infobars::InfoBarDelegate* delegate) {
  return delegate->GetIdentifier() == SAVE_PASSWORD_INFOBAR_DELEGATE_MOBILE
             ? static_cast<IOSChromeSavePasswordInfoBarDelegate*>(delegate)
             : nullptr;
}

IOSChromeSavePasswordInfoBarDelegate::IOSChromeSavePasswordInfoBarDelegate(
    NSString* user_email,
    bool is_sync_user,
    bool password_update,
    std::unique_ptr<PasswordFormManagerForUI> form_manager)
    : IOSChromePasswordManagerInfoBarDelegate(user_email,
                                              is_sync_user,
                                              std::move(form_manager)),
      password_update_(password_update),
      infobar_type_(password_update
                        ? PasswordInfobarType::kPasswordInfobarTypeUpdate
                        : PasswordInfobarType::kPasswordInfobarTypeSave) {}

IOSChromeSavePasswordInfoBarDelegate::~IOSChromeSavePasswordInfoBarDelegate() {
    // If by any reason this delegate gets dealloc before the Infobar is
    // dismissed, record the dismissal metrics.
    if (infobar_presenting_) {
      RecordDismissalMetrics(form_to_save(), infobar_response(),
                             IsUpdateInfobar(infobar_type_));
    }
}

infobars::InfoBarDelegate::InfoBarIdentifier
IOSChromeSavePasswordInfoBarDelegate::GetIdentifier() const {
  return SAVE_PASSWORD_INFOBAR_DELEGATE_MOBILE;
}

std::u16string IOSChromeSavePasswordInfoBarDelegate::GetMessageText() const {
  if (IsPasswordUpdate()) {
    return l10n_util::GetStringUTF16(IDS_IOS_PASSWORD_MANAGER_UPDATE_PASSWORD);
  }
  return l10n_util::GetStringUTF16(
      IDS_IOS_PASSWORD_MANAGER_SAVE_PASSWORD_PROMPT);
}

NSString* IOSChromeSavePasswordInfoBarDelegate::GetInfobarModalTitleText()
    const {
  return l10n_util::GetNSString(IDS_IOS_PASSWORD_MANAGER_SAVE_PASSWORD_TITLE);
}

std::u16string IOSChromeSavePasswordInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  switch (button) {
    case BUTTON_OK:
      return l10n_util::GetStringUTF16(
          IsPasswordUpdate() ? IDS_IOS_PASSWORD_MANAGER_UPDATE_BUTTON
                             : IDS_IOS_PASSWORD_MANAGER_SAVE_BUTTON);
    case BUTTON_CANCEL: {
      return IsPasswordUpdate()
                 ? std::u16string()
                 : l10n_util::GetStringUTF16(
                       IDS_IOS_PASSWORD_MANAGER_MODAL_BLOCK_BUTTON);
    }
    case BUTTON_NONE:
      NOTREACHED();
      return std::u16string();
  }
}

bool IOSChromeSavePasswordInfoBarDelegate::Accept() {
  DCHECK(form_to_save());
  form_to_save()->Save();
  set_infobar_response(password_manager::metrics_util::CLICKED_ACCEPT);
  password_update_ = true;
  current_password_saved_ = true;
  return true;
}

bool IOSChromeSavePasswordInfoBarDelegate::Cancel() {
  DCHECK(form_to_save());
  DCHECK(!password_update_);
  form_to_save()->Blocklist();
  set_infobar_response(password_manager::metrics_util::CLICKED_NEVER);
  return true;
}

void IOSChromeSavePasswordInfoBarDelegate::InfoBarDismissed() {
  DCHECK(form_to_save());
  set_infobar_response(password_manager::metrics_util::CLICKED_CANCEL);
}

bool IOSChromeSavePasswordInfoBarDelegate::ShouldExpire(
    const NavigationDetails& details) const {
  return !details.is_form_submission && !details.is_redirect &&
         ConfirmInfoBarDelegate::ShouldExpire(details);
}

void IOSChromeSavePasswordInfoBarDelegate::UpdateCredentials(
    NSString* username,
    NSString* password) {
  const std::u16string username_string = base::SysNSStringToUTF16(username);
  const std::u16string password_string = base::SysNSStringToUTF16(password);
  UpdatePasswordFormUsernameAndPassword(username_string, password_string,
                                        form_to_save());
}

void IOSChromeSavePasswordInfoBarDelegate::InfobarPresenting(bool automatic) {
  if (infobar_presenting_)
    return;

  RecordPresentationMetrics(form_to_save(), current_password_saved_,
                            IsUpdateInfobar(infobar_type_), automatic);
  infobar_presenting_ = YES;
}

void IOSChromeSavePasswordInfoBarDelegate::InfobarDismissed() {
  if (!infobar_presenting_)
    return;

  RecordDismissalMetrics(form_to_save(), infobar_response(),
                         IsUpdateInfobar(infobar_type_));
  // After the metrics have been recorded we can reset the response.
  set_infobar_response(password_manager::metrics_util::NO_DIRECT_INTERACTION);
  infobar_presenting_ = NO;
}

bool IOSChromeSavePasswordInfoBarDelegate::IsPasswordUpdate() const {
  return password_update_;
}

bool IOSChromeSavePasswordInfoBarDelegate::IsCurrentPasswordSaved() const {
  return current_password_saved_;
}
