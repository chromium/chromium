// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "components/download/public/common/download_task_runner.h"
#include "content/browser/download/mhtml_generation_manager.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/common/download/mhtml_file_writer.mojom.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/mhtml_extra_parts.h"
#include "content/public/browser/mhtml_generation_result.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/mhtml_generation_params.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "net/base/filename_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/frame/find_in_page.mojom.h"

#if BUILDFLAG(IS_WIN)
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#endif  // BUILDFLAG(IS_WIN)

using testing::Contains;
using testing::EndsWith;
using testing::HasSubstr;
using testing::IsEmpty;
using testing::Not;

namespace content {

namespace {

const char kGetPageInfoScript[] = R"js(

// This script is evaluated after loading the original page, and after loading
// the saved page. It returns an object that should usually be equivalent
// between the original and saved pages.
(() => {
  // Pick a subset of styles to export to keep the output size down.
  const styleKeys = ['font-family', 'line-height', 'display'];
  function elementStyles(el) {
    const styles = window.getComputedStyle(el);
    return Object.fromEntries(styleKeys.map(name => [name, styles[name]]));
  }
  function isVisible(el) {
    const styles = window.getComputedStyle(el);
    return styles.display !== 'none';
  }

  return {
    title: document.title,
    innerText: document.body.innerText,
    // loaded state of visible image elements.
    images: Array.from(document.querySelectorAll('img'))
        .filter(isVisible)
        .map(i => i.complete ? 'loaded' : 'not_loaded'),
    // Computed styles for elements with ids.
    computedStyles: Array.from(document.querySelectorAll('*'))
        .filter(e => e.id && isVisible(e))
        .map(e => [e.id, elementStyles(e)])
        .filter(e => e),
  };
})()

)js";

// Information about the MHTML file.
class MHTMLFileInfo {
 public:
  MHTMLFileInfo() = default;
  explicit MHTMLFileInfo(const base::FilePath& path) : path_(path) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::ReadFileToString(path, &content_);
  }

  const base::FilePath& path() const { return path_; }

  const std::string& content() const { return content_; }

  std::vector<std::string> ContentLocations() {
    std::vector<std::string> parts = base::SplitStringUsingSubstr(
        content_,
        "\r\nContent-Location: ", base::WhitespaceHandling::KEEP_WHITESPACE,
        base::SplitResult::SPLIT_WANT_ALL);
    std::vector<std::string> result;
    for (size_t i = 1; i < parts.size(); ++i) {
      const std::string& part = parts[i];
      auto pos = part.find('\r');
      if (pos != std::string::npos) {
        result.push_back(part.substr(0, pos));
      }
    }
    return result;
  }

 private:
  base::FilePath path_;
  std::string content_;
};

struct CompareOptions {
  std::optional<int> expected_number_of_frames;
  // Strings that must be present in the original and saved pages.
  std::vector<std::string> expected_substrings;
  // Forbidden strings for both the original and saved pages.
  std::vector<std::string> forbidden_substrings;
  // Forbidden strings for the saved page.
  std::vector<std::string> forbidden_substrings_in_saved_page;
};

struct CompareResult {
  // Output of `kGetPageInfoScript` for the original page.
  base::Value original_info;
  // Output of `kGetPageInfoScript` for the saved page.
  base::Value saved_info;
  MHTMLFileInfo file;
};

// A dummy WebContentsDelegate which tracks the results of a find operation.
class FindTrackingDelegate : public WebContentsDelegate {
 public:
  explicit FindTrackingDelegate(const std::string& search)
      : search_(search), matches_(-1) {}

  FindTrackingDelegate(const FindTrackingDelegate&) = delete;
  FindTrackingDelegate& operator=(const FindTrackingDelegate&) = delete;

  // Returns number of results.
  int Wait(WebContents* web_contents) {
    WebContentsDelegate* old_delegate = web_contents->GetDelegate();
    web_contents->SetDelegate(this);

    auto options = blink::mojom::FindOptions::New();
    options->run_synchronously_for_testing = true;
    options->match_case = false;

    web_contents->Find(global_request_id++, base::UTF8ToUTF16(search_),
                       std::move(options), /*skip_delay=*/false);
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
};

// static
int FindTrackingDelegate::global_request_id = 0;

const std::string_view kTestData =
    "Sample Text to write on a generated MHTML "
    "file for tests to validate whether the implementation is able to access "
    "and write to the file.";

class MockWriterBase : public mojom::MhtmlFileWriter {
 public:
  MockWriterBase() = default;

