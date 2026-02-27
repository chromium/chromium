// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/autofill_ai/public/save_entity_params.h"

#import "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace autofill {

SaveEntityParams::SaveEntityParams(
    EntityInstance new_entity,
    std::optional<EntityInstance> old_entity,
    std::u16string user_email,
    AutofillClient::EntityImportPromptResultCallback callback)
    : new_entity(std::move(new_entity)),
      old_entity(std::move(old_entity)),
      user_email(std::move(user_email)),
      callback(std::move(callback)) {}

SaveEntityParams::SaveEntityParams(SaveEntityParams&&) = default;
SaveEntityParams& SaveEntityParams::operator=(SaveEntityParams&&) = default;
SaveEntityParams::~SaveEntityParams() = default;

std::u16string SaveEntityParams::GetTitleText() const {
  return l10n_util::GetStringFUTF16(IsUpdate()
                                        ? IDS_IOS_AUTOFILL_AI_UPDATE_PROMPT
                                        : IDS_IOS_AUTOFILL_AI_SAVE_PROMPT,
                                    new_entity.type().GetNameForI18n());
}

std::u16string SaveEntityParams::GetMessageText() const {
  if (new_entity.record_type() == EntityInstance::RecordType::kServerWallet) {
    return l10n_util::GetStringFUTF16(IDS_IOS_INFOBAR_MESSAGE_SAVE_TO_WALLET,
                                      user_email);
  } else {
    return l10n_util::GetStringUTF16(IDS_IOS_INFOBAR_MESSAGE_SAVE_TO_DEVICE);
  }
}

bool SaveEntityParams::IsUpdate() const {
  return old_entity.has_value();
}

}  // namespace autofill
