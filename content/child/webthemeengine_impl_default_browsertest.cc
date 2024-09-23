// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#include "content/public/test/browser_test_utils.h"
#endif

namespace content {

class WebThemeEngineImplDefaultBrowserTest : public ContentBrowserTest {
 public:
  WebThemeEngineImplDefaultBrowserTest() = default;
};

#if BUILDFLAG(IS_WIN)
IN_PROC_BROWSER_TEST_F(WebThemeEngineImplDefaultBrowserTest, GetSystemColor) {
  // The test non-deterministically fails on Windows-2008ServerR2 builders due
  // to a difference in the default theme. As a result, only run the test on
  // non-server versions.
  // TODO(crbug.com/40246975): Remove this, and the windows_version.h
  // include, if the failure turns out to be specific to Windows-2008ServerR2
  // and not any Windows server.
  if (base::win::OSInfo::GetInstance()->version_type() ==
      base::win::VersionType::SUITE_SERVER) {
    return;
  }
  GURL url(
      "data:text/html,"
      "<!doctype html><html>"
      "<body>"
      "<div id='activeBorder' style='color: ActiveBorder'>ActiveBorder</div>"
      "<div id='activeCaption' style='color: ActiveCaption'>ActiveCaption</div>"
      "<div id='activeText' style='color: ActiveText'>ActiveText</div>"
      "<div id='appWorkspace' style='color: AppWorkspace'>AppWorkspace</div>"
      "<div id='background' style='color: Background'>Background</div>"
      "<div id='buttonFace' style='color: ButtonFace'>ButtonFace</div>"
      "<div id='buttonHighlight' style='color: "
      "ButtonHighlight'>ButtonHighlight</div>"
      "<div id='buttonShadow' style='color: ButtonShadow'>ButtonShadow</div>"
      "<div id='buttonText' style='color: ButtonText'>ButtonText</div>"
      "<div id='canvas' style='color: Canvas'>Canvas</div>"
      "<div id='canvasText' style='color: CanvasText'>CanvasText</div>"
      "<div id='captionText' style='color: CaptionText'>CaptionText</div>"
      "<div id='field' style='color: Field'>Field</div>"
      "<div id='fieldText' style='color: FieldText'>FieldText</div>"
      "<div id='grayText' style='color: GrayText'>GrayText</div>"
      "<div id='highlight' style='color: Highlight'>Highlight</div>"
      "<div id='highlightText' style='color: HighlightText'>HighlightText</div>"
      "<div id='inactiveBorder' style='color: "
      "InactiveBorder'>InactiveBorder</div>"
      "<div id='inactiveCaption' style='color: "
      "InactiveCaption'>InactiveCaption</div>"
      "<div id='inactiveCaptionText' style='color: "
      "InactiveCaptionText'>InactiveCaptionText</div>"
      "<div id='infoBackground' style='color: "
      "InfoBackground'>InfoBackground</div>"
      "<div id='infoText' style='color: InfoText'>InfoText</div>"
      "<div id='linkText' style='color: LinkText'>LinkText</div>"
      "<div id='menu' style='color: Menu'>Menu</div>"
      "<div id='menuText' style='color: MenuText'>MenuText</div>"
      "<div id='scrollbar' style='color: Scrollbar'>Scrollbar</div>"
      "<div id='threeDDarkShadow' style='color: "
      "ThreeDDarkShadow'>ThreeDDarkShadow</div>"
      "<div id='threeDFace' style='color: ThreeDFace'>ThreeDFace</div>"
      "<div id='threeDHighlight' style='color: "
      "ThreeDHighlight'>ThreeDHighlight</div>"
      "<div id='threeDLightShadow' style='color: "
      "ThreeDLightShadow'>ThreeDLightShadow</div>"
      "<div id='threeDShadow' style='color: ThreeDShadow'>ThreeDShadow</div>"
      "<div id='visitedText' style='color: VisitedText'>VisitedText</div>"
      "<div id='window' style='color: Window'>Window</div>"
      "<div id='windowFrame' style='color: WindowFrame'>WindowFrame</div>"
      "<div id='windowText' style='color: WindowText'>WindowText</div>"
      "</body></html>");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  std::vector<std::string> ids = {"activeBorder",
                                  "activeCaption",
                                  "activeText",
                                  "appWorkspace",
                                  "background",
                                  "buttonFace",
                                  "buttonHighlight",
                                  "buttonShadow",
                                  "buttonText",
                                  "canvas",
                                  "canvasText",
                                  "captionText",
                                  "field",
                                  "fieldText",
                                  "grayText",
                                  "highlight",
                                  "highlightText",
                                  "inactiveBorder",
                                  "inactiveCaption",
                                  "inactiveCaptionText",
                                  "infoBackground",
                                  "infoText",
                                  "linkText",
                                  "menu",
                                  "menuText",
                                  "scrollbar",
                                  "threeDDarkShadow",
                                  "threeDFace",
                                  "threeDHighlight",
                                  "threeDLightShadow",
                                  "threeDShadow",
                                  "visitedText",
                                  "window",
                                  "windowFrame",
                                  "windowText"};
  const std::vector<std::string> expected_colors = {
      "rgb(0, 0, 0)",       "rgb(0, 0, 0)",       "rgb(0, 102, 204)",
      "rgb(255, 255, 255)", "rgb(255, 255, 255)", "rgb(240, 240, 240)",
      "rgb(240, 240, 240)", "rgb(240, 240, 240)", "rgb(0, 0, 0)",
      "rgb(255, 255, 255)", "rgb(0, 0, 0)",       "rgb(0, 0, 0)",
      "rgb(255, 255, 255)", "rgb(0, 0, 0)",       "rgb(109, 109, 109)",
      "rgb(0, 120, 215)",   "rgb(255, 255, 255)", "rgb(0, 0, 0)",
      "rgb(255, 255, 255)", "rgb(128, 128, 128)", "rgb(255, 255, 255)",
      "rgb(0, 0, 0)",       "rgb(0, 102, 204)",   "rgb(255, 255, 255)",
      "rgb(0, 0, 0)",       "rgb(255, 255, 255)", "rgb(0, 0, 0)",
      "rgb(240, 240, 240)", "rgb(0, 0, 0)",       "rgb(0, 0, 0)",
      "rgb(0, 0, 0)",       "rgb(0, 102, 204)",   "rgb(255, 255, 255)",
      "rgb(0, 0, 0)",       "rgb(0, 0, 0)"};

  ASSERT_EQ(ids.size(), expected_colors.size());

  for (size_t i = 0; i < ids.size(); i++) {
    EXPECT_EQ(expected_colors[i],
              EvalJs(shell(),
                     "window.getComputedStyle(document.getElementById('" +
                         ids[i] + "')).getPropertyValue('color').toString()"));
  }
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace content
