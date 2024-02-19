// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include "base/strings/utf_string_conversions.h"
#include "content/browser/devtools/protocol/devtools_protocol_test_support.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"

namespace content {

class FontPreferencesBrowserTest : public DevToolsProtocolTest {
 public:
  FontPreferencesBrowserTest() = default;
  ~FontPreferencesBrowserTest() override = default;

 protected:
  std::string GetFirstPlatformFontForBody() {
    base::Value::Dict params1;
    params1.Set("depth", 0);
    const base::Value::Dict* result =
        SendCommand("DOM.getDocument", std::move(params1));

    std::optional<int> body_node_id =
        result->FindIntByDottedPath("root.nodeId");
    DCHECK(body_node_id);

    base::Value::Dict params2;
    params2.Set("nodeId", *body_node_id);
    params2.Set("selector", "body");
    result = SendCommand("DOM.querySelector", std::move(params2));
    DCHECK(result);
    body_node_id = result->FindInt("nodeId");
    DCHECK(body_node_id);

    base::Value::Dict params3;
    params3.Set("nodeId", *body_node_id);
    const base::Value::Dict* font_info =
        SendCommand("CSS.getPlatformFontsForNode", std::move(params3));
    DCHECK(font_info);
    const base::Value::List* font_list = font_info->FindList("fonts");
    DCHECK(font_list);
    DCHECK(font_list->size() > 0);
    const base::Value& first_font_info = font_list->front();
    const std::string* first_font_name =
        first_font_info.GetDict().FindString("familyName");
    DCHECK(first_font_name);
    return *first_font_name;
  }

  // Verify that text rendered with CSS font-family set to generic_family uses
  // the corresponding value from WebPreferences:
  // - generic_family: A CSS font-family to test e.g. "serif".
  // - default_preferences: the default WebPreferences.
  // - default_preferences_font_family_map: the font family map corresponding to
  //   the generic_family e.g. default_preferences.serif_font_family_map.
  void TestGenericFamilyPreference(
      const std::string& generic_family,
      blink::web_pref::WebPreferences& default_preferences,
      blink::web_pref::ScriptFontFamilyMap&
          default_preferences_font_family_map) {
    // The test works by setting the tested preference to a system font
    // different from the default value, and verifying that this change is
    // taken into account for text rendering.
    const std::u16string default_system_font =
        default_preferences_font_family_map[blink::web_pref::kCommonScript];
#if BUILDFLAG(IS_WIN)
    const std::string non_default_system_font = "Lucida Console";
#elif BUILDFLAG(IS_MAC)
    const std::string non_default_system_font = "Monaco";
#elif BUILDFLAG(IS_IOS)
    const std::string non_default_system_font = "Verdana";
#elif BUILDFLAG(IS_FUCHSIA)
    // Fuchsia platforms don't seem to have many pre-installed fonts besides the
    // default Roboto families. Let's instead choose the default monospace
    // family or, if 'monospace' is tested, the default sans-serif family.
    const char* default_system_font_sans_serif = "Roboto";
    const char* default_system_font_monospace = "Roboto Mono";
    const std::string non_default_system_font =
        generic_family == "monospace" ? default_system_font_sans_serif
                                      : default_system_font_monospace;
#else
    const std::string non_default_system_font = "Ahem";
#endif

    // Set the font-family of the body to the specified generic family.
    WebContents* web_contents = shell()->web_contents();
    EXPECT_TRUE(ExecJs(web_contents, "document.body.style.fontFamily = '" +
                                         generic_family + "'"));

    // Verify that by default, the non-default system font above is not used.
    web_contents->SetWebPreferences(default_preferences);
    EXPECT_TRUE(ExecJs(web_contents, "document.body.offsetTop"));
    EXPECT_NE(GetFirstPlatformFontForBody(), non_default_system_font);

    // Set the preference to that non-default system font and try again.
    default_preferences_font_family_map[blink::web_pref::kCommonScript] =
        base::ASCIIToUTF16(non_default_system_font);
    web_contents->SetWebPreferences(default_preferences);
    EXPECT_TRUE(ExecJs(web_contents, "document.body.offsetTop"));
    EXPECT_EQ(GetFirstPlatformFontForBody(), non_default_system_font);

    // Restore the preference to its default value.
    default_preferences_font_family_map[blink::web_pref::kCommonScript] =
        default_system_font;
  }
};

IN_PROC_BROWSER_TEST_F(FontPreferencesBrowserTest, GenericFamilies) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_TRUE(NavigateToURL(shell(), GURL("data:text/html,BODY_TEXT")));
  Attach();

  ASSERT_TRUE(SendCommand("DOM.enable", base::Value::Dict(), true));
  ASSERT_TRUE(SendCommand("CSS.enable", base::Value::Dict(), true));

  blink::web_pref::WebPreferences default_preferences =
      shell()->web_contents()->GetOrCreateWebPreferences();
  TestGenericFamilyPreference("initial", default_preferences,
                              default_preferences.standard_font_family_map);
  TestGenericFamilyPreference("serif", default_preferences,
                              default_preferences.serif_font_family_map);
  TestGenericFamilyPreference("sans-serif", default_preferences,
                              default_preferences.sans_serif_font_family_map);
  TestGenericFamilyPreference("cursive", default_preferences,
                              default_preferences.cursive_font_family_map);
  TestGenericFamilyPreference("fantasy", default_preferences,
                              default_preferences.fantasy_font_family_map);
  TestGenericFamilyPreference("monospace", default_preferences,
                              default_preferences.fixed_font_family_map);
  TestGenericFamilyPreference("math", default_preferences,
                              default_preferences.math_font_family_map);
}

}  // namespace content
