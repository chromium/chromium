// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "build/build_config.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/base/filename_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/frame/frame.mojom.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_savable_resources_test_support.h"

namespace content {

class SavableResourcesTest : public ContentBrowserTest {
 public:
  using UrlVectorMatcher = testing::Matcher<std::vector<GURL>>;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kSingleProcess);
  }

  // Test function GetAllSavableResourceLinksForCurrentPage with a web page.
  // We expect result of GetAllSavableResourceLinksForCurrentPage exactly
  // matches expected_resources_set.
  void GetSavableResourceLinksForPage(
      const base::FilePath& page_file_path,
      const UrlVectorMatcher& expected_resources_matcher,
      const UrlVectorMatcher& expected_subframe_urls_matcher) {
    // Convert local file path to file URL.
    GURL file_url = net::FilePathToFileURL(page_file_path);
    // Load the test file.
    EXPECT_TRUE(NavigateToURL(shell(), file_url));

    PostTaskToInProcessRendererAndWait(base::BindOnce(
        &SavableResourcesTest::CheckResources, base::Unretained(this),
        page_file_path, expected_resources_matcher,
        expected_subframe_urls_matcher, file_url,
        shell()->web_contents()->GetPrimaryMainFrame()->GetFrameToken()));
  }

  void CheckResources(const base::FilePath& page_file_path,
                      const UrlVectorMatcher& expected_resources_matcher,
                      const UrlVectorMatcher& expected_subframe_urls_matcher,
                      const GURL& file_url,
                      const blink::LocalFrameToken& frame_token) {
    RenderFrame* render_frame = RenderFrame::FromWebFrame(
        blink::WebLocalFrame::FromFrameToken(frame_token));

    mojo::AssociatedRemote<blink::mojom::LocalFrame> local_frame;
    render_frame->GetRemoteAssociatedInterfaces()->GetInterface(
        local_frame.BindNewEndpointAndPassReceiver());
    local_frame->GetSavableResourceLinks(
        base::BindOnce(&SavableResourcesTest::GetSavableResourceLinksCallback,
                       base::Unretained(this), expected_resources_matcher,
                       expected_subframe_urls_matcher));
  }

  void GetSavableResourceLinksCallback(
      const UrlVectorMatcher& expected_resources_matcher,
      const UrlVectorMatcher& expected_subframe_urls_matcher,
      blink::mojom::GetSavableResourceLinksReplyPtr reply) {
    if (!reply) {
      DCHECK(false)
          << "blink::mojom::GetSavableResourceLinks returnes nullptr.";
      return;
    }

    std::vector<GURL> resources_list;
    for (auto url : reply->resources_list)
      resources_list.push_back(url);
    EXPECT_THAT(resources_list, expected_resources_matcher);

    std::vector<GURL> subframe_original_urls;
    for (auto& subframe : reply->subframes) {
      subframe_original_urls.push_back(subframe->original_url);
    }
    EXPECT_THAT(subframe_original_urls, expected_subframe_urls_matcher);
  }
};

// Flaky on Linux MSan. See crbug.com/1423060.
#if BUILDFLAG(IS_LINUX) && defined(MEMORY_SANITIZER)
#define MAYBE_GetSavableResourceLinksWithPageHasValidStyleLink \
  DISABLED_GetSavableResourceLinksWithPageHasValidStyleLink
#else
#define MAYBE_GetSavableResourceLinksWithPageHasValidStyleLink \
  GetSavableResourceLinksWithPageHasValidStyleLink
#endif
IN_PROC_BROWSER_TEST_F(SavableResourcesTest,
                       MAYBE_GetSavableResourceLinksWithPageHasValidStyleLink) {
  base::FilePath page_file_path =
      GetTestFilePath("dom_serializer", "simple_linked_stylesheet.html");

  auto expected_subresources_matcher = testing::UnorderedElementsAre(
      net::FilePathToFileURL(GetTestFilePath("dom_serializer", "style.css")));

  auto expected_subframe_urls_matcher = testing::IsEmpty();

  GetSavableResourceLinksForPage(page_file_path, expected_subresources_matcher,
                                 expected_subframe_urls_matcher);
}

// Test function GetAllSavableResourceLinksForCurrentPage with a web page
// which has valid savable resource links.
// Flaky on Linux MSan. See crbug.com/1423060.
#if BUILDFLAG(IS_LINUX) && defined(MEMORY_SANITIZER)
#define MAYBE_GetSavableResourceLinksWithPageHasValidLinks \
  DISABLED_GetSavableResourceLinksWithPageHasValidLinks
#else
#define MAYBE_GetSavableResourceLinksWithPageHasValidLinks \
  GetSavableResourceLinksWithPageHasValidLinks
#endif
IN_PROC_BROWSER_TEST_F(SavableResourcesTest,
                       MAYBE_GetSavableResourceLinksWithPageHasValidLinks) {
  base::FilePath page_file_path =
      GetTestFilePath("dom_serializer", "youtube_1.htm");

  auto expected_subresources_matcher = testing::UnorderedElementsAre(
      GURL("file:///c:/yt/css/base_all-vfl36460.css"),
      GURL("file:///c:/yt/js/base_all_with_bidi-vfl36451.js"),
      GURL("file:///c:/yt/img/pixel-vfl73.gif"));

  auto expected_subframe_urls_matcher =
      testing::UnorderedElementsAre(net::FilePathToFileURL(
          GetTestFilePath("dom_serializer", "youtube_2.htm")));

  GetSavableResourceLinksForPage(page_file_path, expected_subresources_matcher,
                                 expected_subframe_urls_matcher);
}

// Test function GetAllSavableResourceLinksForCurrentPage with a web page
// which does not have valid savable resource links.
// Flaky on Linux MSan and Windows ASan. See crbug.com/1423060.
// Flaky in general. See crbug.com/361938524.
IN_PROC_BROWSER_TEST_F(
    SavableResourcesTest,
    DISABLED_GetSavableResourceLinksWithPageHasInvalidLinks) {
  base::FilePath page_file_path =
      GetTestFilePath("dom_serializer", "youtube_2.htm");

  auto expected_subresources_matcher = testing::IsEmpty();

  auto expected_subframe_urls_matcher = testing::IsEmpty();

  GetSavableResourceLinksForPage(page_file_path, expected_subresources_matcher,
                                 expected_subframe_urls_matcher);
}

}  // namespace content
