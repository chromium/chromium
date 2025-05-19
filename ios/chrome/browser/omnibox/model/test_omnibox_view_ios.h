// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_MODEL_TEST_OMNIBOX_VIEW_IOS_H_
#define IOS_CHROME_BROWSER_OMNIBOX_MODEL_TEST_OMNIBOX_VIEW_IOS_H_

#import <stddef.h>

#import <string>

#import "ios/chrome/browser/omnibox/model/omnibox_view_ios.h"
#import "ui/gfx/range/range.h"

// Fake implementation of OmniboxViewIOS for use in tests.
class TestOmniboxViewIOS : public OmniboxViewIOS {
 public:
  explicit TestOmniboxViewIOS(std::unique_ptr<OmniboxClient> client)
      : OmniboxViewIOS(nil, std::move(client), nil, nil, nil) {}

  TestOmniboxViewIOS(const TestOmniboxViewIOS&) = delete;
  TestOmniboxViewIOS& operator=(const TestOmniboxViewIOS&) = delete;

  const std::u16string& inline_autocompletion() const {
    return inline_autocompletion_;
  }

  static State CreateState(std::string text, size_t sel_start, size_t sel_end);

  // OmniboxViewIOS:
  std::u16string GetText() const override;
  void SetWindowTextAndCaretPos(const std::u16string& text,
                                size_t caret_pos,
                                bool update_popup,
                                bool notify_text_changed) override;
  void SetCaretPos(size_t caret_pos) override {}
  void SetAdditionalText(const std::u16string& text) override {}
  void GetSelectionBounds(size_t* start, size_t* end) const override;
  void UpdatePopup() override {}
  void OnInlineAutocompleteTextMaybeChanged(
      const std::u16string& user_text,
      const std::u16string& inline_autocompletion) override;
  void OnBeforePossibleChange() override {}
  bool OnAfterPossibleChange() override;
  using OmniboxViewIOS::GetStateChanges;

 private:
  std::u16string text_;
  std::u16string inline_autocompletion_;
  gfx::Range selection_;
};

#endif  // IOS_CHROME_BROWSER_OMNIBOX_MODEL_TEST_OMNIBOX_VIEW_IOS_H_
