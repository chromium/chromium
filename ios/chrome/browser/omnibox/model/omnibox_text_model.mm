// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/model/omnibox_text_model.h"

#import "base/metrics/user_metrics.h"
#import "base/trace_event/trace_event.h"
#import "ios/chrome/browser/omnibox/model/omnibox_text_controller.h"

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

bool OmniboxTextModel::UpdateStateAfterPossibleChange(
    const OmniboxStateChanges& state_changes) {
  // Update the paste state as appropriate: if we're just finishing a paste
  // that replaced all the text, preserve that information; otherwise, if we've
  // made some other edit, clear paste tracking.
  if (paste_state == OmniboxPasteState::kPasting) {
    paste_state = OmniboxPasteState::kPasted;

    GURL url = GURL(*(state_changes.new_text));
    if (url.is_valid() && omnibox_client) {
      omnibox_client->OnUserPastedInOmniboxResultingInValidURL();
    }
  } else if (state_changes.text_differs) {
    paste_state = OmniboxPasteState::kNone;
  }

  if (state_changes.text_differs || state_changes.selection_differs) {
    // Restore caret visibility whenever the user changes text or selection in
    // the omnibox.
    SetFocusState(OMNIBOX_FOCUS_VISIBLE, OMNIBOX_FOCUS_CHANGE_TYPING);
  }

  // If the user text does not need to be changed, return now, so we don't
  // change any other state, lest arrowing around the omnibox do something like
  // reset `just_deleted_text_`.  Note that modifying the selection accepts any
  // inline autocompletion, which results in a user text change.
  if (!state_changes.text_differs &&
      (!state_changes.selection_differs || inline_autocompletion.empty())) {
    return false;
  }

  UpdateUserText(*state_changes.new_text);
  just_deleted_text = state_changes.just_deleted_text;

  return true;
}

OmniboxStateChanges OmniboxTextModel::GetStateChanges(
    const OmniboxTextState& before,
    const OmniboxTextState& after) const {
  OmniboxStateChanges state_changes;
  state_changes.old_text = &before.text;
  state_changes.new_text = &after.text;
  state_changes.new_sel_start = after.sel_start;
  state_changes.new_sel_end = after.sel_end;
  const bool old_sel_empty = before.sel_start == before.sel_end;
  const bool new_sel_empty = after.sel_start == after.sel_end;
  const bool sel_same_ignoring_direction =
      std::min(before.sel_start, before.sel_end) ==
          std::min(after.sel_start, after.sel_end) &&
      std::max(before.sel_start, before.sel_end) ==
          std::max(after.sel_start, after.sel_end);
  state_changes.selection_differs =
      (!old_sel_empty || !new_sel_empty) && !sel_same_ignoring_direction;
  state_changes.text_differs = before.text != after.text;
  state_changes.just_deleted_text =
      before.text.length() > after.text.length() &&
      // Check that the cursor is at or before the start of the old selection.
      // This ensures that if the user selected text and typed, it's not
      // considered a deletion even if the new text is shorter.
      after.sel_start <= std::min(before.sel_start, before.sel_end);
  return state_changes;
}
