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

// Represents the final state of an auto-submission flow.
struct AutoSubmissionState {
  // True if the submission attempt was aborted mid-flow (e.g., `WebState` being
  // backgrounded or destroyed).
  bool aborted = false;
  // True if the native Enter keystroke (`insertText:@"\n"`) was dispatched to
  // the `WebState`'s first responder. Note that this tracks native keystroke
  // dispatch rather than DOM form submission acceptance.
  bool keystroke_dispatched = false;

  friend bool operator==(const AutoSubmissionState&,
                         const AutoSubmissionState&) = default;
};

using AutoSubmissionCallback = base::OnceCallback<void(AutoSubmissionState)>;

// Triggers the auto-submission flow.
void TriggerAutoSubmission(base::WeakPtr<web::WebState> weak_web_state,
                           const std::string& frame_id,
                           autofill::FieldRendererId field_id,
                           base::OnceClosure completion_handler);

// Triggers the auto-submission flow and invokes `completion_handler` with the
// final `AutoSubmissionState` of the flow. When `should_prevent_keyboard` is
// `false`, skips the JavaScript calls to set `inputmode='none'` for callers
// that already suppress the keyboard via other flows.
void TriggerAutoSubmissionWithResult(
    base::WeakPtr<web::WebState> weak_web_state,
    const std::string& frame_id,
    autofill::FieldRendererId field_id,
    AutoSubmissionCallback completion_handler,
    bool should_prevent_keyboard = true);

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_PASSWORD_NATIVE_KEYSTROKE_AUTOLOGIN_H_
