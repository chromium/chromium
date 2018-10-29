// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/threading/thread_restrictions.h"
#include "components/download/public/common/download_task_runner.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/common/frame_messages.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/mhtml_extra_parts.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/mhtml_generation_params.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/base/filename_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_frame_serializer_cache_control_policy.h"

using testing::ContainsRegex;
using testing::HasSubstr;
using testing::Not;

namespace content {

namespace {

// A dummy WebContentsDelegate which tracks the results of a find operation.
class FindTrackingDelegate : public WebContentsDelegate {
 public:
  explicit FindTrackingDelegate(const std::string& search)
      : search_(search), matches_(-1) {}

  // Returns number of results.
  int Wait(WebContents* web_contents) {
    WebContentsDelegate* old_delegate = web_contents->GetDelegate();
    web_contents->SetDelegate(this);

    auto options = blink::mojom::FindOptions::New();
    options->run_synchronously_for_testing = true;
    options->match_case = false;

    web_contents->Find(global_request_id++, base::UTF8ToUTF16(search_),
                       std::move(options));
    run_loop_.Run();

    web_contents->SetDelegate(old_delegate);

    return matches_;
  }

  void FindReply(WebContents* web_contents,
                 int request_id,
                 int number_of_matches,
                 const gfx::Rect& selection_rect,
                 int active_match_ordinal,
                 bool final_update) override {
    if (final_update) {
      matches_ = number_of_matches;
      run_loop_.Quit();
    }
  }

  static int global_request_id;

 private:
  std::string search_;
  int matches_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(FindTrackingDelegate);
};

// static
int FindTrackingDelegate::global_request_id = 0;

}  // namespace

class MHTMLGenerationTest : public ContentBrowserTest {
 public:
  MHTMLGenerationTest() : has_mhtml_callback_run_(false), file_size_(0) {}

 protected:
  void SetUpOnMainThread() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(embedded_test_server()->Start());
    ContentBrowserTest::SetUpOnMainThread();
  }

  void GenerateMHTML(const base::FilePath& path, const GURL& url) {
    GenerateMHTML(MHTMLGenerationParams(path), url);
  }

  void GenerateMHTML(const MHTMLGenerationParams& params, const GURL& url) {
    NavigateToURL(shell(), url);
    GenerateMHTMLForCurrentPage(params);
  }

  void GenerateMHTMLForCurrentPage(const MHTMLGenerationParams& params) {
    base::RunLoop run_loop;
    histogram_tester_.reset(new base::HistogramTester());

    shell()->web_contents()->GenerateMHTML(
        params, base::BindOnce(&MHTMLGenerationTest::MHTMLGenerated,
                               base::Unretained(this), run_loop.QuitClosure()));

    // Block until the MHTML is generated.
    run_loop.Run();

    ASSERT_TRUE(has_mhtml_callback_run())
        << "Unexpected error generating MHTML file";

    // Skip well formedness check if there was an generation error.
    if (file_size() == -1)
      return;

    // Loads the generated file to check if it is well formed.
    WebContentsDelegate* old_delegate = shell()->web_contents()->GetDelegate();
    ConsoleObserverDelegate console_delegate(shell()->web_contents(),
                                             "Malformed multipart archive: *");
    shell()->web_contents()->SetDelegate(&console_delegate);

    EXPECT_TRUE(
        NavigateToURL(shell(), net::FilePathToFileURL(params.file_path)))
        << "Error navigating to the generated MHTML file";
    EXPECT_EQ(0U, console_delegate.message().length())
        << "The generated MHTML file is malformed";

    shell()->web_contents()->SetDelegate(old_delegate);
  }

