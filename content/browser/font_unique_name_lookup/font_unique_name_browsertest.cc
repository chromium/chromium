// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <memory>

#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "content/browser/devtools/protocol/devtools_protocol_test_support.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"

#if BUILDFLAG(IS_WIN)
#include "base/files/scoped_temp_dir.h"
#endif

namespace content {
namespace {

#if BUILDFLAG(IS_ANDROID)
const auto kExpectedFontFamilyNames = std::to_array({
    "AndroidClock",
    "Droid Sans Mono",
    "Roboto",
    "Noto Color Emoji",
    "Noto Sans Lao UI",
    "Noto Sans Lao UI",
    "Noto Sans Thai",
    "Noto Sans Thai",
    "Noto Sans Thai UI",
    "Noto Sans Thai UI",
});
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
const auto kExpectedFontFamilyNames = std::to_array({
    "Ahem",
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
    "Tinos",
});
#elif BUILDFLAG(IS_APPLE)
const auto kExpectedFontFamilyNames = std::to_array({
    "American Typewriter",
    "Arial Narrow",
    "Baskerville",
    "Devanagari MT",
    "DIN Alternate",
    "Gill Sans",
    "Iowan Old Style",
    "Malayalam Sangam MN",
    "Hiragino Maru Gothic Pro",
    "Hiragino Kaku Gothic StdN",
});
#elif BUILDFLAG(IS_WIN)
const auto kExpectedFontFamilyNames = std::to_array({
    "Cambria Math",
    "MingLiU_HKSCS-ExtB",
    "NSimSun",
    "Calibri",
});
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

// TODO(crbug.com/42050634): Make this work on Fuchsia.
#if !BUILDFLAG(IS_FUCHSIA)
IN_PROC_BROWSER_TEST_F(FontUniqueNameBrowserTest,
                       ContentLocalFontsMatching) {
  LoadAndWait("/font_src_local_matching.html");
  Attach();

  ASSERT_TRUE(SendCommandSync("DOM.enable"));
  ASSERT_TRUE(SendCommandSync("CSS.enable"));

  size_t num_added_nodes =
      static_cast<size_t>(EvalJs(shell(), "addTestNodes()").ExtractInt());
  ASSERT_EQ(num_added_nodes, std::size(kExpectedFontFamilyNames));

  base::Value::Dict get_doc_params;
  get_doc_params.Set("depth", 0);
  const base::Value::Dict* result =
      SendCommand("DOM.getDocument", std::move(get_doc_params));
  int node_id = *result->FindIntByDottedPath("root.nodeId");

  base::Value::Dict query_params;
  query_params.Set("nodeId", node_id);
  query_params.Set("selector", ".testnode");
  result = SendCommand("DOM.querySelectorAll", std::move(query_params));
  // This needs a Clone() because the node list otherwise gets invalid after the
  // next SendCommand call.
  const base::Value::List nodes = result->FindList("nodeIds")->Clone();
  ASSERT_EQ(nodes.size(), num_added_nodes);
  ASSERT_EQ(nodes.size(), std::size(kExpectedFontFamilyNames));
  for (size_t i = 0; i < nodes.size(); ++i) {
    const base::Value& node = nodes[i];
    base::Value::Dict get_fonts_params;
    get_fonts_params.Set("nodeId", node.GetInt());
    const base::Value::Dict* font_info =
        SendCommand("CSS.getPlatformFontsForNode", std::move(get_fonts_params));
    ASSERT_TRUE(font_info);
    const base::Value::List* font_list = font_info->FindList("fonts");
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
