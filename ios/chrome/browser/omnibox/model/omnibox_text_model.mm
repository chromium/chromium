// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/model/omnibox_text_model.h"

#import "base/metrics/user_metrics.h"
#import "base/trace_event/trace_event.h"

OmniboxTextModel::OmniboxTextModel(OmniboxClient* client)
    : omnibox_client(client),
      focus_state(OMNIBOX_FOCUS_NONE),
      user_input_in_progress(false),
      user_text(u""),
      focus_resulted_in_navigation(false),
      just_deleted_text(false),
      inline_autocompletion(u""),
      paste_state(OmniboxPasteState::kNone) {}

OmniboxTextModel::~OmniboxTextModel() = default;

bool OmniboxTextModel::SetInputInProgressNoNotify(bool in_progress) {
  if (user_input_in_progress == in_progress) {
    return false;
  }

  user_input_in_progress = in_progress;
  if (user_input_in_progress) {
    time_user_first_modified_omnibox = base::TimeTicks::Now();
    base::RecordAction(base::UserMetricsAction("OmniboxInputInProgress"));
  }
  return true;
}

bool OmniboxTextModel::HasFocus() {
  return focus_state != OMNIBOX_FOCUS_NONE;
}

void OmniboxTextModel::KillFocus() {
  focus_state = OMNIBOX_FOCUS_NONE;
  last_omnibox_focus = base::TimeTicks();
  paste_state = OmniboxPasteState::kNone;
  if (omnibox_client) {
    omnibox_client->OnFocusChanged(focus_state, OMNIBOX_FOCUS_CHANGE_EXPLICIT);
  }
}

void OmniboxTextModel::OnSetFocus() {
  TRACE_EVENT0("omnibox", "OmniboxTextModel::OnSetFocus");
  last_omnibox_focus = base::TimeTicks::Now();
  focus_resulted_in_navigation = false;

  // If the omnibox lost focus while the caret was hidden and then regained
  // focus, OnSetFocus() is called and should restore visibility. Note that
  // focus can be regained without an accompanying call to
  // OmniboxViewIOS::SetFocus(), e.g. by tabbing in.
  SetFocusState(OMNIBOX_FOCUS_VISIBLE, OMNIBOX_FOCUS_CHANGE_EXPLICIT);

  if (user_input_in_progress || !in_revert) {
    omnibox_client->OnInputStateChanged();
  }
}

void OmniboxTextModel::SetFocusState(OmniboxFocusState state,
                                     OmniboxFocusChangeReason reason) {
  if (state == focus_state) {
    return;
  }

  focus_state = state;
  omnibox_client->OnFocusChanged(focus_state, reason);
}

void OmniboxTextModel::UpdateUserText(const std::u16string& text) {
  user_text = text;
  just_deleted_text = false;
  inline_autocompletion.clear();
}
