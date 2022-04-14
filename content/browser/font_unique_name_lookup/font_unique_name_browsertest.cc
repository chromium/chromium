// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "content/browser/devtools/protocol/devtools_protocol_test_support.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"

#if BUILDFLAG(IS_WIN)
#include "base/files/scoped_temp_dir.h"
#include "content/browser/renderer_host/dwrite_font_lookup_table_builder_win.h"
#endif

namespace content {
namespace {

#if BUILDFLAG(IS_ANDROID)
const char* kExpectedFontFamilyNames[] = {"AndroidClock",
                                          "Roboto",
                                          "Droid Sans Mono",
                                          "Roboto",
                                          "Noto Color Emoji",
                                          "Noto Sans Bengali",
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
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
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
#elif BUILDFLAG(IS_MAC)
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
#elif BUILDFLAG(IS_WIN)
const char* kExpectedFontFamilyNames[] = {"Cambria Math", "MingLiU_HKSCS-ExtB",
                                          "NSimSun", "Calibri"};
#endif

}  // namespace

class FontUniqueNameBrowserTest : public DevToolsProtocolTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    DevToolsProtocolTest::SetUpCommandLine(command_line);
    feature_list_.InitAndEnableFeature(features::kFontSrcLocalMatching);
  }

  void LoadAndWait(const std::string& url) {
    base::ScopedAllowBlockingForTesting blocking_for_load;
    ASSERT_TRUE(embedded_test_server()->Start());
    TestNavigationObserver navigation_observer(
        static_cast<WebContentsImpl*>(shell()->web_contents()));
    EXPECT_TRUE(
        NavigateToURL(shell(), embedded_test_server()->GetURL("a.com", url)));
    ASSERT_TRUE(navigation_observer.last_navigation_succeeded());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
#if BUILDFLAG(IS_WIN)
  base::ScopedTempDir cache_directory_;
#endif
};

// TODO(crbug.com/949181): Make this work on Fuchsia.
#if !BUILDFLAG(IS_FUCHSIA)
// TODO(crbug.com/1270151): Fix this on Android 11 and 12.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_ContentLocalFontsMatching DISABLED_ContentLocalFontsMatching
#else
#define MAYBE_ContentLocalFontsMatching ContentLocalFontsMatching
#endif
IN_PROC_BROWSER_TEST_F(FontUniqueNameBrowserTest,
                       MAYBE_ContentLocalFontsMatching) {
  LoadAndWait("/font_src_local_matching.html");
  Attach();

  base::Value* dom_enable_result = SendCommand("DOM.enable", nullptr, true);
  ASSERT_TRUE(dom_enable_result);

  base::Value* css_enable_result = SendCommand("CSS.enable", nullptr, true);
  ASSERT_TRUE(css_enable_result);

  unsigned num_added_nodes = static_cast<unsigned>(
      content::EvalJs(shell(), "addTestNodes()").ExtractInt());
  ASSERT_EQ(num_added_nodes, std::size(kExpectedFontFamilyNames));

  std::unique_ptr<base::DictionaryValue> params =
      std::make_unique<base::DictionaryValue>();
  params->SetInteger("depth", 0);
  base::Value* result = SendCommand("DOM.getDocument", std::move(params));
  absl::optional<int> nodeId =
      result->GetDict().FindIntByDottedPath("root.nodeId");
  ASSERT_TRUE(nodeId);

  params = std::make_unique<base::DictionaryValue>();
  params->SetInteger("nodeId", *nodeId);
  params->SetString("selector", ".testnode");
  result = SendCommand("DOM.querySelectorAll", std::move(params));
  // This needs a Clone() because node_list otherwise gets invalid after the
  // next SendCommand call.
  const base::Value::List nodes_view =
      result->GetDict().FindList("nodeIds")->Clone();
  ASSERT_EQ(nodes_view.size(), num_added_nodes);
  ASSERT_EQ(nodes_view.size(), std::size(kExpectedFontFamilyNames));
  for (size_t i = 0; i < nodes_view.size(); ++i) {
    const base::Value& nodeId = nodes_view[i];
    params = std::make_unique<base::DictionaryValue>();
    params->SetInteger("nodeId", nodeId.GetInt());
    const base::Value* font_info =
        SendCommand("CSS.getPlatformFontsForNode", std::move(params));
    ASSERT_TRUE(font_info);
    ASSERT_TRUE(font_info->is_dict());
    const base::Value::List* font_list = font_info->GetDict().FindList("fonts");
    ASSERT_TRUE(font_list);
    ASSERT_TRUE(font_list->size());
    const base::Value& first_font_info = font_list->front();
    ASSERT_TRUE(first_font_info.is_dict());
    const std::string* first_font_name =
        first_font_info.GetDict().FindString("familyName");
    ASSERT_TRUE(first_font_name);
    ASSERT_GT(first_font_name->size(), 0u);
    ASSERT_EQ(*first_font_name, kExpectedFontFamilyNames[i]);
  }
}
#endif

}  // namespace content