  int64_t ReadFileSizeFromDisk(base::FilePath path) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    int64_t file_size;
    if (!base::GetFileSize(path, &file_size)) return -1;
    return file_size;
  }

  void TestOriginalVsSavedPage(
      const GURL& url,
      const MHTMLGenerationParams params,
      int expected_number_of_frames,
      const std::vector<std::string>& expected_substrings,
      const std::vector<std::string>& forbidden_substrings_in_saved_page,
      bool skip_verification_of_original_page = false) {
    // Navigate to the test page and verify if test expectations
    // are met (this is mostly a sanity check - a failure to meet
    // expectations would probably mean that there is a test bug
    // (i.e. that we got called with wrong expected_foo argument).
    NavigateToURL(shell(), url);
    if (!skip_verification_of_original_page) {
      AssertExpectationsAboutCurrentTab(expected_number_of_frames,
                                        expected_substrings,
                                        std::vector<std::string>());
    }

    GenerateMHTML(params, url);

    // Stop the test server (to make sure the locally saved page
    // is self-contained / won't try to open original resources).
    ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());

    // Open the saved page and verify if test expectations are
    // met (i.e. if the same expectations are met for "after"
    // [saved version of the page] as for the "before"
    // [the original version of the page].
    NavigateToURL(shell(), net::FilePathToFileURL(params.file_path));
    AssertExpectationsAboutCurrentTab(expected_number_of_frames,
                                      expected_substrings,
                                      forbidden_substrings_in_saved_page);
  }

  void AssertExpectationsAboutCurrentTab(
      int expected_number_of_frames,
      const std::vector<std::string>& expected_substrings,
      const std::vector<std::string>& forbidden_substrings) {
    int actual_number_of_frames =
        shell()->web_contents()->GetAllFrames().size();
    EXPECT_EQ(expected_number_of_frames, actual_number_of_frames);

    for (const auto& expected_substring : expected_substrings) {
      FindTrackingDelegate delegate(expected_substring);
      int actual_number_of_matches = delegate.Wait(shell()->web_contents());
      EXPECT_EQ(1, actual_number_of_matches)
          << "Verifying that \"" << expected_substring << "\" appears "
          << "exactly once in the text of web contents of "
          << shell()->web_contents()->GetURL().spec();
    }

    for (const auto& forbidden_substring : forbidden_substrings) {
      FindTrackingDelegate delegate(forbidden_substring);
      int actual_number_of_matches = delegate.Wait(shell()->web_contents());
      EXPECT_EQ(0, actual_number_of_matches)
          << "Verifying that \"" << forbidden_substring << "\" doesn't "
          << "appear in the text of web contents of "
          << shell()->web_contents()->GetURL().spec();
    }
  }

  bool has_mhtml_callback_run() const { return has_mhtml_callback_run_; }
  int64_t file_size() const { return file_size_; }
  base::HistogramTester* histogram_tester() { return histogram_tester_.get(); }

  base::ScopedTempDir temp_dir_;

 private:
  void MHTMLGenerated(base::Closure quit_closure, int64_t size) {
    has_mhtml_callback_run_ = true;
    file_size_ = size;
    std::move(quit_closure).Run();
  }

  bool has_mhtml_callback_run_;
  int64_t file_size_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

// Tests that generating a MHTML does create contents.
// Note that the actual content of the file is not tested, the purpose of this
// test is to ensure we were successful in creating the MHTML data from the
// renderer.
IN_PROC_BROWSER_TEST_F(MHTMLGenerationTest, GenerateMHTML) {
  base::FilePath path(temp_dir_.GetPath());
  path = path.Append(FILE_PATH_LITERAL("test.mht"));

  GenerateMHTML(path, embedded_test_server()->GetURL("/simple_page.html"));

  // Make sure the actual generated file has some contents.
  EXPECT_GT(file_size(), 0);  // Verify the size reported by the callback.
  EXPECT_GT(ReadFileSizeFromDisk(path), 100);  // Verify the actual file size.

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    std::string mhtml;
    ASSERT_TRUE(base::ReadFileToString(path, &mhtml));
    EXPECT_THAT(mhtml,
                HasSubstr("Content-Transfer-Encoding: quoted-printable"));
  }

  // Checks that the final status reported to UMA is correct.
  histogram_tester()->ExpectUniqueSample(
      "PageSerialization.MhtmlGeneration.FinalSaveStatus",
      static_cast<int>(MhtmlSaveStatus::SUCCESS), 1);
}

