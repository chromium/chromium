// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/autofill_ai_save_entity_infobar_delegate_ios.h"

#import "base/feature_list.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#import "components/autofill/ios/common/features.h"
#import "components/grit/components_scaled_resources.h"
#import "components/infobars/core/infobar_delegate.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/model/autofill_ai_util.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/models/image_model.h"
#import "ui/gfx/image/image.h"

namespace autofill {

AutofillAiSaveEntityInfoBarDelegateIOS::AutofillAiSaveEntityInfoBarDelegateIOS(
    EntityInstance new_entity,
    std::optional<EntityInstance> old_entity,
    std::u16string user_email,
    base::OnceClosure on_accept_action,
    AutofillClient::EntityImportPromptResultCallback callback)
    : new_entity_(std::move(new_entity)),
      old_entity_(std::move(old_entity)),
      callback_(std::move(callback)),
      accept_callback_(std::move(on_accept_action)),
      user_email_(std::move(user_email)) {}

AutofillAiSaveEntityInfoBarDelegateIOS::
    ~AutofillAiSaveEntityInfoBarDelegateIOS() {
  if (!callback_.is_null() && accept_callback_) {
    std::move(callback_).Run(AutofillClient::AutofillAiBubbleResult::kUnknown);
  }
}

infobars::InfoBarDelegate::InfoBarIdentifier
AutofillAiSaveEntityInfoBarDelegateIOS::GetIdentifier() const {
  return infobars::InfoBarDelegate::
      AUTOFILL_AI_SAVE_ENTITY_INFOBAR_DELEGATE_IOS;
}

int AutofillAiSaveEntityInfoBarDelegateIOS::GetIconId() const {
  return IDR_INFOBAR_AUTOFILL_CC;
}

ui::ImageModel AutofillAiSaveEntityInfoBarDelegateIOS::GetIcon() const {
  if (new_entity_.record_type() == EntityInstance::RecordType::kServerWallet) {
#if BUILDFLAG(IOS_USE_BRANDED_ASSETS)
    return ui::ImageModel::FromImage(gfx::Image(CustomSymbolWithPointSize(
        kGoogleWalletSymbol, kInfobarSymbolPointSize)));
#endif
  }

  UIImage* image = DefaultIconForAutofillAiEntityType(new_entity_.type().name(),
                                                      kInfobarSymbolPointSize);
  if (image) {
    return ui::ImageModel::FromImage(gfx::Image(image));
  }

  return ui::ImageModel::FromResourceId(GetIconId());
}

std::u16string AutofillAiSaveEntityInfoBarDelegateIOS::GetTitleText() const {
  return new_entity_.type().GetNameForI18n();
}

std::u16string AutofillAiSaveEntityInfoBarDelegateIOS::GetMessageText() const {
  if (new_entity_.record_type() == EntityInstance::RecordType::kServerWallet) {
    return l10n_util::GetStringFUTF16(IDS_IOS_INFOBAR_MESSAGE_SAVE_TO_WALLET,
                                      user_email_);
  } else {
    return l10n_util::GetStringUTF16(IDS_IOS_INFOBAR_MESSAGE_SAVE_TO_DEVICE);
  }
}

std::u16string AutofillAiSaveEntityInfoBarDelegateIOS::GetButtonLabel(
    InfoBarButton button) const {
  if (button == BUTTON_OK) {
    return l10n_util::GetStringUTF16(
        IsUpdate() ? IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_OK_BUTTON_LABEL
                   : IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_OK_BUTTON_LABEL);
  }
  return button == BUTTON_CANCEL
             ? l10n_util::GetStringUTF16(
                   IDS_IOS_CREDENTIAL_PROVIDER_PROMO_NO_THANKS)
             : u"";
}

bool AutofillAiSaveEntityInfoBarDelegateIOS::Accept() {
  if (!accept_callback_.is_null()) {
    std::move(accept_callback_).Run();
  }
  return true;
}

bool AutofillAiSaveEntityInfoBarDelegateIOS::Cancel() {
  if (!callback_.is_null()) {
    std::move(callback_).Run(
        AutofillClient::AutofillAiBubbleResult::kCancelled);
  }
  return true;
}

void AutofillAiSaveEntityInfoBarDelegateIOS::InfoBarDismissed() {
  if (!callback_.is_null()) {
    std::move(callback_).Run(AutofillClient::AutofillAiBubbleResult::kClosed);
  }
}

bool AutofillAiSaveEntityInfoBarDelegateIOS::ShouldExpire(
    const NavigationDetails& details) const {
  const bool from_user_gesture =
      !base::FeatureList::IsEnabled(kAutofillStickyInfobarIos) ||
      details.has_user_gesture;

  // Expire the Infobar unless the navigation was triggered by the form that
  // presented the Infobar, or the navigation is a redirect.
  // Also, expire the infobar if the navigation is to a different page.
  return !details.is_form_submission && !details.is_redirect &&
         from_user_gesture && ConfirmInfoBarDelegate::ShouldExpire(details);
}

infobars::InfoBarDelegate::InfobarPriority
AutofillAiSaveEntityInfoBarDelegateIOS::GetPriority() const {
  return InfobarPriority::kDefault;
}

}  // namespace autofill
