// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/model/test_omnibox_view_ios.h"

#import <algorithm>

#import "base/strings/utf_string_conversions.h"
#import "components/omnibox/browser/test_omnibox_client.h"
#import "ui/gfx/native_widget_types.h"

// static
OmniboxViewIOS::State TestOmniboxViewIOS::CreateState(std::string text,
                                                      size_t sel_start,
                                                      size_t sel_end) {
  OmniboxViewIOS::State state;
  state.text = base::UTF8ToUTF16(text);
  state.sel_start = sel_start;
  state.sel_end = sel_end;
  return state;
}

std::u16string TestOmniboxViewIOS::GetText() const {
  return text_;
}

void TestOmniboxViewIOS::SetWindowTextAndCaretPos(const std::u16string& text,
                                                  size_t caret_pos,
                                                  bool update_popup,
                                                  bool notify_text_changed) {
  text_ = text;
  selection_ = gfx::Range(caret_pos);
}

void TestOmniboxViewIOS::GetSelectionBounds(size_t* start, size_t* end) const {
  *start = selection_.start();
  *end = selection_.end();
}

void TestOmniboxViewIOS::OnInlineAutocompleteTextMaybeChanged(
    const std::u16string& user_text,
    const std::u16string& inline_autocompletion) {
  std::u16string display_text = user_text + inline_autocompletion;
  const bool text_changed = text_ != display_text;
  text_ = display_text;
  inline_autocompletion_ = inline_autocompletion;

  // Just like the Views control, only change the selection if the text has
  // actually changed.
  if (text_changed) {
    selection_ = gfx::Range(text_.size(), inline_autocompletion.size());
  }
}

bool TestOmniboxViewIOS::OnAfterPossibleChange() {
  return false;
}
