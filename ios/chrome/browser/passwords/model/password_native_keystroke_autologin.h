// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_PASSWORD_NATIVE_KEYSTROKE_AUTOLOGIN_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_PASSWORD_NATIVE_KEYSTROKE_AUTOLOGIN_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/common/unique_ids.h"

namespace web {
class WebState;
}  // namespace web

// Triggers the auto-submission flow.
void TriggerAutoSubmission(base::WeakPtr<web::WebState> weak_web_state,
                           std::string frame_id,
                           autofill::FieldRendererId field_id,
                           base::OnceClosure completion_handler);

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_PASSWORD_NATIVE_KEYSTROKE_AUTOLOGIN_H_