class GenerateMHTMLAndExitRendererMessageFilter : public BrowserMessageFilter {
 public:
  GenerateMHTMLAndExitRendererMessageFilter(
      RenderProcessHostImpl* render_process_host)
      : BrowserMessageFilter(FrameMsgStart),
        render_process_host_(render_process_host) {}

 protected:
  ~GenerateMHTMLAndExitRendererMessageFilter() override {}

 private:
  bool OnMessageReceived(const IPC::Message& message) override {
    if (message.type() == FrameHostMsg_SerializeAsMHTMLResponse::ID) {
      // After |return false| below, this IPC message will be handled by the
      // product code as illustrated below.  (1), (2), (3) depict points in time
      // when product code runs on UI thread and download sequence.  (X), (Y),
      // (Z) depict when we want test-injected tasks to run - for the repro, (Z)
      // has to happen between (1) and (3).  (Y?) and (Z?) depict when test
      // tasks can theoretically happen and ruin the repro.
      //
      //     IO thread       UI thread         download sequence
      //     ---------       ---------           -----------
      //        |                |                     |
      //    WE ARE HERE          |                     |
      //        |                |                     |
      // after |return false|    |                     |
      //        +--------------->+                     |
      //        |                |                     |
      //        |               (X)                    |
      //        |                |                     |
      //        |                |                    (Y?)
      //        |               (Z?)                   |
      //        |                |                     |
      // (1)    |      MHTMLGenerationManager          |
      //        |      ::OnSerializeAsMHTMLResponse    |
      //        |                +-------------------->+
      //        |                |                     |
      //        |                |                    (Y)
      //        |                |                     |
      // (2)    |                |          MHTMLGenerationManager::Job
      //        |                |          ::CloseFileOnFileThread
      //        |                |                     |
      //        |               (Z)                    |
      //        |         test needs to inject         |
      //        |        fast renderer shutdown        |
      //        |      HERE - between (1) and (3)      |
      //        |                |                     |
      //        |                |                     |
      //        |                +<--------------------+
      //        |                |                     |
      // (3)    |      MHTMLGenerationManager          |
      //        |      ::OnFileClosed                  |
      //        |                |                     |
      //
      // We hope that (Z) happens between (1) and (3) by doing the following:
      // - From here post TaskX to UI thread.  (X) is guaranteed to happen
      //   before timepoint (1) (because posting of (1) happens after
      //   |return false| / before we post TaskX below).
      // - From (X) post TaskY to download sequence.  Because this posting is
      //   done before (1), we can guarantee that (Y) will happen before (2).
      // - From (Y) post TaskZ to UI thread.  Because this posting is done
      //   before (2), we can guarantee that (Z) will happen before (3).
      // - We cannot really guarantee that (Y) and (Z) happen *after* (1) - i.e.
      //   execution at (Y?) and (Z?) instead is possible.  In practice,
      //   bouncing off of UI and download sequence does mean (Z) happens
      //   after (1).
      base::PostTaskWithTraits(
          FROM_HERE, {BrowserThread::UI},
          base::BindOnce(&GenerateMHTMLAndExitRendererMessageFilter::TaskX,
                         base::Unretained(this)));
    }

    return false;
  };

  void TaskX() {
    download::GetDownloadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&GenerateMHTMLAndExitRendererMessageFilter::TaskY,
                       base::Unretained(this)));
  }

  void TaskY() {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&GenerateMHTMLAndExitRendererMessageFilter::TaskZ,
                       base::Unretained(this)));
  }

  void TaskZ() {
    render_process_host_->FastShutdownIfPossible();
  }

  RenderProcessHostImpl* render_process_host_;

  DISALLOW_COPY_AND_ASSIGN(GenerateMHTMLAndExitRendererMessageFilter);
};

