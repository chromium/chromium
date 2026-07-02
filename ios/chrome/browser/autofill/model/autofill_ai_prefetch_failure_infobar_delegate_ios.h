// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MODEL_AUTOFILL_AI_PREFETCH_FAILURE_INFOBAR_DELEGATE_IOS_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MODEL_AUTOFILL_AI_PREFETCH_FAILURE_INFOBAR_DELEGATE_IOS_H_

#import "components/infobars/core/confirm_infobar_delegate.h"
#import "ui/base/models/image_model.h"

namespace autofill {

// Infobar delegate that prompts the user about Autofill AI prefetch failure.
class AutofillAiPrefetchFailureInfoBarDelegateIOS
    : public ConfirmInfoBarDelegate {
 public:
  AutofillAiPrefetchFailureInfoBarDelegateIOS();

  AutofillAiPrefetchFailureInfoBarDelegateIOS(
      const AutofillAiPrefetchFailureInfoBarDelegateIOS&) = delete;
  AutofillAiPrefetchFailureInfoBarDelegateIOS& operator=(
      const AutofillAiPrefetchFailureInfoBarDelegateIOS&) = delete;

  ~AutofillAiPrefetchFailureInfoBarDelegateIOS() override;

  // ConfirmInfoBarDelegate implementation.
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  ui::ImageModel GetIcon() const override;
  std::u16string GetMessageText() const override;
  int GetButtons() const override;
  std::u16string GetButtonLabel(InfoBarButton button) const override;
  bool IgnoreIconColorWithTint() const override;

 private:
  ui::ImageModel icon_;
};

}  // namespace autofill

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MODEL_AUTOFILL_AI_PREFETCH_FAILURE_INFOBAR_DELEGATE_IOS_H_
