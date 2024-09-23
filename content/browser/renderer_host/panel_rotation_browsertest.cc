// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/chromeos_buildflags.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/shell/browser/shell.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace content {

// TODO(crbug.com/41478398): Add test coverage across all platforms.
#if BUILDFLAG(IS_CHROMEOS_ASH)
class PanelRotationBrowserTest : public ContentBrowserTest {
 protected:
  void SetPanelRotation(display::Display::Rotation rotation) {
    display::Screen* screen = display::Screen::GetScreen();
    screen->SetPanelRotationForTesting(screen->GetPrimaryDisplay().id(),
                                       rotation);
  }
  int ReadScreenOrientationAngle() {
    return EvalJs(CreateBrowser()->web_contents(), "screen.orientation.angle")
        .ExtractInt();
  }
};

IN_PROC_BROWSER_TEST_F(PanelRotationBrowserTest, ScreenOrientationAPI) {
  SetPanelRotation(display::Display::ROTATE_0);
  EXPECT_EQ(ReadScreenOrientationAngle(), 0);

  SetPanelRotation(display::Display::ROTATE_90);
  EXPECT_EQ(ReadScreenOrientationAngle(), 270);

  SetPanelRotation(display::Display::ROTATE_180);
  EXPECT_EQ(ReadScreenOrientationAngle(), 180);

  SetPanelRotation(display::Display::ROTATE_270);
  EXPECT_EQ(ReadScreenOrientationAngle(), 90);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace content