// Regression test for the crash/race from https://crbug.com/612098.
IN_PROC_BROWSER_TEST_F(MHTMLGenerationTest, GenerateMHTMLAndExitRenderer) {
  NavigateToURL(shell(), embedded_test_server()->GetURL("/simple_page.html"));

  RenderProcessHostImpl* render_process_host =
      static_cast<RenderProcessHostImpl*>(
          shell()->web_contents()->GetMainFrame()->GetProcess());
  scoped_refptr<BrowserMessageFilter> filter =
      new GenerateMHTMLAndExitRendererMessageFilter(render_process_host);
  render_process_host->AddFilter(filter.get());

  base::FilePath path(temp_dir_.GetPath());
  path = path.Append(FILE_PATH_LITERAL("test.mht"));
  GenerateMHTMLForCurrentPage(MHTMLGenerationParams(path));

  EXPECT_GT(ReadFileSizeFromDisk(path), 100);  // Verify the actual file size.
}

// TODO(crbug.com/672313): Flaky on Windows.
#if defined(OS_WIN)
#define MAYBE_InvalidPath DISABLED_InvalidPath
#else
#define MAYBE_InvalidPath InvalidPath
#endif
IN_PROC_BROWSER_TEST_F(MHTMLGenerationTest, MAYBE_InvalidPath) {
  base::FilePath path(FILE_PATH_LITERAL("/invalid/file/path"));

  GenerateMHTML(path, embedded_test_server()->GetURL(
                          "/download/local-about-blank-subframes.html"));

  EXPECT_EQ(file_size(), -1);  // Expecting that the callback reported failure.

  // Checks that the final status reported to UMA is correct.
  histogram_tester()->ExpectUniqueSample(
      "PageSerialization.MhtmlGeneration.FinalSaveStatus",
      static_cast<int>(MhtmlSaveStatus::FILE_CREATION_ERROR), 1);
}

// Tests that MHTML generated using the default 'quoted-printable' encoding does
// not contain the 'binary' Content-Transfer-Encoding header, and generates
// base64 encoding for the image part.
IN_PROC_BROWSER_TEST_F(MHTMLGenerationTest, GenerateNonBinaryMHTMLWithImage) {
  base::FilePath path(temp_dir_.GetPath());
  path = path.Append(FILE_PATH_LITERAL("test_binary.mht"));

  GURL url(embedded_test_server()->GetURL("/page_with_image.html"));
  GenerateMHTML(path, url);
  EXPECT_GT(file_size(), 0);  // Verify the size reported by the callback.
  EXPECT_GT(ReadFileSizeFromDisk(path), 100);  // Verify the actual file size.

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    std::string mhtml;
    ASSERT_TRUE(base::ReadFileToString(path, &mhtml));
    EXPECT_THAT(mhtml, HasSubstr("Content-Transfer-Encoding: base64"));
    EXPECT_THAT(mhtml, Not(HasSubstr("Content-Transfer-Encoding: binary")));
    EXPECT_THAT(mhtml, ContainsRegex("Content-Location:.*blank.jpg"));
    // Verify the boundary should start with CRLF.
    EXPECT_THAT(mhtml, HasSubstr("\r\n------MultipartBoundary"));
  }
}

// Tests that MHTML generated using the binary encoding contains the 'binary'
// Content-Transfer-Encoding header, and does not contain any base64 encoded
// parts.
IN_PROC_BROWSER_TEST_F(MHTMLGenerationTest, GenerateBinaryMHTMLWithImage) {
  base::FilePath path(temp_dir_.GetPath());
  path = path.Append(FILE_PATH_LITERAL("test_binary.mht"));

  GURL url(embedded_test_server()->GetURL("/page_with_image.html"));
  MHTMLGenerationParams params(path);
  params.use_binary_encoding = true;

  GenerateMHTML(params, url);
  EXPECT_GT(file_size(), 0);  // Verify the size reported by the callback.
  EXPECT_GT(ReadFileSizeFromDisk(path), 100);  // Verify the actual file size.

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    std::string mhtml;
    ASSERT_TRUE(base::ReadFileToString(path, &mhtml));
    EXPECT_THAT(mhtml, HasSubstr("Content-Transfer-Encoding: binary"));
    EXPECT_THAT(mhtml, Not(HasSubstr("Content-Transfer-Encoding: base64")));
    EXPECT_THAT(mhtml, ContainsRegex("Content-Location:.*blank.jpg"));
    // Verify the boundary should start with CRLF.
    EXPECT_THAT(mhtml, HasSubstr("\r\n------MultipartBoundary"));
  }
}

