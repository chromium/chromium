// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/ios_chrome_save_password_infobar_delegate.h"

#import <string>
#import <utility>

#import "base/memory/ptr_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/notreached.h"
#import "base/strings/strcat.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/ios/common/features.h"
#import "components/infobars/core/infobar.h"
#import "components/infobars/core/infobar_manager.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/password_form_manager_for_ui.h"
#import "components/password_manager/core/browser/password_form_metrics_recorder.h"
#import "components/password_manager/core/browser/password_manager_constants.h"
#import "components/password_manager/core/browser/password_ui_utils.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

namespace {

inline constexpr std::string_view kInfobarSaveDurationHistogramName =
    "PasswordManager.iOS.InfoBar.SaveDuration";

inline constexpr std::string_view kInfobarUpdateDurationHistogramName =
    "PasswordManager.iOS.InfoBar.UpdateDuration";

// Moment where the infobar is torn down.
enum class InfobarTearDownMoment {
  // Cover all moments.
  kAll,
  // After dismissing the infobar UI.
  kOnDimiss,
  // When deleting the infobar delegate.
  kOnDeletion
};

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
  // TODO(b/318820862): Consider removing this block as it is theoretically
  // impossible to save the password (e.g., tap on "Accept") before presenting.
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
    password_manager::features_util::PasswordAccountStorageUserState
        account_storage_user_state,
    bool update_infobar) {
  form_to_save->GetMetricsRecorder()->RecordUIDismissalReason(infobar_response);

  if (update_infobar) {
    password_manager::metrics_util::LogUpdateUIDismissalReason(
        infobar_response);
  } else {
    password_manager::metrics_util::LogSaveUIDismissalReason(
        infobar_response, account_storage_user_state,
        /*log_adoption_metric=*/false);
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

// Returns the infobar duration histogram name with the suffix that corresponds
// to `is_update` and `moment`.
std::string DurationHistogramName(bool is_update,
                                  InfobarTearDownMoment moment) {
  std::string_view base_name = is_update ? kInfobarUpdateDurationHistogramName
                                         : kInfobarSaveDurationHistogramName;

  switch (moment) {
    case InfobarTearDownMoment::kAll:
      return base::StrCat({base_name, ".All"});
    case InfobarTearDownMoment::kOnDimiss:
      return base::StrCat({base_name, ".OnDismiss"});
    case InfobarTearDownMoment::kOnDeletion:
      return base::StrCat({base_name, ".OnDeletion"});
  }

  NOTREACHED();
}

// Records the infobar duration metric for a given `moment`.
void RecordDurationAtMoment(bool is_update,
                            InfobarTearDownMoment moment,
                            base::TimeDelta duration) {
  UmaHistogramCustomTimes(DurationHistogramName(is_update, moment), duration,
                          base::Milliseconds(1), base::Seconds(25), 50);
}

}  // namespace

using password_manager::PasswordFormManagerForUI;

IOSChromeSavePasswordInfoBarDelegate::IOSChromeSavePasswordInfoBarDelegate(
    std::optional<std::string> account_to_store_password,
    bool password_update,
    password_manager::features_util::PasswordAccountStorageUserState
        account_storage_user_state,
    std::unique_ptr<PasswordFormManagerForUI> form_to_save,
    CommandDispatcher* dispatcher)
    : dispatcher_(dispatcher),
      form_to_save_(std::move(form_to_save)),
      infobar_type_(password_update
                        ? PasswordInfobarType::kPasswordInfobarTypeUpdate
                        : PasswordInfobarType::kPasswordInfobarTypeSave),
      account_to_store_password_(account_to_store_password),
      account_storage_user_state_(account_storage_user_state),
      password_update_(password_update) {}

IOSChromeSavePasswordInfoBarDelegate::~IOSChromeSavePasswordInfoBarDelegate() {
  if (IsPresenting()) {
    // If by any reason this delegate gets dealloc before the Infobar UI is
    // dismissed, record the dismissal metrics, which happens when navigating
    // away from the page presenting the infobar.
    RecordDismissalMetrics(form_to_save_.get(), infobar_response_,
                           account_storage_user_state_,
                           IsUpdateInfobar(infobar_type_));
    RecordInfobarDuration(/*on_dismiss=*/false);
  }
}

// static
IOSChromeSavePasswordInfoBarDelegate*
IOSChromeSavePasswordInfoBarDelegate::FromInfobarDelegate(
    infobars::InfoBarDelegate* delegate) {
  return delegate->GetIdentifier() == SAVE_PASSWORD_INFOBAR_DELEGATE_MOBILE
             ? static_cast<IOSChromeSavePasswordInfoBarDelegate*>(delegate)
             : nullptr;
}

NSString* IOSChromeSavePasswordInfoBarDelegate::GetUserNameText() const {
  return base::SysUTF16ToNSString(
      form_to_save_->GetPendingCredentials().username_value);
}

NSString* IOSChromeSavePasswordInfoBarDelegate::GetPasswordText() const {
  return base::SysUTF16ToNSString(
      form_to_save_->GetPendingCredentials().password_value);
}

NSString* IOSChromeSavePasswordInfoBarDelegate::GetURLHostText() const {
  return base::SysUTF8ToNSString(form_to_save_->GetURL().host());
}

std::optional<std::string>
IOSChromeSavePasswordInfoBarDelegate::GetAccountToStorePassword() const {
  return account_to_store_password_;
}

bool IOSChromeSavePasswordInfoBarDelegate::ShouldExpire(
    const NavigationDetails& details) const {
  const bool from_user_gesture =
      !base::FeatureList::IsEnabled(kAutofillStickyInfobarIos) ||
      details.has_user_gesture;

  return !details.is_form_submission && !details.is_redirect &&
         from_user_gesture && ConfirmInfoBarDelegate::ShouldExpire(details);
}

std::u16string IOSChromeSavePasswordInfoBarDelegate::GetMessageText() const {
  if (IsPasswordUpdate()) {
    return l10n_util::GetStringUTF16(IDS_IOS_PASSWORD_MANAGER_UPDATE_PASSWORD);
  }
  return l10n_util::GetStringUTF16(
      IDS_IOS_PASSWORD_MANAGER_SAVE_PASSWORD_PROMPT);
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
      NOTREACHED_IN_MIGRATION();
      return std::u16string();
  }
}

