// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_MODEL_TEST_OMNIBOX_VIEW_BASE_H_
#define IOS_CHROME_BROWSER_OMNIBOX_MODEL_TEST_OMNIBOX_VIEW_BASE_H_

#import <stddef.h>

#import <string>

#import "ios/chrome/browser/omnibox/model/omnibox_view_base.h"
#import "ui/gfx/range/range.h"

// Fake implementation of OmniboxViewBase for use in tests.
class TestOmniboxViewBase : public OmniboxViewBase {
 public:
  explicit TestOmniboxViewBase(std::unique_ptr<OmniboxClient> client)
      : OmniboxViewBase(std::move(client)) {}

  TestOmniboxViewBase(const TestOmniboxViewBase&) = delete;
  TestOmniboxViewBase& operator=(const TestOmniboxViewBase&) = delete;

  const std::u16string& inline_autocompletion() const {
    return inline_autocompletion_;
  }

  static State CreateState(std::string text, size_t sel_start, size_t sel_end);

  // OmniboxViewBase:
  void Update() override {}
  std::u16string GetText() const override;
  void SetWindowTextAndCaretPos(const std::u16string& text,
                                size_t caret_pos,
                                bool update_popup,
                                bool notify_text_changed) override;
  void SetCaretPos(size_t caret_pos) override {}
  void SetAdditionalText(const std::u16string& text) override {}
  bool IsSelectAll() const override;
  void GetSelectionBounds(size_t* start, size_t* end) const override;
  void SelectAll(bool reversed) override;
  void UpdatePopup() override {}
  void SetFocus(bool is_user_initiated) override {}
  void OnInlineAutocompleteTextMaybeChanged(
      const std::u16string& user_text,
      const std::u16string& inline_autocompletion) override;
  void OnInlineAutocompleteTextCleared() override;
  void OnBeforePossibleChange() override {}
  bool OnAfterPossibleChange() override;
  gfx::NativeView GetNativeView() const override;
  gfx::NativeView GetRelativeWindowForPopup() const override;
  bool IsImeComposing() const override;
  int GetOmniboxTextLength() const override;
  void EmphasizeURLComponents() override {}
  void SetEmphasis(bool emphasize, const gfx::Range& range) override {}
  void UpdateSchemeStyle(const gfx::Range& range) override {}
  using OmniboxViewBase::GetStateChanges;

 private:
  std::u16string text_;
  std::u16string inline_autocompletion_;
  gfx::Range selection_;
};

#endif  // IOS_CHROME_BROWSER_OMNIBOX_MODEL_TEST_OMNIBOX_VIEW_BASE_H_