IN_PROC_BROWSER_TEST_F(MHTMLGenerationTest, GenerateMHTMLIgnoreNoStore) {
  base::FilePath path(temp_dir_.GetPath());
  path = path.Append(FILE_PATH_LITERAL("test.mht"));

  GURL url(embedded_test_server()->GetURL("/nostore.html"));

  // Generate MHTML without specifying the FailForNoStoreMainFrame policy.
  GenerateMHTML(path, url);

  std::string mhtml;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::ReadFileToString(path, &mhtml));
  }

  // Make sure the contents of the body are present.
  EXPECT_THAT(mhtml, HasSubstr("test body"));

  // Make sure that URL of the content is present.
  EXPECT_THAT(mhtml, ContainsRegex("Content-Location:.*/nostore.html"));
}

IN_PROC_BROWSER_TEST_F(MHTMLGenerationTest, GenerateMHTMLObeyNoStoreMainFrame) {
  base::FilePath path(temp_dir_.GetPath());
  path = path.Append(FILE_PATH_LITERAL("test.mht"));

  GURL url(embedded_test_server()->GetURL("/nostore.html"));

  // Generate MHTML, specifying the FailForNoStoreMainFrame policy.
  MHTMLGenerationParams params(path);
  params.cache_control_policy =
      blink::WebFrameSerializerCacheControlPolicy::kFailForNoStoreMainFrame;

  GenerateMHTML(params, url);
  // We expect that there was an error (file size -1 indicates an error.)
  EXPECT_EQ(-1, file_size());

  std::string mhtml;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::ReadFileToString(path, &mhtml));
  }

  // Make sure the contents are missing.
  EXPECT_THAT(mhtml, Not(HasSubstr("test body")));

  // Checks that the final status reported to UMA is correct.
  histogram_tester()->ExpectUniqueSample(
      "PageSerialization.MhtmlGeneration.FinalSaveStatus",
      static_cast<int>(MhtmlSaveStatus::FRAME_SERIALIZATION_FORBIDDEN), 1);
}

IN_PROC_BROWSER_TEST_F(MHTMLGenerationTest,
                       GenerateMHTMLIgnoreNoStoreSubFrame) {
  base::FilePath path(temp_dir_.GetPath());
  path = path.Append(FILE_PATH_LITERAL("test.mht"));

  GURL url(embedded_test_server()->GetURL("/page_with_nostore_iframe.html"));

  // Generate MHTML, specifying the FailForNoStoreMainFrame policy.
  MHTMLGenerationParams params(path);
  params.cache_control_policy =
      blink::WebFrameSerializerCacheControlPolicy::kFailForNoStoreMainFrame;

  GenerateMHTML(params, url);
  // We expect that there was no error (file size -1 indicates an error.)
  EXPECT_LT(0, file_size());

  std::string mhtml;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::ReadFileToString(path, &mhtml));
  }

  EXPECT_THAT(mhtml, HasSubstr("Main Frame"));
  // Make sure that no-store subresources exist in this mode.
  EXPECT_THAT(mhtml, HasSubstr("no-store test body"));
  EXPECT_THAT(mhtml, ContainsRegex("Content-Location:.*nostore.jpg"));
}

