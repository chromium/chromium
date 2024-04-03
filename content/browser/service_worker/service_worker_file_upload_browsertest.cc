// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/values_test_util.h"
#include "build/build_config.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/base/url_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/ssl/ssl_server_config.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

namespace content {

namespace {

void GetKey(const base::Value::Dict& dict,
            const std::string& key,
            std::string* out_value) {
  const std::string* value = dict.FindString(key);
  ASSERT_TRUE(value);
  *out_value = *value;
}

void GetKey(const base::Value::Dict& dict,
            const std::string& key,
            int* out_value) {
  std::optional<int> value = dict.FindInt(key);
  ASSERT_TRUE(value);
  *out_value = *value;
}

// Helper since the default output of EXPECT_EQ isn't useful when debugging
// failures, it doesn't recurse into the dictionary.
void ExpectEqual(const base::Value::Dict& expected,
                 const base::Value::Dict& actual) {
  EXPECT_EQ(expected, actual)
      << "\nExpected: " << expected << "\nActual: " << actual;
}

const char kFileContent[] = "uploaded file content";
const size_t kFileSize = std::size(kFileContent) - 1;
}  // namespace

// Tests POST requests that include a file and are intercepted by a service
// worker. This is a browser test rather than a web test because as
// https://crbug.com/786510 describes, http tests involving file uploads usually
// need to be in the http/tests/local directory, which runs tests from file:
// URLs while serving http resources from the http server, but this trick can
// break the test when Site Isolation is enabled and content from different
// origins end up in different processes.
class ServiceWorkerFileUploadTest : public testing::WithParamInterface<bool>,
                                    public ContentBrowserTest {
 public:
  ServiceWorkerFileUploadTest() = default;

  ServiceWorkerFileUploadTest(const ServiceWorkerFileUploadTest&) = delete;
  ServiceWorkerFileUploadTest& operator=(const ServiceWorkerFileUploadTest&) =
      delete;

  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());

    ContentBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    // Make all hosts resolve to 127.0.0.1 so the same embedded test server can
    // be used for cross-origin URLs.
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->StartAcceptingConnections();
    StoragePartition* partition = shell()
                                      ->web_contents()
                                      ->GetBrowserContext()
                                      ->GetDefaultStoragePartition();
    wrapper_ = static_cast<ServiceWorkerContextWrapper*>(
        partition->GetServiceWorkerContext());
  }

  ServiceWorkerContextWrapper* wrapper() { return wrapper_.get(); }

  enum class TargetOrigin { kSameOrigin, kCrossOrigin };

  // Builds a target URL for the form to submit to. The URL has path
  // "|path|?|query|" so they can be adjusted to tell the service worker how to
  // handle the request.
  GURL BuildTargetUrl(const std::string& path, const std::string& query) {
    return embedded_test_server()->GetURL(path + "?" + query);
  }

  // Tests submitting a form that is intercepted by a service worker. The form
  // has several input elements including a file; this test creates a temp file
  // and uploads it. The service worker is expected to respond with JSON
  // describing the request data it saw.
  // - |target_url|: where to submit the form to.
  // - |target_origin|: whether to submit the form from a page that is
  //   cross-origin to the target.
  // - |out_file_name|: the name of the file this test uploaded via the form.
  // - |out_result|: the body of the resulting document.
  void RunTest(const GURL& target_url,
               TargetOrigin target_origin,
               std::string* out_file_name,
               std::string* out_result) {
    // Install the service worker. Use root scope since the network fallback
    // test needs it: the service worker will intercept "/echo", then fall back
    // to network, and the request gets handled by the default request handler
    // for that URL which echoes back the request.
    EXPECT_TRUE(NavigateToURL(
        shell(), embedded_test_server()->GetURL(
                     "/service_worker/create_service_worker.html")));
    EXPECT_EQ("DONE",
              EvalJs(shell(), "register('file_upload_worker.js', '/');"));

    // Generate the URL for the page with the file upload form.
    GURL page_url = embedded_test_server()->GetURL("/service_worker/form.html");
    // If |target_origin| says to test submitting to a cross-origin target, set
    // this page to a different origin. The |target_url| is expected to point
    // back to the original origin with the service worker.
    if (target_origin == TargetOrigin::kCrossOrigin) {
      GURL::Replacements replacements;
      replacements.SetHostStr("cross-origin.example.com");
      page_url = page_url.ReplaceComponents(replacements);
    }
    // Set the target to |target_url|.
    page_url = net::AppendQueryParameter(page_url, "target", target_url.spec());

    // Navigate to the page with a file upload form.
    EXPECT_TRUE(NavigateToURL(shell(), page_url));

    // Prepare a file for the upload form.
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::ScopedTempDir temp_dir;
    base::FilePath file_path;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
    ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_dir.GetPath(), &file_path));
    ASSERT_TRUE(
        base::WriteFile(file_path, std::string_view(kFileContent, kFileSize)));

    // Fill out the form to refer to the test file.
    base::RunLoop run_loop;
    auto delegate = std::make_unique<FileChooserDelegate>(
        file_path, run_loop.QuitClosure());
    shell()->web_contents()->SetDelegate(delegate.get());
    EXPECT_TRUE(ExecJs(shell(), "fileInput.click();"));
    run_loop.Run();

    // Submit the form.
    TestNavigationObserver form_post_observer(shell()->web_contents(), 1);
    EXPECT_TRUE(ExecJs(shell(), "form.submit();"));
    form_post_observer.Wait();

    // Extract the body payload.
    EvalJsResult result = EvalJs(shell()->web_contents()->GetPrimaryMainFrame(),
                                 "document.body.textContent");
    ASSERT_TRUE(result.error.empty());

    *out_file_name = file_path.BaseName().MaybeAsASCII();
    *out_result = result.ExtractString();
  }

  // Helper for tests where the service worker calls respondWith().
  void RunRespondWithTest(const std::string& target_query,
                          TargetOrigin target_origin,
                          std::string* out_filename,
                          base::Value::Dict& out_result) {
    std::string result;
    RunTest(BuildTargetUrl("/service_worker/upload", target_query),
            TargetOrigin::kSameOrigin, out_filename, &result);
    std::optional<base::Value> parsed_result = base::test::ParseJson(result);
    ASSERT_TRUE(parsed_result);
    ASSERT_TRUE(parsed_result->is_dict());
    out_result = std::move(*parsed_result).TakeDict();
  }

  // Helper for tests where the service worker falls back to network.
  void RunNetworkFallbackTest(TargetOrigin target_origin) {
    std::string filename;
    std::string result;
    // Use "/echo" so the request hits the echo default handler after falling
    // back to network.
    // Use "getAs=fallback" to tell the service worker to fall back.
    RunTest(BuildTargetUrl("/echo", "getAs=fallback"), target_origin, &filename,
            &result);

    // This isn't as rigorous as BuildExpectedBodyAsFormData(). The test author
    // couldn't get that to work maybe because \r\n get stripped somewhere.
    EXPECT_THAT(result, ::testing::HasSubstr(kFileContent));
    EXPECT_THAT(result, ::testing::HasSubstr(filename));
    EXPECT_THAT(result, ::testing::HasSubstr("form-data; name=\"file\""));
  }

  WebContentsImpl* web_contents() {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  RenderFrameHostImpl* current_frame_host() {
    return web_contents()->GetPrimaryFrameTree().root()->current_frame_host();
  }

  int GetServiceWorkerProcessId() {
    const base::flat_map<int64_t, ServiceWorkerRunningInfo>& infos =
        wrapper()->GetRunningServiceWorkerInfos();
    EXPECT_EQ(1u, infos.size());
    const ServiceWorkerRunningInfo& info = infos.begin()->second;
    return info.render_process_id;
  }

  bool IsDifferentProcessForced() { return GetParam(); }

  void RunSubresourceTest(const base::FilePath& file_path,
                          std::string* out_result) {
    // Install the service worker.
    EXPECT_TRUE(NavigateToURL(
        shell(), embedded_test_server()->GetURL(
                     "/service_worker/create_service_worker.html")));
    EXPECT_EQ("DONE", EvalJs(shell(), "register('file_upload_worker.js');"));

    if (IsDifferentProcessForced()) {
      // Stop the service worker.
      base::RunLoop run_loop;
      wrapper()->StopAllServiceWorkers(run_loop.QuitClosure());
      run_loop.Run();

      // Ensure that service worker will re-boot in a different process from the
      // page.
      ServiceWorkerProcessManager* process_manager =
          wrapper()->process_manager();
      process_manager->ForceNewProcessForTest(true);
    }

    // Generate the URL for the page with the file upload form. It will upload
    // to |target_url|.
    GURL page_url = embedded_test_server()->GetURL("/service_worker/form.html");
    GURL target_url = BuildTargetUrl("/service_worker/upload", "getAs=text");
    page_url = net::AppendQueryParameter(page_url, "target", target_url.spec());

    // Navigate to the page with a file upload form.
    EXPECT_TRUE(NavigateToURL(shell(), page_url));

    if (IsDifferentProcessForced()) {
      int page_process_id = current_frame_host()->GetProcess()->GetID();
      int worker_process_id = GetServiceWorkerProcessId();
      ASSERT_NE(page_process_id, worker_process_id);
    }

    // Fill out the form to refer to the test file.
    base::RunLoop run_loop;
    auto delegate = std::make_unique<FileChooserDelegate>(
        file_path, run_loop.QuitClosure());
    shell()->web_contents()->SetDelegate(delegate.get());
    EXPECT_TRUE(ExecJs(shell(), "fileInput.click();"));
    run_loop.Run();

    // Submit the form using XHR.
    EvalJsResult result = EvalJs(shell(), "submitXhr()");
    ASSERT_TRUE(result.error.empty());
    *out_result = result.ExtractString();
  }

  std::string BuildExpectedBodyAsText(const std::string& boundary,
                                      const std::string& filename) {
    return "--" + boundary + "\r\n" +
           "Content-Disposition: form-data; name=\"text1\"\r\n" + "\r\n" +
           "textValue1\r\n" + "--" + boundary + "\r\n" +
           "Content-Disposition: form-data; name=\"text2\"\r\n" + "\r\n" +
           "textValue2\r\n" + "--" + boundary + "\r\n" +
           "Content-Disposition: form-data; name=\"file\"; "
           "filename=\"" +
           filename + "\"\r\n" + "Content-Type: application/octet-stream\r\n" +
           "\r\n" + kFileContent + "\r\n" + "--" + boundary + "--\r\n";
  }

  base::Value::Dict BuildExpectedBodyAsFormData(const std::string& filename) {
    std::string expectation = R"({
      "entries": [
        {
          "key": "text1",
          "value": {
            "type": "string",
            "data": "textValue1"
          }
        },
        {
          "key": "text2",
          "value": {
            "type": "string",
            "data": "textValue2"
          }
        },
        {
          "key": "file",
          "value": {
            "type": "file",
            "name": "@PATH@",
            "size": @SIZE@
          }
        }
      ]
    })";
    base::ReplaceFirstSubstringAfterOffset(&expectation, 0, "@PATH@", filename);
    base::ReplaceFirstSubstringAfterOffset(&expectation, 0, "@SIZE@",
                                           base::NumberToString(kFileSize));
    std::optional<base::Value> result = base::test::ParseJson(expectation);
    return std::move(*result).TakeDict();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_refptr<ServiceWorkerContextWrapper> wrapper_;
};

