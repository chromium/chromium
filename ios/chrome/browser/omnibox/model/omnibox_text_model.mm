// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/model/omnibox_text_model.h"

#import "base/metrics/user_metrics.h"

OmniboxTextModel::OmniboxTextModel()
    : focus_state(OMNIBOX_FOCUS_NONE),
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
}
