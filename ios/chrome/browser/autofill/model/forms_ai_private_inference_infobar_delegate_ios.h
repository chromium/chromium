// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MODEL_FORMS_AI_PRIVATE_INFERENCE_INFOBAR_DELEGATE_IOS_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MODEL_FORMS_AI_PRIVATE_INFERENCE_INFOBAR_DELEGATE_IOS_H_

#import "base/memory/raw_ptr.h"
#import "components/infobars/core/confirm_infobar_delegate.h"

class PrefService;

// Infobar delegate used to present a notice to the user that page content
// will be processed privately in the cloud for Private Inference.
class FormsAiPrivateInferenceInfoBarDelegateIOS
    : public ConfirmInfoBarDelegate {
 public:
  FormsAiPrivateInferenceInfoBarDelegateIOS(PrefService* prefs);
  ~FormsAiPrivateInferenceInfoBarDelegateIOS() override;

  // ConfirmInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  ui::ImageModel GetIcon() const override;
  std::u16string GetTitleText() const override;
  std::u16string GetMessageText() const override;
  int GetButtons() const override;
  std::u16string GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
  void InfoBarDismissed() override;

  // Called when the settings link is clicked.
  virtual void OnSettingsLinkClicked();

 private:
  raw_ptr<PrefService> prefs_ = nullptr;
  // Tracks whether an interaction was logged (Accept or Settings click),
  // preventing double logging of the "Dismissed" metric.
  bool interaction_logged_ = false;
};

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MODEL_FORMS_AI_PRIVATE_INFERENCE_INFOBAR_DELEGATE_IOS_H_