// Tests using Request.text().
IN_PROC_BROWSER_TEST_F(ServiceWorkerFileUploadTest, AsText) {
  std::string filename;
  base::Value::Dict dict;
  RunRespondWithTest("getAs=text", TargetOrigin::kSameOrigin, &filename, dict);

  std::string boundary;
  GetKey(dict, "boundary", &boundary);
  std::string body;
  GetKey(dict, "body", &body);
  std::string expected_body = BuildExpectedBodyAsText(boundary, filename);
  EXPECT_EQ(expected_body, body);
}

// Tests using Request.blob().
IN_PROC_BROWSER_TEST_F(ServiceWorkerFileUploadTest, AsBlob) {
  std::string filename;
  base::Value::Dict dict;
  RunRespondWithTest("getAs=blob", TargetOrigin::kSameOrigin, &filename, dict);

  std::string boundary;
  GetKey(dict, "boundary", &boundary);
  int size;
  GetKey(dict, "bodySize", &size);
  std::string expected_body = BuildExpectedBodyAsText(boundary, filename);
  EXPECT_EQ(base::MakeStrictNum(expected_body.size()), size);
}

// Tests using Request.formData().
IN_PROC_BROWSER_TEST_F(ServiceWorkerFileUploadTest, AsFormData) {
  std::string filename;
  base::Value::Dict dict;
  RunRespondWithTest("getAs=formData", TargetOrigin::kSameOrigin, &filename,
                     dict);

  ExpectEqual(BuildExpectedBodyAsFormData(filename), dict);
}

