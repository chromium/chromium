// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/forms_ai_private_inference_infobar_delegate_ios.h"

#import "base/time/time.h"
#import "components/autofill/core/common/autofill_prefs.h"
#import "components/grit/components_scaled_resources.h"
#import "components/prefs/pref_service.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/models/image_model.h"

FormsAiPrivateInferenceInfoBarDelegateIOS::
    FormsAiPrivateInferenceInfoBarDelegateIOS(PrefService* prefs)
    : prefs_(prefs) {}

FormsAiPrivateInferenceInfoBarDelegateIOS::
    ~FormsAiPrivateInferenceInfoBarDelegateIOS() = default;

infobars::InfoBarDelegate::InfoBarIdentifier
FormsAiPrivateInferenceInfoBarDelegateIOS::GetIdentifier() const {
  return FORMS_AI_PRIVATE_INFERENCE_INFOBAR_DELEGATE_IOS;
}

ui::ImageModel FormsAiPrivateInferenceInfoBarDelegateIOS::GetIcon() const {
  return ui::ImageModel::FromResourceId(IDR_INFOBAR_AUTOFILL_CC);
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
  prefs_->SetTime(
      autofill::prefs::kAutofillAiPrivateInferenceNoticeAcknowledgedTimestamp,
      base::Time::Now());
  return true;
}

void FormsAiPrivateInferenceInfoBarDelegateIOS::InfoBarDismissed() {}