IN_PROC_BROWSER_TEST_F(MHTMLGenerationTest, GenerateMHTMLObeyNoStoreSubFrame) {
  base::FilePath path(temp_dir_.GetPath());
  path = path.Append(FILE_PATH_LITERAL("test.mht"));

  GURL url(embedded_test_server()->GetURL("/page_with_nostore_iframe.html"));

  // Generate MHTML, specifying the FailForNoStoreMainFrame policy.
  MHTMLGenerationParams params(path);
  params.cache_control_policy = blink::WebFrameSerializerCacheControlPolicy::
      kSkipAnyFrameOrResourceMarkedNoStore;

  GenerateMHTML(params, url);
  // We expect that there was no error (file size -1 indicates an error.)
  EXPECT_LT(0, file_size());

  std::string mhtml;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::ReadFileToString(path, &mhtml));
  }

  EXPECT_THAT(mhtml, HasSubstr("Main Frame"));
  // Make sure the contents are missing.
  EXPECT_THAT(mhtml, Not(HasSubstr("no-store test body")));
  // This image comes from a resource marked no-store.
  EXPECT_THAT(mhtml, Not(ContainsRegex("Content-Location:.*nostore.jpg")));
}

// TODO(crbug.com/615291): These fail on Android under some circumstances.
#if defined(OS_ANDROID)
#define MAYBE_ViewedMHTMLContainsNoStoreContentIfNoCacheControlPolicy \
    DISABLED_ViewedMHTMLContainsNoStoreContentIfNoCacheControlPolicy
#define MAYBE_ViewedMHTMLDoesNotContainNoStoreContent \
    DISABLED_ViewedMHTMLDoesNotContainNoStoreContent
#else
#define MAYBE_ViewedMHTMLContainsNoStoreContentIfNoCacheControlPolicy \
    ViewedMHTMLContainsNoStoreContentIfNoCacheControlPolicy
#define MAYBE_ViewedMHTMLDoesNotContainNoStoreContent \
    ViewedMHTMLDoesNotContainNoStoreContent
#endif

IN_PROC_BROWSER_TEST_F(
    MHTMLGenerationTest,
    MAYBE_ViewedMHTMLContainsNoStoreContentIfNoCacheControlPolicy) {
  // Generate MHTML, specifying the FailForNoStoreMainFrame policy.
  base::FilePath path(temp_dir_.GetPath());
  path = path.Append(FILE_PATH_LITERAL("test.mht"));
  MHTMLGenerationParams params(path);

  // No special cache control options so we should see both frames.
  std::vector<std::string> expectations = {
      "Main Frame, normal headers.", "Cache-Control: no-store test body",
  };
  std::vector<std::string> forbidden;
  TestOriginalVsSavedPage(
      embedded_test_server()->GetURL("/page_with_nostore_iframe.html"), params,
      2 /* expected number of frames */, expectations, forbidden);

  std::string mhtml;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::ReadFileToString(params.file_path, &mhtml));
  }
}

IN_PROC_BROWSER_TEST_F(MHTMLGenerationTest,
                       MAYBE_ViewedMHTMLDoesNotContainNoStoreContent) {
  // Generate MHTML, specifying the FailForNoStoreMainFrame policy.
  base::FilePath path(temp_dir_.GetPath());
  path = path.Append(FILE_PATH_LITERAL("test.mht"));
  MHTMLGenerationParams params(path);
  params.cache_control_policy = blink::WebFrameSerializerCacheControlPolicy::
      kSkipAnyFrameOrResourceMarkedNoStore;

  // No special cache control options so we should see both frames.
  std::vector<std::string> expectations = {
      "Main Frame, normal headers.",
  };
  std::vector<std::string> forbidden = {
      "Cache-Control: no-store test body",
  };
  TestOriginalVsSavedPage(
      embedded_test_server()->GetURL("/page_with_nostore_iframe.html"), params,
      2 /* expected number of frames */, expectations, forbidden);

  std::string mhtml;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::ReadFileToString(params.file_path, &mhtml));
  }
}

// Test suite that allows testing --site-per-process against cross-site frames.
// See http://dev.chromium.org/developers/design-documents/site-isolation.
class MHTMLGenerationSitePerProcessTest : public MHTMLGenerationTest {
 public:
  MHTMLGenerationSitePerProcessTest() {}

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    MHTMLGenerationTest::SetUpCommandLine(command_line);