// Tests network fallback.
IN_PROC_BROWSER_TEST_F(ServiceWorkerFileUploadTest, NetworkFallback) {
  RunNetworkFallbackTest(TargetOrigin::kSameOrigin);
}

// Tests using Request.formData() when the form was submitted to a cross-origin
// target. Regression test for https://crbug.com/916070.
IN_PROC_BROWSER_TEST_F(ServiceWorkerFileUploadTest, AsFormData_CrossOrigin) {
  std::string filename;
  base::Value::Dict dict;
  RunRespondWithTest("getAs=formData", TargetOrigin::kCrossOrigin, &filename,
                     dict);

  ExpectEqual(BuildExpectedBodyAsFormData(filename), dict);
}

// Tests network fallback when the form was submitted to a cross-origin target.
IN_PROC_BROWSER_TEST_F(ServiceWorkerFileUploadTest,
                       NetworkFallback_CrossOrigin) {
  RunNetworkFallbackTest(TargetOrigin::kCrossOrigin);
}

// Tests a subresource request.
// Flaky on Android; see https://crbug.com/1320972.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_Subresource DISABLED_Subresource
#else
#define MAYBE_Subresource Subresource
#endif
IN_PROC_BROWSER_TEST_P(ServiceWorkerFileUploadTest, MAYBE_Subresource) {
  // Prepare a file for the upload form.
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  base::FilePath file_path;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_dir.GetPath(), &file_path));
  ASSERT_TRUE(
      base::WriteFile(file_path, std::string_view(kFileContent, kFileSize)));

  std::string result;
  RunSubresourceTest(file_path, &result);

  // Test that the file name and contents are present.
  EXPECT_THAT(result,
              ::testing::HasSubstr(file_path.BaseName().MaybeAsASCII()));
  EXPECT_THAT(result, ::testing::HasSubstr(kFileContent));
}

// Tests a subresource request where the filename is non-ascii. Regression test
// for https://crbug.com/1017184.
// Flaky on Android; see https://crbug.com/1335344.
// Fail on Mac; see https://crbug.com/1320972.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_MAC)
#define MAYBE_Subresource_NonAsciiFilename DISABLED_Subresource_NonAsciiFilename
#else
#define MAYBE_Subresource_NonAsciiFilename Subresource_NonAsciiFilename
#endif
IN_PROC_BROWSER_TEST_P(ServiceWorkerFileUploadTest,
                       MAYBE_Subresource_NonAsciiFilename) {
  // "こんにちは"
  const base::FilePath::CharType nonAsciiFilename[] =
      FILE_PATH_LITERAL("\u3053\u3093\u306B\u3061\u306F");

  // Prepare a file for the upload form.
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path = temp_dir.GetPath().Append(nonAsciiFilename);
  ASSERT_TRUE(
      base::WriteFile(file_path, std::string_view(kFileContent, kFileSize)));

  std::string result;
  RunSubresourceTest(file_path, &result);

  // Test that the file name and contents are present. Repeat "こんにちは" here
  // since HasSubstr() doesn't work with FilePath::CharType on Windows.
  EXPECT_THAT(result, ::testing::HasSubstr("\u3053\u3093\u306B\u3061\u306F"));
  EXPECT_THAT(result, ::testing::HasSubstr(kFileContent));
}

INSTANTIATE_TEST_SUITE_P(All, ServiceWorkerFileUploadTest, ::testing::Bool());

}  // namespace content
