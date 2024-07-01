// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Browser tests targeted at the `blink::WebView` that run in browser context.
// Note that these tests rely on single-process mode, and hence may be
// disabled in some configurations (check gyp files).

#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/shell/common/shell_content_client.h"
#include "content/shell/renderer/shell_content_renderer_client.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/disk_cache.h"
#include "net/http/http_cache.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/clear_data_filter.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/web/web_frame.h"

namespace content {

namespace {

class TestShellContentRendererClient : public ShellContentRendererClient {
 public:
  TestShellContentRendererClient()
      : latest_error_valid_(false),
        latest_error_reason_(0),
        latest_error_stale_copy_in_cache_(false) {}

  void PrepareErrorPage(
      content::RenderFrame* render_frame,
      const blink::WebURLError& error,
      const std::string& http_method,
      mojom::AlternativeErrorPageOverrideInfoPtr alternative_error_page_info,
      std::string* error_html) override {
    if (error_html)
      *error_html = "A suffusion of yellow.";
    latest_error_valid_ = true;
    latest_error_reason_ = error.reason();
    latest_error_stale_copy_in_cache_ = error.has_copy_in_cache();
  }

  bool GetLatestError(int* error_code, bool* stale_cache_entry_present) {
    if (latest_error_valid_) {
      *error_code = latest_error_reason_;
      *stale_cache_entry_present = latest_error_stale_copy_in_cache_;
    }
    return latest_error_valid_;
  }

 private:
  bool latest_error_valid_;
  int latest_error_reason_;
  bool latest_error_stale_copy_in_cache_;
};

}  // namespace

class RenderViewBrowserTest : public ContentBrowserTest {
 public:
  RenderViewBrowserTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // This method is needed to allow interaction with in-process renderer
    // and use of a test ContentRendererClient.
    command_line->AppendSwitch(switches::kSingleProcess);
  }

  void SetUpOnMainThread() override {
    // Override setting of renderer client.
    renderer_client_ = new TestShellContentRendererClient();
    // Explicitly leaks ownership; this object will remain alive
    // until process death.  We don't deleted the returned value,
    // since some contexts set the pointer to a non-heap address.
    SetRendererClientForTesting(renderer_client_);
  }

  // Navigates to the given URL and waits for |num_navigations| to occur, and
  // the title to change to |expected_title|.
  void NavigateToURLAndWaitForTitle(const GURL& url,
                                    const std::string& expected_title,
                                    int num_navigations) {
    content::TitleWatcher title_watcher(
        shell()->web_contents(), base::ASCIIToUTF16(expected_title));

    content::NavigateToURLBlockUntilNavigationsComplete(
        shell(), url, num_navigations);

    EXPECT_EQ(base::ASCIIToUTF16(expected_title),
              title_watcher.WaitAndGetTitle());
  }

  // Returns true if there is a valid error stored; in this case
  // |*error_code| and |*stale_cache_entry_present| will be updated
  // appropriately.
  // Must be called after the renderer thread is created.
  bool GetLatestErrorFromRendererClient(
      int* error_code, bool* stale_cache_entry_present) {
    bool result = false;

    PostTaskToInProcessRendererAndWait(base::BindOnce(
        &RenderViewBrowserTest::GetLatestErrorFromRendererClient0,
        renderer_client_, &result, error_code, stale_cache_entry_present));
    return result;
  }

 private:
  // Must be run on renderer thread.
  static void GetLatestErrorFromRendererClient0(
      TestShellContentRendererClient* renderer_client,
      bool* result, int* error_code, bool* stale_cache_entry_present) {
    *result = renderer_client->GetLatestError(
        error_code, stale_cache_entry_present);
  }

  raw_ptr<TestShellContentRendererClient> renderer_client_;
};

// https://crbug.com/788788
#if (BUILDFLAG(IS_ANDROID) && defined(ADDRESS_SANITIZER)) || \
    (BUILDFLAG(IS_LINUX) && defined(MEMORY_SANITIZER))
#define MAYBE_ConfirmCacheInformationPlumbed \
  DISABLED_ConfirmCacheInformationPlumbed
#else
#define MAYBE_ConfirmCacheInformationPlumbed ConfirmCacheInformationPlumbed
#endif  // BUILDFLAG(IS_ANDROID) && defined(ADDRESS_SANITIZER)
IN_PROC_BROWSER_TEST_F(RenderViewBrowserTest,
                       MAYBE_ConfirmCacheInformationPlumbed) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Load URL with "nocache" set, to create stale cache.
  GURL test_url(embedded_test_server()->GetURL("/nocache-with-etag.html"));
  NavigateToURLAndWaitForTitle(test_url, "Nocache Test Page", 1);

  // Shut down the server to force a network error.
  ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());

  // An error results in one completed navigation.
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 1);
  int error_code = net::OK;
  bool stale_cache_entry_present = false;
  ASSERT_TRUE(GetLatestErrorFromRendererClient(
      &error_code, &stale_cache_entry_present));
  EXPECT_EQ(net::ERR_CONNECTION_REFUSED, error_code);
  EXPECT_TRUE(stale_cache_entry_present);

  // Clear the cache and repeat; confirm lack of entry in cache reported.
  {
    base::RunLoop run_loop;
    content::StoragePartition* partition = shell()
                                               ->web_contents()
                                               ->GetPrimaryMainFrame()
                                               ->GetProcess()
                                               ->GetStoragePartition();
    partition->GetNetworkContext()->ClearHttpCache(
        base::Time(), base::Time(), nullptr /* filter */,
        base::BindOnce(run_loop.QuitClosure()));
    run_loop.Run();
  }

  content::NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 1);

  error_code = net::OK;
  stale_cache_entry_present = true;
  ASSERT_TRUE(GetLatestErrorFromRendererClient(
      &error_code, &stale_cache_entry_present));
  EXPECT_EQ(net::ERR_CONNECTION_REFUSED, error_code);
  EXPECT_FALSE(stale_cache_entry_present);
}

}  // namespace content
