// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/stl_util.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/browser/devtools/protocol/devtools_protocol_test_support.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"

namespace content {
namespace {

#if defined(OS_ANDROID)
const char* kExpectedFontFamilyNames[] = {"AndroidClock",
                                          "Roboto",
                                          "Droid Sans Mono",
                                          "Roboto",
                                          "Noto Color Emoji",
                                          "Noto Sans Bengali",
                                          "Noto Sans Bengali",
                                          "Noto Sans Bengali UI",
                                          "Noto Sans Bengali UI",
                                          "Noto Sans Devanagari",
                                          "Noto Sans Devanagari",
                                          "Noto Sans Devanagari UI",
                                          "Noto Sans Devanagari UI",
                                          "Noto Sans Kannada",
                                          "Noto Sans Kannada",
                                          "Noto Sans Kannada UI",
                                          "Noto Sans Kannada UI",
                                          "Noto Sans Lao",
                                          "Noto Sans Lao",
                                          "Noto Sans Lao UI",
                                          "Noto Sans Lao UI",
                                          "Noto Sans Malayalam",
                                          "Noto Sans Malayalam",
                                          "Noto Sans Malayalam UI",
                                          "Noto Sans Malayalam UI",
                                          "Noto Sans Tamil",
                                          "Noto Sans Tamil",
                                          "Noto Sans Tamil UI",
                                          "Noto Sans Tamil UI",
                                          "Noto Sans Telugu",
                                          "Noto Sans Telugu",
                                          "Noto Sans Telugu UI",
                                          "Noto Sans Telugu UI",
                                          "Noto Sans Thai",
                                          "Noto Sans Thai",
                                          "Noto Sans Thai UI",
                                          "Noto Sans Thai UI",
                                          "Roboto",
                                          "Roboto Condensed",
                                          "Roboto Condensed",
                                          "Roboto Condensed",
                                          "Roboto Condensed",
                                          "Roboto"};
#elif defined(OS_LINUX)
const char* kExpectedFontFamilyNames[] = {"Ahem",
                                          "Arimo",
                                          "Arimo",
                                          "Arimo",
                                          "Arimo",
                                          "Cousine",
                                          "Cousine",
                                          "Cousine",
                                          "Cousine",
                                          "DejaVu Sans",
                                          "DejaVu Sans",
                                          "Garuda",
                                          "Gelasio",
                                          "Gelasio",
                                          "Gelasio",
                                          "Gelasio",
                                          "Lohit Devanagari",
                                          "Lohit Gurmukhi",
                                          "Lohit Tamil",
                                          "Noto Sans Khmer",
                                          "Tinos",
                                          "Tinos",
                                          "Tinos",
                                          "Tinos",
                                          "Mukti Narrow",
                                          "Tinos"};
#elif defined(OS_MACOSX)
const char* kExpectedFontFamilyNames[] = {"American Typewriter",
                                          "Arial Narrow",
                                          "Baskerville",
                                          "Devanagari MT",
                                          "DIN Alternate",
                                          "Gill Sans",
                                          "Iowan Old Style",
                                          "Malayalam Sangam MN",
                                          "Hiragino Maru Gothic Pro",
                                          "Hiragino Kaku Gothic StdN"};
#endif

}  // namespace

class FontUniqueNameBrowserTest : public DevToolsProtocolTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    feature_list_.InitAndEnableFeature(features::kFontSrcLocalMatching);
  }

  void LoadAndWait(const std::string& url) {
    ASSERT_TRUE(embedded_test_server()->Start());
    TestNavigationObserver navigation_observer(
        static_cast<WebContentsImpl*>(shell()->web_contents()));
    NavigateToURL(shell(), embedded_test_server()->GetURL("a.com", url));
    ASSERT_TRUE(navigation_observer.last_navigation_succeeded());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// TODO(drott): Enable this on all platforms.
#if defined(OS_ANDROID) || defined(OS_LINUX) || defined(OS_MACOSX)
IN_PROC_BROWSER_TEST_F(FontUniqueNameBrowserTest, ContentLocalFontsMatching) {
  LoadAndWait("/font_src_local_matching.html");
  Attach();

  base::Value* dom_enable_result = SendCommand("DOM.enable", nullptr, true);
  ASSERT_TRUE(dom_enable_result);

  base::Value* css_enable_result = SendCommand("CSS.enable", nullptr, true);
  ASSERT_TRUE(css_enable_result);

  unsigned num_added_nodes = static_cast<unsigned>(
      content::EvalJs(shell(), "addTestNodes()").ExtractInt());
  ASSERT_EQ(num_added_nodes, base::size(kExpectedFontFamilyNames));

  std::unique_ptr<base::DictionaryValue> params =
      std::make_unique<base::DictionaryValue>();
  params->SetInteger("depth", 0);
  base::Value* result = SendCommand("DOM.getDocument", std::move(params));
  result = result->FindPath({"root", "nodeId"});
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_int());

  params.reset(new base::DictionaryValue());
  params->SetInteger("nodeId", result->GetInt());
  params->SetString("selector", ".testnode");
  result = SendCommand("DOM.querySelectorAll", std::move(params));
  // This needs a Clone() because node_list otherwise gets invalid after the
  // next SendCommand call.
  base::Value node_list =
      result->FindKeyOfType("nodeIds", base::Value::Type::LIST)->Clone();
  std::vector<base::Value>& nodes_vector = node_list.GetList();
  ASSERT_EQ(nodes_vector.size(), num_added_nodes);
  ASSERT_EQ(nodes_vector.size(), base::size(kExpectedFontFamilyNames));
  for (size_t i = 0; i < nodes_vector.size(); ++i) {
    base::Value& nodeId = nodes_vector[i];
    params.reset(new base::DictionaryValue());
    params->SetInteger("nodeId", nodeId.GetInt());
    base::Value* font_info =
        SendCommand("CSS.getPlatformFontsForNode", std::move(params));
    ASSERT_TRUE(font_info);
    ASSERT_TRUE(font_info->is_dict());
    base::Value* font_list = font_info->FindKey("fonts");
    ASSERT_TRUE(font_list);
    ASSERT_TRUE(font_list->is_list());
    std::vector<base::Value>& font_info_list = font_list->GetList();
    ASSERT_TRUE(font_info_list.size());
    base::Value& first_font_info = font_info_list[0];
    ASSERT_TRUE(first_font_info.is_dict());
    base::Value* first_font_name = first_font_info.FindKey("familyName");
    ASSERT_TRUE(first_font_name);
    ASSERT_TRUE(first_font_name->is_string());
    ASSERT_GT(first_font_name->GetString().size(), 0u);
    ASSERT_EQ(first_font_name->GetString(), kExpectedFontFamilyNames[i]);
  }
}
#endif

}  // namespace content