  MockWriterBase(const MockWriterBase&) = delete;
  MockWriterBase& operator=(const MockWriterBase&) = delete;

  ~MockWriterBase() override = default;

  void BindReceiver(mojo::ScopedInterfaceEndpointHandle handle) {
    receiver_.Bind(mojo::PendingAssociatedReceiver<mojom::MhtmlFileWriter>(
        std::move(handle)));
  }

 protected:
  void SendResponse(SerializeAsMHTMLCallback callback) {
    std::vector<std::string> dummy_digests;
    std::move(callback).Run(mojom::MhtmlSaveStatus::kSuccess, dummy_digests);
  }

  void WriteDataToDestinationFile(base::File& destination_file) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    destination_file.WriteAtCurrentPos(base::as_byte_span(kTestData));
    destination_file.Close();
  }

  void WriteDataToProducerPipe(
      mojo::ScopedDataPipeProducerHandle producer_pipe) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    size_t actually_written_bytes = 0;
    producer_pipe->WriteData(base::as_byte_span(kTestData),
                             MOJO_WRITE_DATA_FLAG_NONE, actually_written_bytes);
    producer_pipe.reset();
  }

  mojo::AssociatedReceiver<mojom::MhtmlFileWriter> receiver_{this};
};

// This Mock injects our overwritten interface, running the callback
// SerializeAsMHTMLResponse and immediately disconnecting the message pipe.
class RespondAndDisconnectMockWriter
    : public MockWriterBase,
      public base::RefCountedThreadSafe<RespondAndDisconnectMockWriter> {
 public:
  RespondAndDisconnectMockWriter() = default;

  RespondAndDisconnectMockWriter(const RespondAndDisconnectMockWriter&) =
      delete;
  RespondAndDisconnectMockWriter& operator=(
      const RespondAndDisconnectMockWriter&) = delete;

  void SerializeAsMHTML(mojom::SerializeAsMHTMLParamsPtr params,
                        SerializeAsMHTMLCallback callback) override {
    // Upon using the overridden mock interface implementation, this will be
    // handled by the product code as illustrated below.  (1), (2), (3) depict
    // points in time when product code runs on UI thread and download sequence.
    // For the repro, the message pipe disconnection needs to happen between (1)
    // and (3).
    //
    //   Test instance     UI thread         download sequence
    //     ---------       ---------           -----------
    //        |                |                     |
    //    WE ARE HERE          |                     |
    //        |                |                     |
    //        |                |                     |
    //        +--------------->+                     |
    //        |                |                     |
    //        |                |                     |
    //        |                |                     |
    //        |                |                     |
    //        |                |                     |
    //        |                |                     |
    // (1)    |      MHTMLGenerationManager::Job     |
    //        |      ::SerializeAsMHTMLResponse      |
    //        |                +-------------------->+
    //        |                |                     |
    //        |                |                     |
    //        |                |                     |
    // (2)    |                |          MHTMLGenerationManager::Job
    //        |                |          ::CloseFileOnFileThread
    //        |                |                     |
    //        |                |                     |
    //        |           test needs to              |
    //        |       disconnect message pipe        |
    //        |      HERE - between (1) and (3)      |
    //        |                |                     |
    //        |                |                     |
    //        |                +<--------------------+
    //        |                |                     |
    // (3)    |      MHTMLGenerationManager          |
    //        |      Job::OnFinished                 |
    //        |                |                     |
    //
    // We hope that the error handler is invoked between (1) and (3) by doing
    // the following:
    // - From here, run the callback response to the UI thread. This queues
    //   the response message onto the bound message pipe.
    // - After running the callback response, immediately unbind the message
    //   pipe in order to queue a message onto the bound message pipe to notify
    //   the Browser the connection was closed and invoke the error handler.
    // - Upon resuming operation, the FIFO ordering property of associated
    //   interfaces guarantees the execution of (1) before the error handler.
    //   (1) posts (2) to the download sequence and terminates. The client end
    //   then accepts the error notification and invokes the connection error
    //   handler, guaranteeing its execution before (3).

    bool compute_contents_hash = params->output_handle->is_producer_handle();

    // Write a valid MHTML file to its respective handle, since we are not
    // actively running a serialization pipeline in the mock implementation.
    if (compute_contents_hash) {
      WriteDataToProducerPipe(
          std::move(params->output_handle->get_producer_handle()));
    } else {
      WriteDataToDestinationFile(params->output_handle->get_file_handle());
    }

    SendResponse(std::move(callback));

    // Reset the message pipe connection to invoke the disconnect callback. The
    // disconnect handler from here will finalize the Job and attempt to call
    // MHTMLGenerationManager::Job::CloseFile a second time. If this situation
    // is handled correctly, the browser file should be invalidated and
    // idempotent.
    if (!compute_contents_hash) {
      receiver_.reset();
      return;
    }

    // In the case we are using a data pipe to stream serialized MHTML data,
    // we must ensure the write complete notification arrives before the
    // connection error notification, otherwise the Browser will report
    // an MhtmlSaveStatus != kSuccess. We can guarantee this by potentially
    // running tasks after each watcher invocation to send notifications that
    // it has been completed. We need at least two tasks to guarantee this,
    // as there can be at most two watcher invocations to write a block of
    // data smaller than the data pipe buffer to file.
    download::GetDownloadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&RespondAndDisconnectMockWriter::TaskX,
                       scoped_refptr<RespondAndDisconnectMockWriter>(this)));
  }

  void TaskX() {
    download::GetDownloadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&RespondAndDisconnectMockWriter::TaskY,
                       scoped_refptr<RespondAndDisconnectMockWriter>(this)));
  }

  void TaskY() {
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&RespondAndDisconnectMockWriter::TaskZ,
                       scoped_refptr<RespondAndDisconnectMockWriter>(this)));
  }

  void TaskZ() { receiver_.reset(); }

 private:
  friend base::RefCountedThreadSafe<RespondAndDisconnectMockWriter>;

  ~RespondAndDisconnectMockWriter() override = default;
};


