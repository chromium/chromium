// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/autofill_ai_save_entity_infobar_delegate_ios.h"

#import "base/feature_list.h"
#import "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#import "components/autofill/ios/common/features.h"
#import "components/grit/components_scaled_resources.h"
#import "components/infobars/core/infobar_delegate.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/autofill_ai/public/autofill_ai_ui_util.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/models/image_model.h"
#import "ui/gfx/image/image.h"

namespace {
constexpr CGFloat kAutofillAiInfobarSymbolPointSize = 24.0;
}  // namespace

namespace autofill {

AutofillAiSaveEntityInfoBarDelegateIOS::AutofillAiSaveEntityInfoBarDelegateIOS(
    SaveEntityParams params,
    base::OnceClosure on_accept_action)
    : params_(std::move(params)),
      accept_callback_(std::move(on_accept_action)) {
  UIImage* image = nil;
  if (params.new_entity.record_type() ==
      EntityInstance::RecordType::kServerWallet) {
    image = GetWalletLogo(kAutofillAiInfobarSymbolPointSize,
                          [UIColor colorNamed:kBlue600Color]);
  } else {
    image = DefaultIconForAutofillAiEntityType(
        params.new_entity.type().name(), kAutofillAiInfobarSymbolPointSize,
        [UIColor colorNamed:kBlue600Color]);
  }

  if (image) {
    icon_ = ui::ImageModel::FromImage(gfx::Image(image));
  } else {
    icon_ = ui::ImageModel::FromResourceId(GetIconId());
  }
}

AutofillAiSaveEntityInfoBarDelegateIOS::
    ~AutofillAiSaveEntityInfoBarDelegateIOS() {
  if (!params_.callback.is_null() && accept_callback_) {
    std::move(params_.callback)
        .Run(AutofillClient::AutofillAiBubbleResult::kUnknown, std::nullopt,
             {});
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
  return icon_;
}

std::u16string AutofillAiSaveEntityInfoBarDelegateIOS::GetTitleText() const {
  return params_.GetTitleText();
}

std::u16string AutofillAiSaveEntityInfoBarDelegateIOS::GetMessageText() const {
  return params_.GetMessageText();
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
  if (!params_.callback.is_null()) {
    std::move(params_.callback)
        .Run(AutofillClient::AutofillAiBubbleResult::kCancelled, std::nullopt,
             {});
  }
  return true;
}

void AutofillAiSaveEntityInfoBarDelegateIOS::InfoBarDismissed() {
  if (!params_.callback.is_null()) {
    std::move(params_.callback)
        .Run(AutofillClient::AutofillAiBubbleResult::kClosed, std::nullopt, {});
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

bool AutofillAiSaveEntityInfoBarDelegateIOS::UseIconBackgroundTint() const {
  return false;
}

}  // namespace autofill
