// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MODEL_AUTOFILL_AI_SAVE_ENTITY_INFOBAR_DELEGATE_IOS_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MODEL_AUTOFILL_AI_SAVE_ENTITY_INFOBAR_DELEGATE_IOS_H_

#import "base/functional/callback_forward.h"
#import "components/autofill/core/browser/foundations/autofill_client.h"
#import "components/infobars/core/confirm_infobar_delegate.h"
#import "ios/chrome/browser/autofill/autofill_ai/public/save_entity_params.h"

namespace autofill {

// Infobar delegate that prompts the user to save or update an Autofill AI
// entity.
class AutofillAiSaveEntityInfoBarDelegateIOS : public ConfirmInfoBarDelegate {
 public:
  AutofillAiSaveEntityInfoBarDelegateIOS(SaveEntityParams params,
                                         base::OnceClosure on_accept_action);

  AutofillAiSaveEntityInfoBarDelegateIOS(
      const AutofillAiSaveEntityInfoBarDelegateIOS&) = delete;
  AutofillAiSaveEntityInfoBarDelegateIOS& operator=(
      const AutofillAiSaveEntityInfoBarDelegateIOS&) = delete;

  ~AutofillAiSaveEntityInfoBarDelegateIOS() override;

  // Returns the parameters for the save entity infobar. This call takes the
  // ownership of the parameters from the infobar delegate. Without this call,
  // the deconstructor of the infobar delegate calls the callback in the
  // `SaveEntityParams` with `kUnknown`. With this call, the callback is not
  // called.
  SaveEntityParams ExtractParams() { return std::move(params_); }

  // ConfirmInfoBarDelegate implementation.
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  int GetIconId() const override;
  ui::ImageModel GetIcon() const override;
  std::u16string GetTitleText() const override;
  std::u16string GetMessageText() const override;
  std::u16string GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
  bool Cancel() override;
  void InfoBarDismissed() override;
  bool ShouldExpire(const NavigationDetails& details) const override;
  infobars::InfoBarDelegate::InfobarPriority GetPriority() const override;

 private:
  // Returns whether this is an update prompt.
  bool IsUpdate() const { return params_.old_entity.has_value(); }

  SaveEntityParams params_;
  base::OnceClosure accept_callback_;
};

}  // namespace autofill

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MODEL_AUTOFILL_AI_SAVE_ENTITY_INFOBAR_DELEGATE_IOS_H_