    // Append --site-per-process flag.
    content::IsolateAllSitesForTesting(command_line);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(embedded_test_server());

    MHTMLGenerationTest::SetUpOnMainThread();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MHTMLGenerationSitePerProcessTest);
};

// Test for crbug.com/538766.
IN_PROC_BROWSER_TEST_F(MHTMLGenerationSitePerProcessTest, GenerateMHTML) {
  base::FilePath path(temp_dir_.GetPath());
  path = path.Append(FILE_PATH_LITERAL("test.mht"));

  GURL url(embedded_test_server()->GetURL(
      "a.com", "/frame_tree/page_with_one_frame.html"));
  GenerateMHTML(path, url);

  std::string mhtml;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::ReadFileToString(path, &mhtml));
  }

  // Make sure the contents of both frames are present.
  EXPECT_THAT(mhtml, HasSubstr("This page has one cross-site iframe"));
  EXPECT_THAT(mhtml, HasSubstr("This page has no title"));  // From title1.html.

  // Make sure that URLs of both frames are present
  // (note that these are single-line regexes).
  EXPECT_THAT(
      mhtml,
      ContainsRegex("Content-Location:.*/frame_tree/page_with_one_frame.html"));
  EXPECT_THAT(mhtml, ContainsRegex("Content-Location:.*/title1.html"));
}

IN_PROC_BROWSER_TEST_F(MHTMLGenerationTest, RemovePopupOverlay) {
  base::FilePath path(temp_dir_.GetPath());
  path = path.Append(FILE_PATH_LITERAL("test.mht"));

  GURL url(embedded_test_server()->GetURL("/popup.html"));

  MHTMLGenerationParams params(path);
  params.remove_popup_overlay = true;

  GenerateMHTML(params, url);

  std::string mhtml;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::ReadFileToString(path, &mhtml));
  }

  // Make sure the overlay is removed.
  EXPECT_THAT(mhtml, Not(HasSubstr("class=3D\"overlay")));
  EXPECT_THAT(mhtml, Not(HasSubstr("class=3D\"modal")));
}

IN_PROC_BROWSER_TEST_F(MHTMLGenerationTest, GenerateMHTMLWithExtraData) {
  const char kFakeSignalData1[] = "FakeSignalData1";
  const char kFakeSignalData2[] = "OtherMockDataForSignals";
  const char kFakeContentType[] = "text/plain";
  const char kFakeContentLocation[] =
      "cid:signal-data-62691-645341c4-62b3-478e-a8c5-e0dfccc3ca02@mhtml.blink";
  base::FilePath path(temp_dir_.GetPath());
  path = path.Append(FILE_PATH_LITERAL("test.mht"));
  GURL url(embedded_test_server()->GetURL("/page_with_image.html"));
  MHTMLGenerationParams params(path);

  // Place the extra data we need into the web contents user data.
  std::string content_type(kFakeContentType);
  std::string content_location(kFakeContentLocation);
  std::string extra_headers;

  // Get the MHTMLExtraParts
  MHTMLExtraParts* extra_parts =
      MHTMLExtraParts::FromWebContents(shell()->web_contents());

  // Add two extra data parts to the MHTML.
  extra_parts->AddExtraMHTMLPart(content_type, content_location, extra_headers,
                                 kFakeSignalData1);
  extra_parts->AddExtraMHTMLPart(content_type, content_location, extra_headers,
                                 kFakeSignalData2);
  EXPECT_EQ(extra_parts->size(), 2);
  GenerateMHTML(params, url);

  EXPECT_TRUE(has_mhtml_callback_run());

  std::string mhtml;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::ReadFileToString(path, &mhtml));
  }

  // Make sure that both extra data parts made it into the mhtml.
  EXPECT_THAT(mhtml, HasSubstr(kFakeSignalData1));
  EXPECT_THAT(mhtml, HasSubstr(kFakeSignalData2));
}

}  // namespace content
