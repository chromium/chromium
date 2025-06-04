// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/model/omnibox_text_model.h"

OmniboxTextModel::OmniboxTextModel()
    : focus_state(OMNIBOX_FOCUS_NONE),
      user_input_in_progress(false),
      user_text(u""),
      focus_resulted_in_navigation(false),
      just_deleted_text(false),
      inline_autocompletion(u""),
      paste_state(OmniboxPasteState::kNone) {}

OmniboxTextModel::~OmniboxTextModel() = default;
