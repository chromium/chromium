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
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/test/metrics/histogram_tester.h"
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
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/frame/find_in_page.mojom.h"

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

const char kTestData[] =
    "Sample Text to write on a generated MHTML "
    "file for tests to validate whether the implementation is able to access "
    "and write to the file.";

class MockWriterBase : public mojom::MhtmlFileWriter {
 public:
  MockWriterBase() = default;
  ~MockWriterBase() override = default;

  void BindReceiver(mojo::ScopedInterfaceEndpointHandle handle) {
    receiver_.Bind(mojo::PendingAssociatedReceiver<mojom::MhtmlFileWriter>(
        std::move(handle)));
  }

 protected:
  void SendResponse(SerializeAsMHTMLCallback callback) {
    std::vector<std::string> dummy_digests;
    base::TimeDelta dummy_time_delta = base::TimeDelta::FromMilliseconds(100);
    std::move(callback).Run(mojom::MhtmlSaveStatus::kSuccess, dummy_digests,
                            dummy_time_delta);
  }

  void WriteDataToDestinationFile(base::File& destination_file) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    destination_file.WriteAtCurrentPos(kTestData, strlen(kTestData));
    destination_file.Close();
  }

  void WriteDataToProducerPipe(
      mojo::ScopedDataPipeProducerHandle producer_pipe) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    uint32_t size = strlen(kTestData);
    producer_pipe->WriteData(kTestData, &size, MOJO_WRITE_DATA_FLAG_NONE);
    producer_pipe.reset();
  }

  mojo::AssociatedReceiver<mojom::MhtmlFileWriter> receiver_{this};

 private:
  DISALLOW_COPY_AND_ASSIGN(MockWriterBase);
};

// This Mock injects our overwritten interface, running the callback
// SerializeAsMHTMLResponse and immediately disconnecting the message pipe.
class RespondAndDisconnectMockWriter
    : public MockWriterBase,
      public base::RefCountedThreadSafe<RespondAndDisconnectMockWriter> {
 public:
  RespondAndDisconnectMockWriter() {}

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
    base::PostTask(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&RespondAndDisconnectMockWriter::TaskZ,
                       scoped_refptr<RespondAndDisconnectMockWriter>(this)));
  }

  void TaskZ() { receiver_.reset(); }

 private:
  friend base::RefCountedThreadSafe<RespondAndDisconnectMockWriter>;

  ~RespondAndDisconnectMockWriter() override = default;

  DISALLOW_COPY_AND_ASSIGN(RespondAndDisconnectMockWriter);
};

}  // namespace

class MHTMLGenerationTest
    : public ContentBrowserTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  MHTMLGenerationTest()
      : has_mhtml_callback_run_(false),
        file_size_(0),
        file_digest_(base::nullopt),
        well_formedness_check_(true) {}

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
            ->GetMainFrame()
            ->GetRemoteAssociatedInterfaces();
    remote_interfaces->OverrideBinderForTesting(
        mojom::MhtmlFileWriter::Name_,
        base::BindRepeating(&MockWriterBase::BindReceiver,
                            base::Unretained(mock_writer)));
  }

  void GenerateMHTML(base::FilePath& path, const GURL& url) {
    MHTMLGenerationParams params(path);
    GenerateMHTML(params, url);
  }

  void GenerateMHTML(MHTMLGenerationParams& params, const GURL& url) {
    EXPECT_TRUE(NavigateToURL(shell(), url));
    GenerateMHTMLForCurrentPage(params);
  }

  void GenerateMHTMLForCurrentPage(MHTMLGenerationParams& params) {
    base::RunLoop run_loop;
    histogram_tester_.reset(new base::HistogramTester());

    bool use_result_callback;
    std::tie(params.compute_contents_hash, use_result_callback) = GetParam();

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

    ASSERT_TRUE(has_mhtml_callback_run())
        << "Unexpected error generating MHTML file";

    // TODO(crbug.com/997408): Add tests which will let MHTMLGeneration manager
    // fail during file write operation. This will allow us to actually test if
    // we receive a bogus hash instead of a base::nullopt.
    bool generation_failed = file_size() == -1;
    if (use_result_callback && !generation_failed &&
        params.compute_contents_hash) {
      // File contents write was successful, verify compute contents hash.
      TestComputeContentsHash(params.file_path);
    } else {
      // expect that no hash was produced
      EXPECT_EQ(base::nullopt, file_digest());
    }

    // Skip well formedness check if explicitly disabled or there was a
    // generation error.
    if (!well_formedness_check_ || generation_failed)
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

  void TwoStepSyncTestFor(const TaskOrder order);

  int64_t ReadFileSizeFromDisk(base::FilePath path) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    int64_t file_size;
    if (!base::GetFileSize(path, &file_size)) return -1;
    return file_size;
  }

  void TestOriginalVsSavedPage(
      const GURL& url,
      MHTMLGenerationParams params,
      int expected_number_of_frames,
      const std::vector<std::string>& expected_substrings,
      const std::vector<std::string>& forbidden_substrings_in_saved_page,
      bool skip_verification_of_original_page = false) {
    // Navigate to the test page and verify if test expectations
    // are met (this is mostly a sanity check - a failure to meet
    // expectations would probably mean that there is a test bug
    // (i.e. that we got called with wrong expected_foo argument).
    EXPECT_TRUE(NavigateToURL(shell(), url));
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
    EXPECT_TRUE(
        NavigateToURL(shell(), net::FilePathToFileURL(params.file_path)));
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
  base::Optional<std::string> file_digest() const { return file_digest_; }
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

  bool has_mhtml_callback_run_;
  int64_t file_size_;
  base::Optional<std::string> file_digest_;
  bool well_formedness_check_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

// Tests that generating a MHTML does create contents.
// Note that the actual content of the file is not tested, the purpose of this
// test is to ensure we were successful in creating the MHTML data from the
// renderer.
IN_PROC_BROWSER_TEST_P(MHTMLGenerationTest, GenerateMHTML) {
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
      static_cast<int>(mojom::MhtmlSaveStatus::kSuccess), 1);
}

