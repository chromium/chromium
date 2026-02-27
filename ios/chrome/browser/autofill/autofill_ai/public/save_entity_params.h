// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_PUBLIC_SAVE_ENTITY_PARAMS_H_
#define IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_PUBLIC_SAVE_ENTITY_PARAMS_H_

#import <optional>

#import "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#import "components/autofill/core/browser/foundations/autofill_client.h"

namespace autofill {

// Parameters for the save entity bottom sheet command.
struct SaveEntityParams {
  SaveEntityParams(EntityInstance new_entity,
                   std::optional<EntityInstance> old_entity,
                   std::u16string user_email,
                   AutofillClient::EntityImportPromptResultCallback callback);

  SaveEntityParams(const SaveEntityParams&) = delete;
  SaveEntityParams& operator=(const SaveEntityParams&) = delete;

  SaveEntityParams(SaveEntityParams&&);
  SaveEntityParams& operator=(SaveEntityParams&&);
  ~SaveEntityParams();

  std::u16string GetTitleText() const;
  std::u16string GetMessageText() const;

  bool IsUpdate() const;

  EntityInstance new_entity;
  std::optional<EntityInstance> old_entity;
  std::u16string user_email;
  AutofillClient::EntityImportPromptResultCallback callback;
};

}  // namespace autofill

#endif  // IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_PUBLIC_SAVE_ENTITY_PARAMS_H_