class MHTMLGenerationTest : public ContentBrowserTest,
                            public testing::WithParamInterface<bool> {
 public:
  MHTMLGenerationTest() = default;

  enum TaskOrder { WriteThenRespond, RespondThenWrite };

 protected:
  void SetUpOnMainThread() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(embedded_test_server()->Start());
    ContentBrowserTest::SetUpOnMainThread();
  }

  void OverrideInterface(MockWriterBase* mock_writer) {
    blink::AssociatedInterfaceProvider* remote_interfaces =
        shell()
            ->web_contents()
            ->GetPrimaryMainFrame()
            ->GetRemoteAssociatedInterfaces();
    remote_interfaces->OverrideBinderForTesting(
        mojom::MhtmlFileWriter::Name_,
        base::BindRepeating(&MockWriterBase::BindReceiver,
                            base::Unretained(mock_writer)));
  }

  MHTMLFileInfo GenerateMHTML(base::FilePath& path, const GURL& url) {
    MHTMLGenerationParams params(path);
    return GenerateMHTML(params, url);
  }

  MHTMLFileInfo GenerateMHTML(MHTMLGenerationParams& params, const GURL& url) {
    EXPECT_TRUE(NavigateToURL(shell(), url));
    return GenerateMHTMLForCurrentPage(params);
  }

  // Loads the generated MHTML file to check if it is well formed.
  void NavigateToMHTML(const base::FilePath& path) {
    // Loads the generated file to check if it is well formed.
    WebContentsConsoleObserver console_observer(shell()->web_contents());
    console_observer.SetPattern("Malformed multipart archive: *");

    EXPECT_TRUE(NavigateToURL(shell(), net::FilePathToFileURL(path)))
        << "Error navigating to the generated MHTML file";
    EXPECT_TRUE(console_observer.messages().empty())
        << "The generated MHTML file is malformed";
  }

  // Extracts various information from the loaded page. Extracts information
  // that should be equivalent in the original and saved pages.
  base::Value GetPageInfo() {
    auto result = EvalJs(shell(), kGetPageInfoScript);
    EXPECT_EQ(result.error, "");
    return result.value.Clone();
  }

  MHTMLFileInfo GenerateMHTMLForCurrentPage(MHTMLGenerationParams& params) {
    base::RunLoop run_loop;
    histogram_tester_ = std::make_unique<base::HistogramTester>();

    bool use_result_callback = GetParam();

    if (use_result_callback) {
      shell()->web_contents()->GenerateMHTMLWithResult(
          params,
          base::BindOnce(&MHTMLGenerationTest::MHTMLGeneratedWithResult,
                         base::Unretained(this), run_loop.QuitClosure()));
    } else {
      shell()->web_contents()->GenerateMHTML(
          params,
          base::BindOnce(&MHTMLGenerationTest::MHTMLGenerated,
                         base::Unretained(this), run_loop.QuitClosure()));
    }

    // Block until the MHTML is generated.
    run_loop.Run();

    EXPECT_TRUE(has_mhtml_callback_run())
        << "Unexpected error generating MHTML file";
    if (!has_mhtml_callback_run()) {
      return MHTMLFileInfo(params.file_path);
    }

    // TODO(crbug.com/40641976): Add tests which will let MHTMLGeneration
    // manager fail during file write operation. This will allow us to actually
    // test if we receive a bogus hash instead of a std::nullopt.
    EXPECT_EQ(std::nullopt, file_digest());

    MHTMLFileInfo info(params.file_path);

    // Skip well formedness check if explicitly disabled or there was a
    // generation error.
    if (well_formedness_check_) {
      EXPECT_NE(file_size(), -1) << "GenerateMHTML callback wasn't called";
      EXPECT_THAT(info.content(), Not(IsEmpty()));
    }

    return info;
  }

  void TwoStepSyncTestFor(const TaskOrder order);

  int64_t ReadFileSizeFromDisk(base::FilePath path) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    int64_t file_size;
    if (!base::GetFileSize(path, &file_size)) return -1;
    return file_size;
  }

  CompareResult TestOriginalVsSavedPage(const GURL& url,
                                        MHTMLGenerationParams params,
                                        CompareOptions& options) {
    CompareResult result;
    // Navigate to the test page and verify if test expectations
    // are met (this is mostly a sanity check - a failure to meet
    // expectations would probably mean that there is a test bug
    // (i.e. that we got called with wrong expected_foo argument).
    EXPECT_TRUE(NavigateToURL(shell(), url));

    result.file = GenerateMHTML(params, url);
    result.original_info = GetPageInfo();
    AssertExpectationsAboutCurrentTab(options.expected_number_of_frames,
                                      options.expected_substrings,
                                      options.forbidden_substrings);

    // Stop the test server (to make sure the locally saved page
    // is self-contained / won't try to open original resources).
    if (!embedded_test_server()->ShutdownAndWaitUntilComplete()) {
      EXPECT_FALSE(true) << "ShutdownAndWaitUntilComplete failed";
      return result;
    }

    // Open the saved page and verify if test expectations are
    // met (i.e. if the same expectations are met for "after"
    // [saved version of the page] as for the "before"
    // [the original version of the page].
    NavigateToMHTML(params.file_path);
    result.saved_info = GetPageInfo();

    auto forbidden_strings = options.forbidden_substrings_in_saved_page;
    forbidden_strings.insert(forbidden_strings.end(),
                             options.forbidden_substrings.begin(),
                             options.forbidden_substrings.end());
    AssertExpectationsAboutCurrentTab(options.expected_number_of_frames,
                                      options.expected_substrings,
                                      forbidden_strings);
    return result;
  }

  void AssertExpectationsAboutCurrentTab(
      std::optional<int> expected_number_of_frames,
      const std::vector<std::string>& expected_substrings,
      const std::vector<std::string>& forbidden_substrings) {
    int actual_number_of_frames =
        CollectAllRenderFrameHosts(shell()->web_contents()->GetPrimaryPage())
            .size();
    if (expected_number_of_frames) {
      EXPECT_EQ(*expected_number_of_frames, actual_number_of_frames);
    }

    for (const auto& expected_substring : expected_substrings) {
      FindTrackingDelegate delegate(expected_substring);
      int actual_number_of_matches = delegate.Wait(shell()->web_contents());
      EXPECT_EQ(1, actual_number_of_matches)
          << "Verifying that \"" << expected_substring << "\" appears "
          << "exactly once in the text of web contents of "
          << shell()->web_contents()->GetLastCommittedURL().spec();
    }

    for (const auto& forbidden_substring : forbidden_substrings) {
      FindTrackingDelegate delegate(forbidden_substring);
      int actual_number_of_matches = delegate.Wait(shell()->web_contents());
      EXPECT_EQ(0, actual_number_of_matches)
          << "Verifying that \"" << forbidden_substring << "\" doesn't "
          << "appear in the text of web contents of "
          << shell()->web_contents()->GetLastCommittedURL().spec();
    }
  }

  // Tests that the result of setting compute_contents_hash is the same as
  // manually hashing the file. Because MHTMLGenerationManager depends on
  // net::GenerateMimeMultipartBoundary() to write the boundary, we cannot
  // compute the digest in advance. Therefore, we must compute the hash of the
  // whole file and assert that the computed hash is the same as the hash
  // produced here.
  void TestComputeContentsHash(base::FilePath& path) {
    base::ScopedAllowBlockingForTesting allow_blocking;

    // Reload the file to an mhtml string for hashing
    std::string test_mhtml;
    ASSERT_TRUE(base::ReadFileToString(path, &test_mhtml));

    // Hash the file in one big step. This is not recommended to do outside of
    // tests because the files being hashed could be too large.
    std::unique_ptr<crypto::SecureHash> secure_hash =
        crypto::SecureHash::Create(crypto::SecureHash::Algorithm::SHA256);
    secure_hash->Update(test_mhtml.c_str(), test_mhtml.size());
    std::string expected_digest(secure_hash->GetHashLength(), 0);
    secure_hash->Finish(&(expected_digest[0]), expected_digest.size());
    secure_hash.reset();

    ASSERT_TRUE(file_digest());
    EXPECT_EQ(file_digest().value(), expected_digest);
  }

  // In the case that we are using a pre-generated .mhtml file, we do
  // not have any control over the final mhtml_boundary_marker write
  // operation. This results in the post-generation verification tests
  // reporting a malformed multipart archive, unintentionally failing the
  // test.
  void DisableWellformednessCheck() { well_formedness_check_ = false; }

  bool has_mhtml_callback_run() const { return has_mhtml_callback_run_; }
  int64_t file_size() const { return file_size_; }
  std::optional<std::string> file_digest() const { return file_digest_; }
  base::HistogramTester* histogram_tester() { return histogram_tester_.get(); }

  base::ScopedTempDir temp_dir_;

 private:
  void MHTMLGenerated(base::OnceClosure quit_closure, int64_t size) {
    has_mhtml_callback_run_ = true;
    file_size_ = size;
    std::move(quit_closure).Run();
  }
  void MHTMLGeneratedWithResult(base::OnceClosure quit_closure,
                                const MHTMLGenerationResult& result) {
    has_mhtml_callback_run_ = true;
    file_size_ = result.file_size;
    file_digest_ = result.file_digest;
    std::move(quit_closure).Run();
  }

  bool has_mhtml_callback_run_ = false;
  int64_t file_size_ = 0;
  std::optional<std::string> file_digest_;
  bool well_formedness_check_ = true;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