// Regression test for the crash/race from https://crbug.com/612098.
//
// TODO(crbug.com/959435): Flaky on Android.
#if defined(OS_ANDROID)
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

// TODO(crbug.com/672313): Flaky on Windows.
#if defined(OS_WIN)
#define MAYBE_InvalidPath DISABLED_InvalidPath
#else
#define MAYBE_InvalidPath InvalidPath
#endif
IN_PROC_BROWSER_TEST_P(MHTMLGenerationTest, MAYBE_InvalidPath) {
  base::FilePath path(FILE_PATH_LITERAL("/invalid/file/path"));

  GenerateMHTML(path, embedded_test_server()->GetURL("/page_with_image.html"));

  EXPECT_EQ(file_size(), -1);  // Expecting that the callback reported failure.

  // Checks that the final status reported to UMA is correct.
  histogram_tester()->ExpectUniqueSample(
      "PageSerialization.MhtmlGeneration.FinalSaveStatus",
      static_cast<int>(mojom::MhtmlSaveStatus::kFileCreationError), 1);
}

// Tests that MHTML generated using the default 'quoted-printable' encoding does
// not contain the 'binary' Content-Transfer-Encoding header, and generates
// base64 encoding for the image part.
IN_PROC_BROWSER_TEST_P(MHTMLGenerationTest, GenerateNonBinaryMHTMLWithImage) {
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
IN_PROC_BROWSER_TEST_P(MHTMLGenerationTest, GenerateBinaryMHTMLWithImage) {
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

IN_PROC_BROWSER_TEST_P(MHTMLGenerationTest, GenerateMHTMLIgnoreNoStore) {
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

// TODO(crbug.com/615291): These fail on Android under some circumstances.
#if defined(OS_ANDROID)
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

  // We should see both frames.
  std::vector<std::string> expectations = {
      "Main Frame, normal headers.", "Cache-Control: no-store test body",
  };
  std::vector<std::string> forbidden;
  TestOriginalVsSavedPage(
      embedded_test_server()->GetURL("/page_with_nostore_iframe.html"), params,
      2 /* expected number of frames */, expectations, forbidden);
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
IN_PROC_BROWSER_TEST_P(MHTMLGenerationSitePerProcessTest, GenerateMHTML) {
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

IN_PROC_BROWSER_TEST_P(MHTMLGenerationTest, RemovePopupOverlay) {
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
  GenerateMHTML(path, url);

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

IN_PROC_BROWSER_TEST_P(MHTMLGenerationTest, GenerateMHTMLWithMultipleFrames) {
  base::FilePath path(temp_dir_.GetPath());
  path = path.Append(FILE_PATH_LITERAL("test.mht"));

  const std::string kContentURLs[] = {
      "Content-Location:.*/page_with_image.html",
      "Content-Location:.*/page_with_popup.html",
      "Content-Location:.*/page_with_frameset.html",
      "Content-Location:.*/page_with_allowfullscreen_frame.html",
      "Content-Location:.*/page_with_iframe_and_link.html"};

  MHTMLGenerationParams params(path);
  TestOriginalVsSavedPage(
      embedded_test_server()->GetURL("/page_with_multiple_iframes.html"),
      params, 11 /* expected number of frames */, std::vector<std::string>(),
      std::vector<std::string>());

  // Test whether generation was successful.
  EXPECT_GT(file_size(), 0);  // Verify the size reported by the callback.
  EXPECT_GT(ReadFileSizeFromDisk(path), 100);  // Verify the actual file size.

  std::string mhtml;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::ReadFileToString(path, &mhtml));
  }

  // Expect all frames in the .html are included in the generated file.
  for (const auto& regex : kContentURLs)
    EXPECT_THAT(mhtml, ContainsRegex(regex));
}

// Tests for the synchronization logic that waits for both the Mojo
// response and the data pipe closure to consider a frame serialization done.
// This is only relevant when a Mojo data pipe is being used (hash computation
// case) and is skipped if writing directly to file.
namespace {
class OrderedTaskMockWriter : public MockWriterBase {
 public:
  explicit OrderedTaskMockWriter(MHTMLGenerationTest::TaskOrder order)
      : order_(order) {}

  void SerializeAsMHTML(mojom::SerializeAsMHTMLParamsPtr params,
                        SerializeAsMHTMLCallback callback) override {
    DCHECK(params->output_handle->is_producer_handle());
    DCHECK(params->output_handle->get_producer_handle()->is_valid());

    switch (order_) {
      case MHTMLGenerationTest::TaskOrder::RespondThenWrite:
        delayed_callback_ = base::BindOnce(
            &OrderedTaskMockWriter::WriteDataToProducerPipe,
            base::Unretained(this),
            std::move(params->output_handle->get_producer_handle()));
        SendResponse(std::move(callback));
        PostClosure();
        break;
      case MHTMLGenerationTest::TaskOrder::WriteThenRespond:
        delayed_callback_ =
            base::BindOnce(&OrderedTaskMockWriter::SendResponse,
                           base::Unretained(this), std::move(callback));
        WriteDataToProducerPipe(
            std::move(params->output_handle->get_producer_handle()));
        // For this case, we must post to the download sequence first to
        // ensure we run the closure after the write operation completes.
        download::GetDownloadTaskRunner()->PostTask(
            FROM_HERE, base::BindOnce(&OrderedTaskMockWriter::PostClosure,
                                      base::Unretained(this)));
        break;
    }
  }

  // Posts the quit closure to the UI thread to unblock the serialization Job
  // after receiving the first task complete notification.
  void PostClosure() {
    base::PostTask(FROM_HERE, {BrowserThread::UI},
                   std::move(first_run_loop_closure_));
  }

  base::OnceClosure first_run_loop_closure_;
  base::OnceClosure delayed_callback_;

 private:
  MHTMLGenerationTest::TaskOrder order_;

  DISALLOW_COPY_AND_ASSIGN(OrderedTaskMockWriter);
};
}  // namespace

void MHTMLGenerationTest::TwoStepSyncTestFor(
    const MHTMLGenerationTest::TaskOrder order) {
  OrderedTaskMockWriter mock_writer(order);

  base::FilePath path(temp_dir_.GetPath());
  path = path.Append(FILE_PATH_LITERAL("test.mht"));

  MHTMLGenerationParams params(path);

  OverrideInterface(&mock_writer);

  base::RunLoop first_run_loop;
  base::RunLoop second_run_loop;

  params.compute_contents_hash = true;
  mock_writer.first_run_loop_closure_ = first_run_loop.QuitWhenIdleClosure();

  shell()->web_contents()->GenerateMHTML(
      params,
      base::BindOnce(&MHTMLGenerationTest::MHTMLGenerated,
                     base::Unretained(this), second_run_loop.QuitClosure()));

  // Run serialization pipeline until stalled.
  first_run_loop.Run();
  ASSERT_FALSE(has_mhtml_callback_run())
      << "MHTML generation complete but should be waiting on operation.";

  // Run stalled task and block until MHTML generation completes.
  DCHECK(mock_writer.delayed_callback_);
  std::move(mock_writer.delayed_callback_).Run();
  second_run_loop.Run();

  ASSERT_TRUE(has_mhtml_callback_run())
      << "MHTML generation has not been complete despite unblocking the Job.";

  // Verify the file has some contents written to it.
  EXPECT_GT(ReadFileSizeFromDisk(path), 100);
  // Verify the reported file size matches the file written to disk.
  EXPECT_EQ(ReadFileSizeFromDisk(path), file_size_);
}

// These tests do not depend on the parameter declared by the
// MHTMLGenerationTest test suite, so we only want to run them once.
IN_PROC_BROWSER_TEST_F(MHTMLGenerationTest, GenerateMHTMLButDelayWrite) {
  TwoStepSyncTestFor(TaskOrder::RespondThenWrite);
}

IN_PROC_BROWSER_TEST_F(MHTMLGenerationTest, GenerateMHTMLButDelayResponse) {
  TwoStepSyncTestFor(TaskOrder::WriteThenRespond);
}

// We instantiate the MHTML Generation Tests with a matrix of boolean values
// because we want to test both compute_contents_hash enabled, and using
// GenerateMHTMLWithResults callback independently.
INSTANTIATE_TEST_SUITE_P(MHTMLGenerationTest,
                         MHTMLGenerationTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

}  // namespace content
