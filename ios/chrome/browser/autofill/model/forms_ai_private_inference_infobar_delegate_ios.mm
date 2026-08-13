// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/forms_ai_private_inference_infobar_delegate_ios.h"

#import "base/metrics/histogram_functions.h"
#import "base/time/time.h"
#import "components/autofill/core/common/autofill_prefs.h"
#import "components/grit/components_scaled_resources.h"
#import "components/prefs/pref_service.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/models/image_model.h"
#import "ui/gfx/image/image.h"

namespace {

// Outcomes of interaction with the Forms AI Private Inference notice infobar.
// LINT.IfChange(PopupNoticeInteractions)
enum class PopupNoticeInteractions {
  kShown = 0,
  kAcknowledged = 1,
  kDismissed = 2,
  kLinkButtonClicked = 3,
  kMaxValue = kLinkButtonClicked,
};
// LINT.ThenChange(tools/metrics/histograms/metadata/personal_context/enums.xml:PopupNoticeInteractions)

}  // namespace

FormsAiPrivateInferenceInfoBarDelegateIOS::
    FormsAiPrivateInferenceInfoBarDelegateIOS(PrefService* prefs)
    : prefs_(prefs) {
  base::UmaHistogramEnumeration(
      "Autofill.Ai.PrivateInferenceNoticeInteractions",
      PopupNoticeInteractions::kShown);
}

FormsAiPrivateInferenceInfoBarDelegateIOS::
    ~FormsAiPrivateInferenceInfoBarDelegateIOS() = default;

infobars::InfoBarDelegate::InfoBarIdentifier
FormsAiPrivateInferenceInfoBarDelegateIOS::GetIdentifier() const {
  return FORMS_AI_PRIVATE_INFERENCE_INFOBAR_DELEGATE_IOS;
}

ui::ImageModel FormsAiPrivateInferenceInfoBarDelegateIOS::GetIcon() const {
  UIImage* image =
      SymbolWithPointSize(SymbolChromeProduct, kInfobarSymbolPointSize);
  return ui::ImageModel::FromImage(gfx::Image(image));
}

std::u16string FormsAiPrivateInferenceInfoBarDelegateIOS::GetTitleText() const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_AI_PRIVATE_INFERENCE_NOTICE_TITLE);
}

std::u16string FormsAiPrivateInferenceInfoBarDelegateIOS::GetMessageText()
    const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_AI_PRIVATE_INFERENCE_NOTICE_DESCRIPTION);
}

int FormsAiPrivateInferenceInfoBarDelegateIOS::GetButtons() const {
  return BUTTON_OK;
}

std::u16string FormsAiPrivateInferenceInfoBarDelegateIOS::GetButtonLabel(
    InfoBarButton button) const {
  DCHECK_EQ(button, BUTTON_OK);
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_AI_PRIVATE_INFERENCE_NOTICE_PRIMARY_BUTTON_TEXT);
}

bool FormsAiPrivateInferenceInfoBarDelegateIOS::Accept() {
  interaction_logged_ = true;
  base::UmaHistogramEnumeration(
      "Autofill.Ai.PrivateInferenceNoticeInteractions",
      PopupNoticeInteractions::kAcknowledged);
  prefs_->SetTime(
      autofill::prefs::kAutofillAiPrivateInferenceNoticeAcknowledgedTimestamp,
      base::Time::Now());
  return true;
}

void FormsAiPrivateInferenceInfoBarDelegateIOS::InfoBarDismissed() {
  if (!interaction_logged_) {
    base::UmaHistogramEnumeration(
        "Autofill.Ai.PrivateInferenceNoticeInteractions",
        PopupNoticeInteractions::kDismissed);
  }
}

void FormsAiPrivateInferenceInfoBarDelegateIOS::OnSettingsLinkClicked() {
  interaction_logged_ = true;
  base::UmaHistogramEnumeration(
      "Autofill.Ai.PrivateInferenceNoticeInteractions",
      PopupNoticeInteractions::kLinkButtonClicked);
}
