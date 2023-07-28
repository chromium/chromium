// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_COLOR_CHOOSER_SHELL_COLOR_CHOOSER_IOS_H_
#define CONTENT_SHELL_BROWSER_COLOR_CHOOSER_SHELL_COLOR_CHOOSER_IOS_H_

#include "base/memory/raw_ptr.h"
#include "content/public/browser/color_chooser.h"
#include "third_party/blink/public/mojom/choosers/color_chooser.mojom-forward.h"

namespace content {

class WebContents;

class ShellColorChooserIOS : public ColorChooser {
 public:
  ShellColorChooserIOS(
      WebContents* web_contents,
      SkColor color,
      const std::vector<blink::mojom::ColorSuggestionPtr>& suggestions);
  ~ShellColorChooserIOS() override;

  ShellColorChooserIOS(const ShellColorChooserIOS&) = delete;
  ShellColorChooserIOS& operator=(const ShellColorChooserIOS&) = delete;

  // Show the color chooser dialog.
  static std::unique_ptr<ColorChooser> OpenColorChooser(
      WebContents* web_contents,
      SkColor color,
      const std::vector<blink::mojom::ColorSuggestionPtr>& suggestions);

  // ColorChooser:
  void End() override;
  void SetSelectedColor(SkColor color) override;

 private:
  raw_ptr<WebContents> web_contents_;
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_COLOR_CHOOSER_SHELL_COLOR_CHOOSER_IOS_H_