class MHTMLGenerationImprovedTest : public MHTMLGenerationTest {
 public:
  MHTMLGenerationImprovedTest() {
    feature_list_.InitAndEnableFeature(blink::features::kMHTML_Improvements);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that generating a MHTML does create contents.
// Note that the actual content of the file is not tested, the purpose of this
// test is to ensure we were successful in creating the MHTML data from the
// renderer.
IN_PROC_BROWSER_TEST_P(MHTMLGenerationTest, GenerateMHTML) {
  base::FilePath path(temp_dir_.GetPath());
  path = path.Append(FILE_PATH_LITERAL("test.mht"));

  MHTMLFileInfo info =
      GenerateMHTML(path, embedded_test_server()->GetURL("/simple_page.html"));

  // Make sure the actual generated file has some contents.
  EXPECT_THAT(info.content(),
              HasSubstr("Content-Transfer-Encoding: quoted-printable"));
}

#if BUILDFLAG(IS_WIN)
// This Windows only test generates an MHTML file in a path that is explicitly
// not in the temp directory and not in the user data dir. This is to test that
// the mojo security constraints correctly allow this writeable handle to a
// renderer process. See `mojo/core/platform_handle_security_util_win.cc`.
IN_PROC_BROWSER_TEST_P(MHTMLGenerationTest, GenerateMHTMLInNonTempDir) {
  base::FilePath local_app_data;
  // This test creates a temporary directory in %LocalAppData% then deletes it
  // afterwards.
  EXPECT_TRUE(
      base::PathService::Get(base::DIR_LOCAL_APP_DATA, &local_app_data));
  base::FilePath new_dir;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(base::CreateTemporaryDirInDir(
        local_app_data, FILE_PATH_LITERAL("MHTMLGenerationTest"), &new_dir));
  }
  absl::Cleanup delete_dir = [new_dir] {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::DeletePathRecursively(new_dir);
  };

  base::FilePath path = new_dir.Append(FILE_PATH_LITERAL("test.mht"));

  MHTMLFileInfo info =
      GenerateMHTML(path, embedded_test_server()->GetURL("/simple_page.html"));

  EXPECT_THAT(info.content(),
              HasSubstr("Content-Transfer-Encoding: quoted-printable"));
}
#endif  // BUILDFLAG(IS_WIN)

// Regression test for the crash/race from https://crbug.com/612098.
//
// TODO(crbug.com/41456635): Flaky on Android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_GenerateMHTMLAndCloseConnection \
  DISABLED_GenerateMHTMLAndCloseConnection
#else
#define MAYBE_GenerateMHTMLAndCloseConnection GenerateMHTMLAndCloseConnection
#endif
IN_PROC_BROWSER_TEST_P(MHTMLGenerationTest,
                       MAYBE_GenerateMHTMLAndCloseConnection) {
  scoped_refptr<RespondAndDisconnectMockWriter> mock_writer =
      base::MakeRefCounted<RespondAndDisconnectMockWriter>();

  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/simple_page.html")));
  base::FilePath path(temp_dir_.GetPath());
  path = path.Append(FILE_PATH_LITERAL("test.mht"));

  OverrideInterface(mock_writer.get());
  DisableWellformednessCheck();

  MHTMLGenerationParams params(path);
  GenerateMHTMLForCurrentPage(params);

  // Verify the file has some contents written to it.
  EXPECT_GT(ReadFileSizeFromDisk(path), 100);
  // Verify the reported file size matches the file written to disk.
  EXPECT_EQ(ReadFileSizeFromDisk(path), file_size());
}

// TODO(crbug.com/41290169): Flaky on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_InvalidPath DISABLED_InvalidPath
#else
#define MAYBE_InvalidPath InvalidPath
#endif
IN_PROC_BROWSER_TEST_P(MHTMLGenerationTest, MAYBE_InvalidPath) {
  base::FilePath path(FILE_PATH_LITERAL("/invalid/file/path"));
  DisableWellformednessCheck();

  GenerateMHTML(path, embedded_test_server()->GetURL("/page_with_image.html"));

  EXPECT_EQ(file_size(), -1);  // Expecting that the callback reported failure.
}

// Tests that MHTML generated using the default 'quoted-printable' encoding does
// not contain the 'binary' Content-Transfer-Encoding header, and generates
// base64 encoding for the image part.
IN_PROC_BROWSER_TEST_P(MHTMLGenerationTest, GenerateNonBinaryMHTMLWithImage) {
  base::FilePath path(temp_dir_.GetPath());
  path = path.Append(FILE_PATH_LITERAL("test_binary.mht"));

  GURL url(embedded_test_server()->GetURL("/page_with_image.html"));
  MHTMLFileInfo info = GenerateMHTML(path, url);

  EXPECT_THAT(info.content(), HasSubstr("Content-Transfer-Encoding: base64"));
  EXPECT_THAT(info.content(),
              Not(HasSubstr("Content-Transfer-Encoding: binary")));
  EXPECT_THAT(info.ContentLocations(), Contains(EndsWith("blank.jpg")));
  // Verify the boundary should start with CRLF.
  EXPECT_THAT(info.content(), HasSubstr("\r\n------MultipartBoundary"));
}

// Tests that MHTML generated using the binary encoding contains the 'binary'
// Content-Transfer-Encoding header, and does not contain any base64 encoded
// parts.
IN_PROC_BROWSER_TEST_P(MHTMLGenerationTest, GenerateBinaryMHTMLWithImage) {
  base::FilePath path(temp_dir_.GetPath());
  path = path.Append(FILE_PATH_LITERAL("test_binary.mht"));

  GURL url(embedded_test_server()->GetURL("/page_with_image.html"));
  MHTMLGenerationParams params(path);
  params.use_binary_encoding = true;

  MHTMLFileInfo info = GenerateMHTML(params, url);

  EXPECT_THAT(info.content(), HasSubstr("Content-Transfer-Encoding: binary"));
  EXPECT_THAT(info.content(),
              Not(HasSubstr("Content-Transfer-Encoding: base64")));
  EXPECT_THAT(info.ContentLocations(), Contains(EndsWith("blank.jpg")));
  // Verify the boundary should start with CRLF.
  EXPECT_THAT(info.content(), HasSubstr("\r\n------MultipartBoundary"));
}

IN_PROC_BROWSER_TEST_P(MHTMLGenerationTest, GenerateMHTMLIgnoreNoStore) {
  base::FilePath path(temp_dir_.GetPath());
  path = path.Append(FILE_PATH_LITERAL("test.mht"));

  GURL url(embedded_test_server()->GetURL("/nostore.html"));

  // Generate MHTML without specifying the FailForNoStoreMainFrame policy.
  MHTMLFileInfo info = GenerateMHTML(path, url);

  // Make sure the contents of the body are present.
  EXPECT_THAT(info.content(), HasSubstr("test body"));

  // Make sure that URL of the content is present.
  EXPECT_THAT(info.ContentLocations(), Contains(EndsWith("/nostore.html")));
}

// TODO(crbug.com/40470937): These fail on Android under some circumstances.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_ViewedMHTMLContainsNoStoreContent \
  DISABLED_ViewedMHTMLContainsNoStoreContent
#else
#define MAYBE_ViewedMHTMLContainsNoStoreContent \
  ViewedMHTMLContainsNoStoreContent
#endif

IN_PROC_BROWSER_TEST_P(MHTMLGenerationTest,
                       MAYBE_ViewedMHTMLContainsNoStoreContent) {
  // Generate MHTML.
  base::FilePath path(temp_dir_.GetPath());
  path = path.Append(FILE_PATH_LITERAL("test.mht"));
  MHTMLGenerationParams params(path);

  CompareOptions options;
  options.expected_number_of_frames = 2;
  // We should see both frames.
  options.expected_substrings = {
      "Main Frame, normal headers.",
      "Cache-Control: no-store test body",
  };
  TestOriginalVsSavedPage(
      embedded_test_server()->GetURL("/page_with_nostore_iframe.html"), params,
      options);
}

// Test suite that allows testing --site-per-process against cross-site frames.
// See http://dev.chromium.org/developers/design-documents/site-isolation.
class MHTMLGenerationSitePerProcessTest : public MHTMLGenerationTest {
 public:
  MHTMLGenerationSitePerProcessTest() = default;

