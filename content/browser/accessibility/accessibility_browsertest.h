// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>

#include "content/public/test/content_browser_test.h"
#include "content/public/test/scoped_accessibility_mode_override.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/gfx/native_widget_types.h"

#ifndef CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_BROWSERTEST_H_
#define CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_BROWSERTEST_H_

namespace content {

// A test fixture that enables web accessibility for the duration of a
// test. Individual tests may override this by setting the initial accessibility
// mode before performing any work.
class AccessibilityBrowserTest : public ContentBrowserTest {
 protected:
  AccessibilityBrowserTest();
  ~AccessibilityBrowserTest() override;

  // Sets the initial accessibility mode for the test. Must be called before the
  // first load is performed.
  void SetInitialAccessibilityMode(ui::AXMode accessibility_mode);

  // ContentBrowserTest:
  void TearDownOnMainThread() override;

  gfx::NativeViewAccessible GetRendererAccessible();
  void ExecuteScript(const std::u16string& script);
  void LoadInitialAccessibilityTreeFromHtml(const std::string& html);

  void LoadInputField();
  void LoadTextareaField();
  void LoadScrollableInputField(std::string type);
  void LoadSampleParagraph();
  void LoadSampleParagraphInScrollableEditable();
  void LoadSampleParagraphInScrollableDocument();

  static std::string InputContentsString();
  static std::string TextAreaContentsString();

 private:
  std::optional<ScopedAccessibilityModeOverride> accessibility_mode_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_BROWSERTEST_H_
