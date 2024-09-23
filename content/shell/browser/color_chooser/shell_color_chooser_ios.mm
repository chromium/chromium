// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/color_chooser/shell_color_chooser_ios.h"

namespace content {

ShellColorChooserIOS::ShellColorChooserIOS(
    content::WebContents* web_contents,
    SkColor initial_color,
    const std::vector<blink::mojom::ColorSuggestionPtr>& suggestions)
    : web_contents_(web_contents) {
  // TODO(crbug.com/40276929): Show a color chooser using UI components on iOS.
}

ShellColorChooserIOS::~ShellColorChooserIOS() = default;

// static
std::unique_ptr<ColorChooser> ShellColorChooserIOS::OpenColorChooser(
    WebContents* web_contents,
    SkColor color,
    const std::vector<blink::mojom::ColorSuggestionPtr>& suggestions) {
  return std::make_unique<ShellColorChooserIOS>(web_contents, color,
                                                suggestions);
}

void ShellColorChooserIOS::End() {
  // TODO(crbug.com/40276929): Close the color chooser.
}

void ShellColorChooserIOS::SetSelectedColor(SkColor color) {
}  // TODO(crbug.com/40276929): Set the color.

}  // namespace content
