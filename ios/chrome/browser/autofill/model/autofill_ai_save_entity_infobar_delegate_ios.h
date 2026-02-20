// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MODEL_AUTOFILL_AI_SAVE_ENTITY_INFOBAR_DELEGATE_IOS_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MODEL_AUTOFILL_AI_SAVE_ENTITY_INFOBAR_DELEGATE_IOS_H_

#import "base/functional/callback_forward.h"
#import "base/functional/callback_helpers.h"
#import "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#import "components/autofill/core/browser/foundations/autofill_client.h"
#import "components/infobars/core/confirm_infobar_delegate.h"

namespace autofill {

class EntityInstance;

// Infobar delegate that prompts the user to save or update an Autofill AI
// entity.
class AutofillAiSaveEntityInfoBarDelegateIOS : public ConfirmInfoBarDelegate {
 public:
  AutofillAiSaveEntityInfoBarDelegateIOS(
      EntityInstance new_entity,
      std::optional<EntityInstance> old_entity,
      std::u16string user_email,
      base::OnceClosure on_accept_action,
      AutofillClient::EntityImportPromptResultCallback callback);

  AutofillAiSaveEntityInfoBarDelegateIOS(
      const AutofillAiSaveEntityInfoBarDelegateIOS&) = delete;
  AutofillAiSaveEntityInfoBarDelegateIOS& operator=(
      const AutofillAiSaveEntityInfoBarDelegateIOS&) = delete;

  ~AutofillAiSaveEntityInfoBarDelegateIOS() override;

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
  bool IsUpdate() const { return old_entity_.has_value(); }

  EntityInstance new_entity_;
  std::optional<EntityInstance> old_entity_;
  AutofillClient::EntityImportPromptResultCallback callback_;
  base::OnceClosure accept_callback_;
  std::u16string user_email_;
};

}  // namespace autofill

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MODEL_AUTOFILL_AI_SAVE_ENTITY_INFOBAR_DELEGATE_IOS_H_