  MHTMLGenerationSitePerProcessTest(const MHTMLGenerationSitePerProcessTest&) =
      delete;
  MHTMLGenerationSitePerProcessTest& operator=(
      const MHTMLGenerationSitePerProcessTest&) = delete;

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
};

// Test for crbug.com/538766.
IN_PROC_BROWSER_TEST_P(MHTMLGenerationSitePerProcessTest, GenerateMHTML) {
  base::FilePath path(temp_dir_.GetPath());
  path = path.Append(FILE_PATH_LITERAL("test.mht"));

  GURL url(embedded_test_server()->GetURL(
      "a.com", "/frame_tree/page_with_one_frame.html"));
  MHTMLFileInfo info = GenerateMHTML(path, url);

  // Make sure the contents of both frames are present.
  EXPECT_THAT(info.content(), HasSubstr("This page has one cross-site iframe"));
  EXPECT_THAT(info.content(),
              HasSubstr("This page has no title"));  // From title1.html.

  // Make sure that URLs of both frames are present
  // (note that these are single-line regexes).
  EXPECT_THAT(
      info.ContentLocations(),
      testing::IsSupersetOf({EndsWith("/frame_tree/page_with_one_frame.html"),
                             EndsWith("/title1.html")}));
}

IN_PROC_BROWSER_TEST_P(MHTMLGenerationTest, RemovePopupOverlay) {
  base::FilePath path(temp_dir_.GetPath());
  path = path.Append(FILE_PATH_LITERAL("test.mht"));

  GURL url(embedded_test_server()->GetURL("/popup.html"));

  MHTMLGenerationParams params(path);
  params.remove_popup_overlay = true;

  MHTMLFileInfo info = GenerateMHTML(params, url);

  // Make sure the overlay is removed.
  EXPECT_THAT(info.content(), Not(HasSubstr("class=3D\"overlay")));
  EXPECT_THAT(info.content(), Not(HasSubstr("class=3D\"modal")));
}

IN_PROC_BROWSER_TEST_P(MHTMLGenerationTest, GenerateMHTMLWithExtraData) {
  const char kFakeSignalData1[] = "FakeSignalData1";
  const char kFakeSignalData2[] = "OtherMockDataForSignals";
  const char kFakeContentType[] = "text/plain";
  const char kFakeContentLocation[] =
      "cid:signal-data-62691-645341c4-62b3-478e-a8c5-e0dfccc3ca02@mhtml.blink";
  base::FilePath path(temp_dir_.GetPath());
  path = path.Append(FILE_PATH_LITERAL("test.mht"));
  GURL url(embedded_test_server()->GetURL("/page_with_image.html"));

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
  MHTMLFileInfo info = GenerateMHTML(path, url);

  EXPECT_TRUE(has_mhtml_callback_run());


  // Make sure that both extra data parts made it into the mhtml.
  EXPECT_THAT(info.content(), HasSubstr(kFakeSignalData1));
  EXPECT_THAT(info.content(), HasSubstr(kFakeSignalData2));
}

IN_PROC_BROWSER_TEST_P(MHTMLGenerationTest, GenerateMHTMLWithMultipleFrames) {
  base::FilePath path(temp_dir_.GetPath());
  path = path.Append(FILE_PATH_LITERAL("test.mht"));

  MHTMLGenerationParams params(path);
  CompareOptions options;
  options.expected_number_of_frames = 11;
  CompareResult result = TestOriginalVsSavedPage(
      embedded_test_server()->GetURL("/page_with_multiple_iframes.html"),
      params, options);

  EXPECT_EQ(result.original_info, result.saved_info);

  // Expect all frames in the .html are included in the generated file.
  EXPECT_THAT(
      result.file.ContentLocations(),
      testing::IsSupersetOf({EndsWith("/page_with_image.html"),
                             EndsWith("/page_with_popup.html"),
                             EndsWith("/page_with_frameset.html"),
                             EndsWith("/page_with_allowfullscreen_frame.html"),
                             EndsWith("/page_with_iframe_and_link.html")}));
}

IN_PROC_BROWSER_TEST_P(MHTMLGenerationImprovedTest, CustomElement) {
  MHTMLGenerationParams params(
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("test.mht")));