bool IOSChromeSavePasswordInfoBarDelegate::Accept() {
  DCHECK(form_to_save_);
  form_to_save_->Save();
  infobar_response_ = password_manager::metrics_util::CLICKED_ACCEPT;
  password_update_ = true;
  current_password_saved_ = true;
  return true;
}

bool IOSChromeSavePasswordInfoBarDelegate::Cancel() {
  DCHECK(form_to_save_);
  DCHECK(!password_update_);
  form_to_save_->Blocklist();
  infobar_response_ = password_manager::metrics_util::CLICKED_NEVER;
  return true;
}

void IOSChromeSavePasswordInfoBarDelegate::InfoBarDismissed() {
  DCHECK(form_to_save_);
  infobar_response_ = password_manager::metrics_util::CLICKED_CANCEL;
}

void IOSChromeSavePasswordInfoBarDelegate::UpdateCredentials(
    NSString* username,
    NSString* password) {
  const std::u16string username_string = base::SysNSStringToUTF16(username);
  const std::u16string password_string = base::SysNSStringToUTF16(password);
  UpdatePasswordFormUsernameAndPassword(username_string, password_string,
                                        form_to_save_.get());
}

void IOSChromeSavePasswordInfoBarDelegate::InfobarPresenting(bool automatic) {
  if (IsPresenting()) {
    return;
  }

  RecordPresentationMetrics(form_to_save_.get(), current_password_saved_,
                            IsUpdateInfobar(infobar_type_), automatic);
  start_timestamp_ = base::TimeTicks::Now();
}

void IOSChromeSavePasswordInfoBarDelegate::InfobarGone() {
  if (!IsPresenting()) {
    return;
  }

  RecordDismissalMetrics(form_to_save_.get(), infobar_response_,
                         account_storage_user_state_,
                         IsUpdateInfobar(infobar_type_));

  RecordInfobarDuration(/*on_dismiss=*/true);

  // Reset presentation state.
  infobar_response_ = password_manager::metrics_util::NO_DIRECT_INTERACTION;
  start_timestamp_ = std::nullopt;
}

bool IOSChromeSavePasswordInfoBarDelegate::IsPasswordUpdate() const {
  return password_update_;
}

bool IOSChromeSavePasswordInfoBarDelegate::IsCurrentPasswordSaved() const {
  return current_password_saved_;
}

infobars::InfoBarDelegate::InfoBarIdentifier
IOSChromeSavePasswordInfoBarDelegate::GetIdentifier() const {
  return SAVE_PASSWORD_INFOBAR_DELEGATE_MOBILE;
}

void IOSChromeSavePasswordInfoBarDelegate::RecordInfobarDuration(
    bool on_dismiss) {
  // Check that recording the infobar duration is only done when there is a
  // start timestamp and the infobar had been presenting.
  CHECK(start_timestamp_);

  bool is_update = IsUpdateInfobar(infobar_type_);
  base::TimeDelta duration = base::TimeTicks::Now() - *start_timestamp_;

  RecordDurationAtMoment(is_update, InfobarTearDownMoment::kAll, duration);

  if (on_dismiss) {
    RecordDurationAtMoment(is_update, InfobarTearDownMoment::kOnDimiss,
                           duration);
  } else {
    RecordDurationAtMoment(is_update, InfobarTearDownMoment::kOnDeletion,
                           duration);
  }
}

bool IOSChromeSavePasswordInfoBarDelegate::IsPresenting() const {
  return start_timestamp_.has_value();
}