  CompareOptions options;
  options.expected_number_of_frames = 1;
  options.expected_substrings = {
      // If this isn't show, the custom element is either not created, or
      // not defined through customElements.define.
      "Inside Custom Element",
  };
  options.forbidden_substrings = {
      "Hidden with adopted stylesheet on shadowRoot",
      // TODO(crbug.com/363289333): Fix and uncomment.
      // "Hidden because undefined-test-element is not defined.",
      "Hidden with adopted stylesheet on document",
      "Hidden with stylesheet on shadowRoot",
  };
  CompareResult result = TestOriginalVsSavedPage(
      embedded_test_server()->GetURL("/mhtml/custom_element_defined.html"),
      params, options);
  EXPECT_EQ(result.original_info, result.saved_info);
}

IN_PROC_BROWSER_TEST_P(MHTMLGenerationImprovedTest, CustomElementInFrame) {
  MHTMLGenerationParams params(
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("test.mht")));

  // Note this has all the same string assertions from
  // `GenerateMHTMLWithCustomElement`.
  CompareOptions options;
  options.expected_number_of_frames = 2;
  options.expected_substrings = {
      "Inside Custom Element",
  };
  options.forbidden_substrings = {
      "Hidden with adopted stylesheet on shadowRoot",
      // TODO(crbug.com/363289333): Fix and uncomment.
      // "Hidden because undefined-test-element is not defined.",
      "Hidden with adopted stylesheet on document",
      "Hidden with stylesheet on shadowRoot",

      // Verify <test-element> isn't accidentally defined outside of the
      // frame.
      "Hidden because not defined outside of frame.",
  };
  CompareResult result = TestOriginalVsSavedPage(
      embedded_test_server()->GetURL(
          "/mhtml/custom_element_defined_in_frame.html"),
      params, options);
  EXPECT_EQ(result.original_info, result.saved_info);
}

IN_PROC_BROWSER_TEST_P(MHTMLGenerationImprovedTest, Styles) {
  MHTMLGenerationParams params(
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("test.mht")));

  CompareOptions options;
  options.expected_number_of_frames = 1;
  options.expected_substrings = {"hidden1", "hidden4"};
  options.forbidden_substrings = {"hidden2", "hidden3"};
  CompareResult result = TestOriginalVsSavedPage(
      embedded_test_server()->GetURL("/mhtml/styles.html"), params, options);
  EXPECT_EQ(result.original_info, result.saved_info);
}

// We instantiate the MHTML Generation Tests both using and not using the
// GenerateMHTMLWithResults callback.
INSTANTIATE_TEST_SUITE_P(MHTMLGenerationTest,
                         MHTMLGenerationTest,
                         testing::Bool());
INSTANTIATE_TEST_SUITE_P(MHTMLGenerationSitePerProcessTest,
                         MHTMLGenerationSitePerProcessTest,
                         testing::Bool());
INSTANTIATE_TEST_SUITE_P(MHTMLGenerationImprovedTest,
                         MHTMLGenerationImprovedTest,
                         testing::Bool());

}  // namespace
}  // namespace content
