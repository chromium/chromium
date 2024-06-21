// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/simple_url_loader.h"

#include <stdint.h>

#include <list>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "base/base_paths.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/power_monitor_test.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/mock_network_change_notifier.h"
#include "net/base/network_change_notifier.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/url_request.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader_stream_consumer.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/data_pipe_getter.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/fake_test_cert_verifier_params_factory.h"
#include "services/network/test/test_network_context_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/radio_utils.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace network {
namespace {

using ::testing::ElementsAre;

// Server path that returns a response containing as many a's as are specified
// in the query part of the URL.
const char kResponseSizePath[] = "/response-size";

// Server path that returns a gzip response with a non-gzipped body.
const char kInvalidGzipPath[] = "/invalid-gzip";

// Server path that returns truncated response (Content-Length less than body
// size).
const char kTruncatedBodyPath[] = "/truncated-body";
// The body of the truncated response (After truncation).
const char kTruncatedBody[] = "Truncated Body";

// Server path returns a 5xx error once, then returns the request body.
const char kFailOnceThenEchoBody[] = "/fail-once-then-echo-body";

// Used in string upload tests.
const char kShortUploadBody[] =
    "Though this upload be but little, it is fierce.";

// Standard value used on requests / responses.
const char kExpectedResponse[] = "Expected Response";

const int64_t kExpectedResponseSize = strlen(kExpectedResponse);

// Returns a string longer than
// SimpleURLLoader::kMaxUploadStringAsStringLength, to test the path where
// strings are streamed to the URLLoader.
std::string GetLongUploadBody(
    size_t size = SimpleURLLoader::kMaxUploadStringSizeToCopy) {
  std::string long_string;
  long_string.reserve(size);
  while (long_string.length() <= size) {
    long_string.append(kShortUploadBody);
  }
  return long_string;
}

// Class to make it easier to start a SimpleURLLoader, wait for it to complete,
// and check the result.
class SimpleLoaderTestHelper : public SimpleURLLoaderStreamConsumer {
 public:
  // What the response should be downloaded to. Running all tests for all types
  // is more than strictly needed, but simplest just to cover all cases.
  enum class DownloadType {
    TO_STRING,
    TO_FILE,
    TO_TEMP_FILE,
    HEADERS_ONLY,
    AS_STREAM
  };

  explicit SimpleLoaderTestHelper(
      std::unique_ptr<network::ResourceRequest> resource_request,
      DownloadType download_type)
      : download_type_(download_type),
        simple_url_loader_(
            SimpleURLLoader::Create(std::move(resource_request),
                                    TRAFFIC_ANNOTATION_FOR_TESTS)) {
    // Create a desistination directory, if downloading to a file.
    if (download_type_ == DownloadType::TO_FILE) {
      base::ScopedAllowBlockingForTesting allow_blocking;
      EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
      dest_path_ = temp_dir_.GetPath().AppendASCII("foo");
    }
  }

  SimpleLoaderTestHelper(const SimpleLoaderTestHelper&) = delete;
  SimpleLoaderTestHelper& operator=(const SimpleLoaderTestHelper&) = delete;

  ~SimpleLoaderTestHelper() override {
    base::ScopedAllowBlockingForTesting allow_blocking;
    if (temp_dir_.IsValid())
      EXPECT_TRUE(temp_dir_.Delete());
    if (!on_destruction_callback_.is_null())
      std::move(on_destruction_callback_).Run();
  }

  // Returns true if the DownloadType indicates the response body is being saved
  // to disk. Writing to disk can sometimes be slow on the bots.
  static bool IsDownloadTypeToFile(DownloadType type) {
    return type == DownloadType::TO_FILE || type == DownloadType::TO_TEMP_FILE;
  }

  // File path that will be written to.
  const base::FilePath& dest_path() const {
    DCHECK_EQ(DownloadType::TO_FILE, download_type_);
    return dest_path_;
  }

  // Starts a SimpleURLLoader using the method corresponding to the
  // DownloadType, but does not wait for it to complete. The default
  // |max_body_size| of -1 means don't use a max body size (Use
  // DownloadToStringOfUnboundedSizeUntilCrashAndDie for string downloads, and
  // don't specify a size for other types of downloads).
  void StartSimpleLoader(network::mojom::URLLoaderFactory* url_loader_factory,
                         int64_t max_body_size = -1) {
    EXPECT_FALSE(done_);
    switch (download_type_) {
      case DownloadType::TO_STRING:
        if (max_body_size < 0) {
          simple_url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
              url_loader_factory,
              base::BindOnce(&SimpleLoaderTestHelper::DownloadedToString,
                             base::Unretained(this)));
        } else {
          simple_url_loader_->DownloadToString(
              url_loader_factory,
              base::BindOnce(&SimpleLoaderTestHelper::DownloadedToString,
                             base::Unretained(this)),
              max_body_size);
        }
        break;
      case DownloadType::TO_FILE:
        if (max_body_size < 0) {
          simple_url_loader_->DownloadToFile(
              url_loader_factory,
              base::BindOnce(&SimpleLoaderTestHelper::DownloadedToFile,
                             base::Unretained(this)),
              dest_path_);
        } else {
          simple_url_loader_->DownloadToFile(
              url_loader_factory,
              base::BindOnce(&SimpleLoaderTestHelper::DownloadedToFile,
                             base::Unretained(this)),
              dest_path_, max_body_size);
        }
        break;
      case DownloadType::TO_TEMP_FILE:
        if (max_body_size < 0) {
          simple_url_loader_->DownloadToTempFile(
              url_loader_factory,
              base::BindOnce(&SimpleLoaderTestHelper::DownloadedToFile,
                             base::Unretained(this)));
        } else {
          simple_url_loader_->DownloadToTempFile(
              url_loader_factory,
              base::BindOnce(&SimpleLoaderTestHelper::DownloadedToFile,
                             base::Unretained(this)),
              max_body_size);
        }
        break;
      case DownloadType::HEADERS_ONLY:
        simple_url_loader_->DownloadHeadersOnly(
            url_loader_factory,
            base::BindOnce(&SimpleLoaderTestHelper::DownloadedHeadersOnly,
                           base::Unretained(this)));
        break;
      case DownloadType::AS_STREAM:
        // Downloading to stream doesn't support a max body size.
        DCHECK_LT(max_body_size, 0);
        simple_url_loader_->DownloadAsStream(url_loader_factory, this);
        break;
    }
  }

  // Starts the SimpleURLLoader waits for completion.
  void StartSimpleLoaderAndWait(
      network::mojom::URLLoaderFactory* url_loader_factory,
      int64_t max_body_size = -1) {
    StartSimpleLoader(url_loader_factory, max_body_size);
    Wait();
  }

  // Waits until the request is completed. Automatically called by
  // StartSimpleLoaderAndWait, but exposed so some tests can start the
  // SimpleURLLoader directly.
  void Wait() {
    const base::test::ScopedRunLoopTimeout run_timeout(
        // Some of the bots run tests quite slowly, and the default timeout is
        // too short for them for some of the heavier weight tests.
        // See https://crbug.com/1046745 and https://crbug.com/1035127.
        FROM_HERE, TestTimeouts::action_max_timeout());
    run_loop_.Run();
  }

  // Sets whether a file should still exists on download-to-file errors.
  // Defaults to false.
  void set_expect_path_exists_on_error(bool expect_path_exists_on_error) {
    EXPECT_EQ(DownloadType::TO_FILE, download_type_);
    expect_path_exists_on_error_ = expect_path_exists_on_error;
  }

  // Sets whether reading is resumed asynchronously when downloading as a
  // stream. Defaults to false.
  void set_download_to_stream_async_resume(
      bool download_to_stream_async_resume) {
    download_to_stream_async_resume_ = download_to_stream_async_resume;
  }

  // Sets whether the resume-reading closure should be captured and later
  // available in TakeCapturedStreamResume()
  void set_download_to_stream_capture_resume(
      bool download_to_stream_capture_resume) {
    download_to_stream_capture_resume_ = download_to_stream_capture_resume;
  }

  base::OnceClosure TakeCapturedStreamResume() {
    return std::move(captured_stream_resume_);
  }

  // Sets whether the helper should destroy the SimpleURLLoader in
  // OnDataReceived.
  void set_download_to_stream_destroy_on_data_received(
      bool download_to_stream_destroy_on_data_received) {
    download_to_stream_destroy_on_data_received_ =
        download_to_stream_destroy_on_data_received;
  }

  // Sets whether retrying is done asynchronously when downloading as a stream.
  // Defaults to false.
  void set_download_to_stream_async_retry(bool download_to_stream_async_retry) {
    download_to_stream_async_retry_ = download_to_stream_async_retry;
  }

  // Sets whether the helper should destroy the SimpleURLLoader in OnRetry.
  void set_download_to_stream_destroy_on_retry(
      bool download_to_stream_destroy_on_retry) {
    download_to_stream_destroy_on_retry_ = download_to_stream_destroy_on_retry;
  }

  // Sets whether the SimpleURLLoader should be destroyed when invoking the
  // completion callback. When enabled, it will be destroyed before touching the
  // completion data, to make sure it's still available after the destruction of
  // the SimpleURLLoader.
  void set_destroy_loader_on_complete(bool destroy_loader_on_complete) {
    destroy_loader_on_complete_ = destroy_loader_on_complete;
  }

  // Received response body, if any. Returns nullptr if no body was received
  // (Which is different from a 0-length body). For DownloadType::TO_STRING,
  // this is just the value passed to the callback. For DownloadType::TO_FILE,
  // it is nullptr if an empty FilePath was passed to the callback, or the
  // contents of the file, otherwise.
  const std::string* response_body() const {
    EXPECT_TRUE(done_);
    return response_body_.get();
  }

  // Returns true if the callback has been invoked.
  bool done() const { return done_; }

  SimpleURLLoader* simple_url_loader() { return simple_url_loader_.get(); }

  // Destroys the SimpleURLLoader. Useful in tests where the SimpleURLLoader is
  // destroyed while it still has an open file, as the file needs to be closed
  // before the SimpleLoaderTestHelper's destructor tries to clean up the temp
  // directory.
  void DestroySimpleURLLoader() { simple_url_loader_.reset(); }

  void SetAllowPartialResults(bool allow_partial_results) {
    simple_url_loader_->SetAllowPartialResults(allow_partial_results);
    allow_http_error_results_ = allow_partial_results;
  }

  // Returns the HTTP response code. Fails if there isn't one.
  int GetResponseCode() const {
    EXPECT_TRUE(done_);
    if (!simple_url_loader_->ResponseInfo()) {
      ADD_FAILURE() << "No response info.";
      return -1;
    }
    if (!simple_url_loader_->ResponseInfo()->headers) {
      ADD_FAILURE() << "No response headers.";
      return -1;
    }
    return simple_url_loader_->ResponseInfo()->headers->response_code();
  }

  // Returns the number of retries. Number of retries are only exposed when
  // downloading as a stream.
  int download_as_stream_retries() const {
    DCHECK_EQ(DownloadType::AS_STREAM, download_type_);
    return download_as_stream_retries_;
  }

 private:
  void DownloadedToString(std::unique_ptr<std::string> response_body) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    EXPECT_FALSE(done_);
    EXPECT_EQ(DownloadType::TO_STRING, download_type_);
    EXPECT_FALSE(response_body_);

    if (destroy_loader_on_complete_)
      simple_url_loader_.reset();

    response_body_ = std::move(response_body);

    done_ = true;
    run_loop_.Quit();
  }

  void DownloadedToFile(base::FilePath file_path) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    EXPECT_FALSE(done_);
    EXPECT_TRUE(download_type_ == DownloadType::TO_FILE ||
                download_type_ == DownloadType::TO_TEMP_FILE);
    EXPECT_FALSE(response_body_);

    if (destroy_loader_on_complete_)
      simple_url_loader_.reset();

    base::ScopedAllowBlockingForTesting allow_blocking;

    if (!file_path.empty()) {
      EXPECT_TRUE(base::PathExists(file_path));
      response_body_ = std::make_unique<std::string>();
      EXPECT_TRUE(base::ReadFileToString(file_path, response_body_.get()));
    }

    // Can do some additional checks in the TO_FILE case. Unfortunately, in the
    // temp file case, can't check that temp files were cleaned up, since it's
    // a shared directory.
    if (download_type_ == DownloadType::TO_FILE) {
      // Make sure the destination file exists if |file_path| is non-empty, or
      // the file is expected to exist on error.
      EXPECT_EQ(!file_path.empty() || expect_path_exists_on_error_,
                base::PathExists(dest_path_));

      if (!file_path.empty())
        EXPECT_EQ(dest_path_, file_path);
    }

    // Clean up file, so tests don't leave around files in the temp directory.
    // Only matters in the TO_TEMP_FILE case.
    if (!file_path.empty())
      base::DeleteFile(file_path);

    done_ = true;
    run_loop_.Quit();
  }

  void DownloadedHeadersOnly(scoped_refptr<net::HttpResponseHeaders> headers) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    EXPECT_FALSE(done_);
    EXPECT_EQ(DownloadType::HEADERS_ONLY, download_type_);
    EXPECT_FALSE(response_body_);

    if (destroy_loader_on_complete_)
      simple_url_loader_.reset();

    done_ = true;
    run_loop_.Quit();
  }

  // SimpleURLLoaderStreamConsumer implementation:

  void OnDataReceived(std::string_view string_piece,
                      base::OnceClosure resume) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    EXPECT_FALSE(done_);
    EXPECT_EQ(DownloadType::AS_STREAM, download_type_);

    // If destroying the stream on data received, destroy the stream before
    // reading the data, to make sure that works.
    if (download_to_stream_destroy_on_data_received_) {
      simple_url_loader_.reset();
      done_ = true;
    }

    download_as_stream_response_body_.append(std::string(string_piece));

    if (download_to_stream_destroy_on_data_received_) {
      run_loop_.Quit();
      return;
    }

    if (download_to_stream_capture_resume_) {
      if (captured_stream_resume_) {
        std::move(captured_stream_resume_).Run();
      }
      captured_stream_resume_ = std::move(resume);
      return;
    }

    if (download_to_stream_async_resume_) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(resume));
      return;
    }
    std::move(resume).Run();
  }

  void OnComplete(bool success) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    EXPECT_FALSE(done_);
    EXPECT_EQ(DownloadType::AS_STREAM, download_type_);
    EXPECT_FALSE(response_body_);

    // If headers weren't received for the final request, the response body
    // should be empty.
    if (!simple_url_loader_->ResponseInfo())
      DCHECK(download_as_stream_response_body_.empty());

    // This makes behavior of downloading a response to a stream more closely
    // resemble other DownloadTypes, so most test logic can be shared.
    if (success ||
        (allow_http_error_results_ && simple_url_loader_->ResponseInfo())) {
      response_body_ =
          std::make_unique<std::string>(download_as_stream_response_body_);
    }

    if (destroy_loader_on_complete_)
      simple_url_loader_.reset();

    done_ = true;
    run_loop_.Quit();
  }

  void OnRetry(base::OnceClosure start_retry) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    EXPECT_FALSE(done_);
    EXPECT_EQ(DownloadType::AS_STREAM, download_type_);

    ++download_as_stream_retries_;
    download_as_stream_response_body_.clear();

    if (download_to_stream_destroy_on_retry_) {
      simple_url_loader_.reset();
      done_ = true;
      run_loop_.Quit();
      return;
    }

    if (download_to_stream_async_retry_) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(start_retry));
      return;
    }
    std::move(start_retry).Run();
  }

  DownloadType download_type_;
  bool done_ = false;

  bool expect_path_exists_on_error_ = false;

  std::unique_ptr<SimpleURLLoader> simple_url_loader_;
  base::RunLoop run_loop_;

  // Response body, regardless of DownloadType. Only populated on completion.
  // Null on error.
  std::unique_ptr<std::string> response_body_;

  base::OnceClosure on_destruction_callback_;

  // Response data when downloading as stream:
  std::string download_as_stream_response_body_;
  int download_as_stream_retries_ = 0;

  bool download_to_stream_async_resume_ = false;
  bool download_to_stream_destroy_on_data_received_ = false;
  bool download_to_stream_async_retry_ = false;
  bool download_to_stream_destroy_on_retry_ = false;
  bool download_to_stream_capture_resume_ = false;
  base::OnceClosure captured_stream_resume_;

  bool destroy_loader_on_complete_ = false;

  bool allow_http_error_results_ = false;

  base::ScopedTempDir temp_dir_;
  base::FilePath dest_path_;

  SEQUENCE_CHECKER(sequence_checker_);
};

// Request handler for the embedded test server that returns a response body
// with the length indicated by the query string.
std::unique_ptr<net::test_server::HttpResponse> HandleResponseSize(
    const net::test_server::HttpRequest& request) {
  if (request.GetURL().path_piece() != kResponseSizePath)
    return nullptr;

  std::unique_ptr<net::test_server::BasicHttpResponse> response =
      std::make_unique<net::test_server::BasicHttpResponse>();

  uint32_t length;
  if (!base::StringToUint(request.GetURL().query(), &length)) {
    ADD_FAILURE() << "Invalid length: " << request.GetURL();
  } else {
    response->set_content(std::string(length, 'a'));
  }

  return std::move(response);
}

// Request handler for the embedded test server that returns a an invalid gzip
// response body. No body bytes will be read successfully.
std::unique_ptr<net::test_server::HttpResponse> HandleInvalidGzip(
    const net::test_server::HttpRequest& request) {
  if (request.GetURL().path_piece() != kInvalidGzipPath)
    return nullptr;

  std::unique_ptr<net::test_server::BasicHttpResponse> response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  response->AddCustomHeader("Content-Encoding", "gzip");
  response->set_content("Not gzipped");

  return std::move(response);
}

// Request handler for the embedded test server that returns a response with a
// truncated body. Consumer should see an error after reading some data.
std::unique_ptr<net::test_server::HttpResponse> HandleTruncatedBody(
    const net::test_server::HttpRequest& request) {
  if (request.GetURL().path_piece() != kTruncatedBodyPath)
    return nullptr;

  std::unique_ptr<net::test_server::RawHttpResponse> response =
      std::make_unique<net::test_server::RawHttpResponse>(
          base::StringPrintf("HTTP/1.1 200 OK\r\n"
                             "Content-Length: %" PRIuS "\r\n",
                             strlen(kTruncatedBody) + 4),
          kTruncatedBody);

  return std::move(response);
}

// Request handler for the embedded test server that returns a 5xx error once,
// and on future requests, has a response body matching the request body.
std::unique_ptr<net::test_server::HttpResponse> FailOnceThenEchoBody(
    bool* has_failed_request,
    const net::test_server::HttpRequest& request) {
  if (request.GetURL().path_piece() != kFailOnceThenEchoBody)
    return nullptr;

  if (!*has_failed_request) {
    EXPECT_FALSE(request.content.empty());
    *has_failed_request = true;
    std::unique_ptr<net::test_server::BasicHttpResponse> response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_INTERNAL_SERVER_ERROR);
    return response;
  }

  std::unique_ptr<net::test_server::BasicHttpResponse> response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_content(request.content);
  return std::move(response);
}

// Base class with shared setup logic.
class SimpleURLLoaderTestBase {
 public:
  SimpleURLLoaderTestBase()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {
    mojo::Remote<network::mojom::NetworkService> network_service_remote;
    network_service_ = network::NetworkService::Create(
        network_service_remote.BindNewPipeAndPassReceiver());
    network::mojom::NetworkContextParamsPtr context_params =
        network::mojom::NetworkContextParams::New();

    // Use a dummy CertVerifier that always passes cert verification, since
    // these unittests don't need to test CertVerifier behavior.
    context_params->cert_verifier_params =
        FakeTestCertVerifierParamsFactory::GetCertVerifierParams();

    network_service_remote->CreateNetworkContext(
        network_context_.BindNewPipeAndPassReceiver(),
        std::move(context_params));

    mojo::PendingReceiver<network::mojom::URLLoaderNetworkServiceObserver>
        default_observer_receiver;
    network::mojom::NetworkServiceParamsPtr network_service_params =
        network::mojom::NetworkServiceParams::New();
    network_service_params->default_observer =
        default_observer_receiver.InitWithNewPipeAndPassRemote();
    network_service_remote->SetParams(std::move(network_service_params));

    mojo::PendingRemote<network::mojom::NetworkContextClient>
        network_context_client_remote;
    network_context_client_ = std::make_unique<TestNetworkContextClient>(
        network_context_client_remote.InitWithNewPipeAndPassReceiver());
    network_context_->SetClient(std::move(network_context_client_remote));

    mojom::URLLoaderFactoryParamsPtr params =
        mojom::URLLoaderFactoryParams::New();
    params->process_id = mojom::kBrowserProcessId;
    params->is_orb_enabled = false;
    url::Origin origin = url::Origin::Create(test_server_.base_url());
    params->isolation_info =
        net::IsolationInfo::CreateForInternalRequest(origin);
    params->is_trusted = true;
    network_context_->CreateURLLoaderFactory(
        url_loader_factory_.BindNewPipeAndPassReceiver(), std::move(params));

    test_server_.AddDefaultHandlers(base::FilePath(FILE_PATH_LITERAL("")));
    test_server_.RegisterRequestHandler(
        base::BindRepeating(&HandleResponseSize));
    test_server_.RegisterRequestHandler(
        base::BindRepeating(&HandleInvalidGzip));
    test_server_.RegisterRequestHandler(
        base::BindRepeating(&HandleTruncatedBody));
    test_server_.RegisterRequestHandler(base::BindRepeating(
        &FailOnceThenEchoBody, base::Owned(new bool(false))));

    EXPECT_TRUE(test_server_.Start());

    // Can only create this after blocking calls.  Creating the network stack
    // has some, as does starting the test server.
    disallow_blocking_ = std::make_unique<base::ScopedDisallowBlocking>();
  }

  virtual ~SimpleURLLoaderTestBase() {}

  // Returns the path of a file that can be used in upload tests.
  static base::FilePath GetTestFilePath() {
    base::FilePath test_data_dir;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_data_dir);
    return test_data_dir.AppendASCII("services/test/data/title1.html");
  }

  static std::string GetTestFileContents() {
    base::ScopedAllowBlockingForTesting allow_blocking;
    std::string file_contents;
    EXPECT_TRUE(base::ReadFileToString(GetTestFilePath(), &file_contents));
    return file_contents;
  }

 protected:
  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<network::mojom::NetworkService> network_service_;
  std::unique_ptr<network::mojom::NetworkContextClient> network_context_client_;
  mojo::Remote<network::mojom::NetworkContext> network_context_;
  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory_;

  net::test_server::EmbeddedTestServer test_server_;

  std::unique_ptr<base::ScopedDisallowBlocking> disallow_blocking_;
};

enum class ReadAndDiscardBodyType {
  // The "SimpleURLLoaderUseReadAndDiscardBodyOption" feature is disabled.
  // TODO(ricea): Remove this when the
  // "SimpleURLLoaderUseReadAndDiscardBodyOption" feature is removed.
  kDisabled,

  // The feature is enabled, and MockURLLoader obeys it.
  kEnabled,

  // The feature is enabled, but MockURLLoader ignores it. This emulates a
  // URLLoader that does not implement the feature or forward to one that does.
  kEnabledButIgnored,
};

using DownloadTypeAndOptions =
    std::tuple<SimpleLoaderTestHelper::DownloadType, ReadAndDiscardBodyType>;

void PrintTo(const DownloadTypeAndOptions& value, std::ostream* os) {
  using enum SimpleLoaderTestHelper::DownloadType;
  const char* download_type = nullptr;
  switch (std::get<0>(value)) {
    case TO_STRING:
      download_type = "TO_STRING";
      break;
    case TO_FILE:
      download_type = "TO_FILE";
      break;
    case TO_TEMP_FILE:
      download_type = "TO_TEMP_FILE";
      break;
    case HEADERS_ONLY:
      download_type = "HEADERS_ONLY";
      break;
    case AS_STREAM:
      download_type = "AS_STREAM";
      break;
  }
  using enum ReadAndDiscardBodyType;
  const char* read_body_and_discard_type = nullptr;
  switch (std::get<1>(value)) {
    case kDisabled:
      read_body_and_discard_type = "NoReadAndDiscardBody";
      break;
    case kEnabled:
      read_body_and_discard_type = "ReadAndDiscardBodyImplemented";
      break;
    case kEnabledButIgnored:
      read_body_and_discard_type = "ReadAndDiscardBodyIgnored";
      break;
  }
  *os << download_type << "_and_" << read_body_and_discard_type;
}

struct URLLoaderFactoryTestConfig {
  ReadAndDiscardBodyType read_and_discard_body_type =
      ReadAndDiscardBodyType::kDisabled;
  bool expect_read_and_discard_option = false;
};

class SimpleURLLoaderTest
    : public SimpleURLLoaderTestBase,
      public testing::TestWithParam<DownloadTypeAndOptions> {
 public:
  SimpleURLLoaderTest() {
    if (GetReadAndDiscardBodyType() == ReadAndDiscardBodyType::kDisabled) {
      scoped_feature_list_.InitAndDisableFeature(
          kSimpleURLLoaderUseReadAndDiscardBodyOption);
    } else {
      scoped_feature_list_.InitAndEnableFeature(
          kSimpleURLLoaderUseReadAndDiscardBodyOption);
    }
  }

  ~SimpleURLLoaderTest() override {}

  SimpleLoaderTestHelper::DownloadType GetDownloadType() const {
    return std::get<0>(GetParam());
  }

  ReadAndDiscardBodyType GetReadAndDiscardBodyType() const {
    return std::get<1>(GetParam());
  }

  URLLoaderFactoryTestConfig GetURLLoaderFactoryTestConfig() {
    auto read_and_discard_body_type = GetReadAndDiscardBodyType();
    return {read_and_discard_body_type,
            IsHeadersOnly() && read_and_discard_body_type !=
                                   ReadAndDiscardBodyType::kDisabled};
  }

  // Many tests need to check this, so provide a short-cut.
  bool IsHeadersOnly() const {
    return GetDownloadType() ==
           SimpleLoaderTestHelper::DownloadType::HEADERS_ONLY;
  }

  std::unique_ptr<SimpleLoaderTestHelper> CreateHelper(
      std::unique_ptr<network::ResourceRequest> resource_request) {
    EXPECT_TRUE(resource_request);
    return std::make_unique<SimpleLoaderTestHelper>(std::move(resource_request),
                                                    GetDownloadType());
  }

  std::unique_ptr<SimpleLoaderTestHelper> CreateHelperForURL(
      const GURL& url,
      const char* method = "GET") {
    std::unique_ptr<network::ResourceRequest> resource_request =
        std::make_unique<network::ResourceRequest>();
    resource_request->url = url;
    resource_request->method = method;
    resource_request->enable_upload_progress = true;
    resource_request->trusted_params =
        network::ResourceRequest::TrustedParams();
    url::Origin request_origin = url::Origin::Create(url);
    resource_request->trusted_params->isolation_info =
        net::IsolationInfo::Create(net::IsolationInfo::RequestType::kOther,
                                   request_origin, request_origin,
                                   net::SiteForCookies());
    return std::make_unique<SimpleLoaderTestHelper>(std::move(resource_request),
                                                    GetDownloadType());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(SimpleURLLoaderTest, BasicRequest) {
  std::unique_ptr<network::ResourceRequest> resource_request =
      std::make_unique<network::ResourceRequest>();
  // Use a more interesting request than "/echo", just to verify more than the
  // request URL is hooked up.
  resource_request->url = test_server_.GetURL("/echoheader?foo");
  resource_request->headers.SetHeader("foo", kExpectedResponse);
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelper(std::move(resource_request));
  test_helper->StartSimpleLoaderAndWait(url_loader_factory_.get());

  EXPECT_EQ(net::OK, test_helper->simple_url_loader()->NetError());
  ASSERT_TRUE(test_helper->simple_url_loader()->CompletionStatus());
  EXPECT_EQ(net::OK,
            test_helper->simple_url_loader()->CompletionStatus()->error_code);
  EXPECT_EQ(200, test_helper->GetResponseCode());

  if (!IsHeadersOnly()) {
    ASSERT_TRUE(test_helper->response_body());
    EXPECT_EQ(kExpectedResponse, *test_helper->response_body());
    EXPECT_EQ(kExpectedResponseSize,
              test_helper->simple_url_loader()->GetContentSize());
    EXPECT_EQ(kExpectedResponseSize, test_helper->simple_url_loader()
                                         ->CompletionStatus()
                                         ->decoded_body_length);
  }
}

// Make sure the class works when the size of the encoded and decoded bodies are
// different.
TEST_P(SimpleURLLoaderTest, GzipBody) {
  std::string content(100, 'a');
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(test_server_.GetURL(
          base::StringPrintf("/gzip-body?%s", content.c_str())));
  test_helper->StartSimpleLoaderAndWait(url_loader_factory_.get());

  EXPECT_EQ(net::OK, test_helper->simple_url_loader()->NetError());
  ASSERT_TRUE(test_helper->simple_url_loader()->CompletionStatus());
  EXPECT_EQ(net::OK,
            test_helper->simple_url_loader()->CompletionStatus()->error_code);
  EXPECT_EQ(200, test_helper->GetResponseCode());

  if (!IsHeadersOnly()) {
    ASSERT_TRUE(test_helper->response_body());
    EXPECT_EQ(content, *test_helper->response_body());
    EXPECT_EQ(static_cast<int64_t>(content.size()),
              test_helper->simple_url_loader()->GetContentSize());
    ASSERT_TRUE(test_helper->simple_url_loader()->CompletionStatus());
    EXPECT_EQ(static_cast<int64_t>(content.size()),
              test_helper->simple_url_loader()
                  ->CompletionStatus()
                  ->decoded_body_length);
    EXPECT_LT(test_helper->simple_url_loader()
                  ->CompletionStatus()
                  ->encoded_body_length,
              test_helper->simple_url_loader()
                  ->CompletionStatus()
                  ->decoded_body_length);
  }
}

// Make sure redirects are followed.
TEST_P(SimpleURLLoaderTest, Redirect) {
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(test_server_.GetURL(
          "/server-redirect?" + test_server_.GetURL("/echo").spec()));
  test_helper->StartSimpleLoaderAndWait(url_loader_factory_.get());

  EXPECT_EQ(net::OK, test_helper->simple_url_loader()->NetError());
  ASSERT_TRUE(test_helper->simple_url_loader()->CompletionStatus());
  EXPECT_EQ(net::OK,
            test_helper->simple_url_loader()->CompletionStatus()->error_code);
  EXPECT_EQ(200, test_helper->GetResponseCode());

  if (!IsHeadersOnly()) {
    ASSERT_TRUE(test_helper->response_body());
    EXPECT_EQ("Echo", *test_helper->response_body());
  }
}

// Redirect to a file:// URL.
TEST_P(SimpleURLLoaderTest, RedirectFile) {
  std::unique_ptr<SimpleLoaderTestHelper> test_helper = CreateHelperForURL(
      test_server_.GetURL("/server-redirect?file:///etc/passwd"));
  test_helper->StartSimpleLoaderAndWait(url_loader_factory_.get());

  EXPECT_EQ(net::ERR_UNKNOWN_URL_SCHEME,
            test_helper->simple_url_loader()->NetError());
  ASSERT_TRUE(test_helper->simple_url_loader()->CompletionStatus());
  EXPECT_EQ(net::ERR_UNKNOWN_URL_SCHEME,
            test_helper->simple_url_loader()->CompletionStatus()->error_code);
  EXPECT_FALSE(test_helper->simple_url_loader()->ResponseInfo());
}

// Redirect to a data:// URL.
TEST_P(SimpleURLLoaderTest, RedirectData) {
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(test_server_.GetURL(
          "/server-redirect?data:text/plain;charset=utf-8;base64,Zm9v"));
  test_helper->StartSimpleLoaderAndWait(url_loader_factory_.get());

  EXPECT_EQ(net::ERR_UNKNOWN_URL_SCHEME,
            test_helper->simple_url_loader()->NetError());
  ASSERT_TRUE(test_helper->simple_url_loader()->CompletionStatus());
  EXPECT_EQ(net::ERR_UNKNOWN_URL_SCHEME,
            test_helper->simple_url_loader()->CompletionStatus()->error_code);
  EXPECT_FALSE(test_helper->simple_url_loader()->ResponseInfo());
}

// Make sure OnRedirectCallback is invoked on a redirect.
TEST_P(SimpleURLLoaderTest, OnRedirectCallback) {
  const GURL kInitialURL = test_server_.GetURL(
      "/server-redirect?" + test_server_.GetURL("/echo").spec());
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(kInitialURL);

  int num_redirects = 0;
  net::RedirectInfo redirect_info;
  network::mojom::URLResponseHeadPtr response_head;
  GURL url_before_redirect;
  test_helper->simple_url_loader()->SetOnRedirectCallback(base::BindRepeating(
      [](int* num_redirects, GURL* url_before_redirect_ptr,
         net::RedirectInfo* redirect_info_ptr,
         network::mojom::URLResponseHeadPtr* response_head_ptr,
         const GURL& url_before_redirect,
         const net::RedirectInfo& redirect_info,
         const network::mojom::URLResponseHead& response_head,
         std::vector<std::string>* to_be_removed_headers) {
        ++*num_redirects;
        *url_before_redirect_ptr = url_before_redirect;
        *redirect_info_ptr = redirect_info;
        *response_head_ptr = response_head.Clone();
      },
      base::Unretained(&num_redirects), base::Unretained(&url_before_redirect),
      base::Unretained(&redirect_info), base::Unretained(&response_head)));

  test_helper->StartSimpleLoaderAndWait(url_loader_factory_.get());

  if (!IsHeadersOnly()) {
    ASSERT_TRUE(test_helper->response_body());
    EXPECT_EQ("Echo", *test_helper->response_body());
  }

  EXPECT_EQ(1, num_redirects);
  EXPECT_EQ(kInitialURL, url_before_redirect);
  EXPECT_EQ(test_server_.GetURL("/echo"), redirect_info.new_url);
  ASSERT_TRUE(response_head->headers);
  EXPECT_EQ(301, response_head->headers->response_code());
}

// Make sure OnRedirectCallback is invoked on each redirect.
TEST_P(SimpleURLLoaderTest, OnRedirectCallbackTwoRedirects) {
  const GURL kRedirectURL = test_server_.GetURL(
      "/server-redirect?" + test_server_.GetURL("/echo").spec());
  const GURL kInitialURL =
      test_server_.GetURL("/server-redirect?" + kRedirectURL.spec());
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(kInitialURL);
  int num_redirects = 0;
  std::vector<GURL> urls_before_redirect;
  test_helper->simple_url_loader()->SetOnRedirectCallback(base::BindRepeating(
      [](int* num_redirects, std::vector<GURL>* urls_before_redirect,
         const GURL& url_before_redirect,
         const net::RedirectInfo& redirect_info,
         const network::mojom::URLResponseHead& response_head,
         std::vector<std::string>* to_be_removed_headers) {
        ++*num_redirects;
        urls_before_redirect->push_back(url_before_redirect);
      },
      base::Unretained(&num_redirects),
      base::Unretained(&urls_before_redirect)));

  test_helper->StartSimpleLoaderAndWait(url_loader_factory_.get());

  if (!IsHeadersOnly()) {
    ASSERT_TRUE(test_helper->response_body());
    EXPECT_EQ("Echo", *test_helper->response_body());
  }

  EXPECT_EQ(2, num_redirects);
  EXPECT_THAT(urls_before_redirect, ElementsAre(kInitialURL, kRedirectURL));
}

TEST_P(SimpleURLLoaderTest, DeleteInOnRedirectCallback) {
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(test_server_.GetURL(
          "/server-redirect?" + test_server_.GetURL("/echo").spec()));

  base::RunLoop run_loop;
  test_helper->simple_url_loader()->SetOnRedirectCallback(
      base::BindLambdaForTesting(
          [&](const GURL& url_before_redirect,
              const net::RedirectInfo& redirect_info,
              const network::mojom::URLResponseHead& response_head,
              std::vector<std::string>* to_be_removed_headers) {
            CHECK(test_helper);
            test_helper.reset();
            // Access the parameters to trigger a memory error if they have been
            // deleted. (ASAN build should catch it)
            EXPECT_FALSE(response_head.request_start.is_null());
            EXPECT_TRUE(url_before_redirect.is_valid());
            EXPECT_FALSE(redirect_info.new_url.is_empty());
            EXPECT_NE(to_be_removed_headers, nullptr);
            run_loop.Quit();
          }));
  test_helper->StartSimpleLoader(url_loader_factory_.get());

  run_loop.Run();
}

TEST_P(SimpleURLLoaderTest, UploadShortStringWithRedirect) {
  // Use a 307 redirect to preserve the body across the redirect.
  std::unique_ptr<SimpleLoaderTestHelper> test_helper = CreateHelperForURL(
      test_server_.GetURL("/server-redirect-307?" +
                          test_server_.GetURL("/echo").spec()),
      "POST");
  test_helper->simple_url_loader()->AttachStringForUpload(kShortUploadBody,
                                                          "text/plain");

  int num_redirects = 0;
  test_helper->simple_url_loader()->SetOnRedirectCallback(base::BindRepeating(
      [](int* num_redirects, const GURL& url_before_redirect,
         const net::RedirectInfo& redirect_info,
         const network::mojom::URLResponseHead& response_head,
         std::vector<std::string>* to_be_removed_headers) { ++*num_redirects; },
      base::Unretained(&num_redirects)));

  test_helper->StartSimpleLoaderAndWait(url_loader_factory_.get());
  EXPECT_EQ(net::OK, test_helper->simple_url_loader()->NetError());
  ASSERT_TRUE(test_helper->simple_url_loader()->CompletionStatus());
  EXPECT_EQ(net::OK,
            test_helper->simple_url_loader()->CompletionStatus()->error_code);

  if (!IsHeadersOnly()) {
    ASSERT_TRUE(test_helper->response_body());
    EXPECT_EQ(kShortUploadBody, *test_helper->response_body());
  }

  // Make sure request really was redirected.
  EXPECT_EQ(1, num_redirects);
}

TEST_P(SimpleURLLoaderTest, UploadLongStringWithRedirect) {
  // Use a 307 redirect to preserve the body across the redirect.
  std::unique_ptr<SimpleLoaderTestHelper> test_helper = CreateHelperForURL(
      test_server_.GetURL("/server-redirect-307?" +
                          test_server_.GetURL("/echo").spec()),
      "POST");
  test_helper->simple_url_loader()->AttachStringForUpload(GetLongUploadBody(),
                                                          "text/plain");

  int num_redirects = 0;
  test_helper->simple_url_loader()->SetOnRedirectCallback(base::BindRepeating(
      [](int* num_redirects, const GURL& url_before_redirect,
         const net::RedirectInfo& redirect_info,
         const network::mojom::URLResponseHead& response_head,
         std::vector<std::string>* to_be_removed_headers) { ++*num_redirects; },
      base::Unretained(&num_redirects)));

  test_helper->StartSimpleLoaderAndWait(url_loader_factory_.get());
  EXPECT_EQ(net::OK, test_helper->simple_url_loader()->NetError());
  ASSERT_TRUE(test_helper->simple_url_loader()->CompletionStatus());
  EXPECT_EQ(net::OK,
            test_helper->simple_url_loader()->CompletionStatus()->error_code);

  if (!IsHeadersOnly()) {
    ASSERT_TRUE(test_helper->response_body());
    EXPECT_EQ(GetLongUploadBody(), *test_helper->response_body());
  }

  // Make sure request really was redirected.
  EXPECT_EQ(1, num_redirects);
}

TEST_P(SimpleURLLoaderTest,
       OnRedirectCallbackReturnsExistingToBeRemovedHeaders) {
  // "/echoheader?foo" is used here to let |test_server_| send response with
  // "foo" header, and SimpleURLLoader's redirect callback marks "foo" header
  // to be removed, so that we can test "foo" header has been removed.
  GURL url = test_server_.GetURL("/echoheader?foo");
  std::unique_ptr<network::ResourceRequest> resource_request =
      std::make_unique<network::ResourceRequest>();
  resource_request->url = test_server_.GetURL("/server-redirect?" + url.spec());
  resource_request->headers.SetHeader("foo", kExpectedResponse);
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelper(std::move(resource_request));

  int num_redirects = 0;
  test_helper->simple_url_loader()->SetOnRedirectCallback(base::BindRepeating(
      [](int* num_redirects, const GURL& url_before_redirect,
         const net::RedirectInfo& redirect_info,
         const network::mojom::URLResponseHead& response_head,
         std::vector<std::string>* to_be_removed_headers) {
        ++*num_redirects;
        to_be_removed_headers->push_back("foo");
      },
      base::Unretained(&num_redirects)));

  test_helper->StartSimpleLoaderAndWait(url_loader_factory_.get());
  EXPECT_EQ(net::OK, test_helper->simple_url_loader()->NetError());
  ASSERT_TRUE(test_helper->simple_url_loader()->CompletionStatus());
  EXPECT_EQ(net::OK,
            test_helper->simple_url_loader()->CompletionStatus()->error_code);

  if (!IsHeadersOnly()) {
    ASSERT_TRUE(test_helper->response_body());
    // The "foo" header is removed since the SimpleURLLoader's redirect callback
    // marks "foo" header to be removed.
    EXPECT_EQ("None", *test_helper->response_body());
  }

  // Make sure request really was redirected.
  EXPECT_EQ(1, num_redirects);
}

TEST_P(SimpleURLLoaderTest,
       OnRedirectCallbackReturnsNonExistingToBeRemovedHeaders) {
  GURL url = test_server_.GetURL("/echoheader?foo");
  std::unique_ptr<network::ResourceRequest> resource_request =
      std::make_unique<network::ResourceRequest>();
  resource_request->url = test_server_.GetURL("/server-redirect?" + url.spec());
  resource_request->headers.SetHeader("foo", kExpectedResponse);
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelper(std::move(resource_request));

  int num_redirects = 0;
  test_helper->simple_url_loader()->SetOnRedirectCallback(base::BindRepeating(
      [](int* num_redirects, const GURL& url_before_redirect,
         const net::RedirectInfo& redirect_info,
         const network::mojom::URLResponseHead& response_head,
         std::vector<std::string>* to_be_removed_headers) {
        ++*num_redirects;
        to_be_removed_headers->push_back("bar");
      },
      base::Unretained(&num_redirects)));

  test_helper->StartSimpleLoaderAndWait(url_loader_factory_.get());
  EXPECT_EQ(net::OK, test_helper->simple_url_loader()->NetError());
  ASSERT_TRUE(test_helper->simple_url_loader()->CompletionStatus());
  EXPECT_EQ(net::OK,
            test_helper->simple_url_loader()->CompletionStatus()->error_code);

  if (!IsHeadersOnly()) {
    ASSERT_TRUE(test_helper->response_body());
    // The "foo" header is not removed since the SimpleURLLoader's redirect
    // callback marks "bar" header to be removed.
    EXPECT_EQ(kExpectedResponse, *test_helper->response_body());
  }

  // Make sure request really was redirected.
  EXPECT_EQ(1, num_redirects);
}

TEST_P(SimpleURLLoaderTest, OnResponseStartedCallback) {
  GURL url = test_server_.GetURL("/set-header?foo: bar");
  std::unique_ptr<network::ResourceRequest> resource_request =
      std::make_unique<network::ResourceRequest>();
  resource_request->url = test_server_.GetURL("/server-redirect?" + url.spec());
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelper(std::move(resource_request));

  base::RunLoop run_loop;
  GURL actual_url;
  std::string foo_header_value;
  test_helper->simple_url_loader()->SetOnResponseStartedCallback(base::BindOnce(
      [](GURL* out_final_url, std::string* foo_header_value,
         base::OnceClosure quit_closure, const GURL& final_url,
         const mojom::URLResponseHead& response_head) {
        *out_final_url = final_url;
        if (response_head.headers) {
          response_head.headers->EnumerateHeader(/*iter=*/nullptr, "foo",
                                                 foo_header_value);
        }
        std::move(quit_closure).Run();
      },
      &actual_url, &foo_header_value, run_loop.QuitClosure()));
  test_helper->StartSimpleLoaderAndWait(url_loader_factory_.get());
  run_loop.Run();

  EXPECT_EQ(url, actual_url);
  EXPECT_EQ("bar", foo_header_value);
}

TEST_P(SimpleURLLoaderTest, DeleteInOnResponseStartedCallback) {
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(test_server_.GetURL("/echo"));

  SimpleLoaderTestHelper* unowned_test_helper = test_helper.get();
  base::RunLoop run_loop;
  unowned_test_helper->simple_url_loader()->SetOnResponseStartedCallback(
      base::BindOnce(
          [](std::unique_ptr<SimpleLoaderTestHelper> test_helper,
             base::OnceClosure quit_closure, const GURL& final_url,
             const mojom::URLResponseHead& response_head) {
            // Delete the SimpleURLLoader.
            test_helper.reset();
            // Access the parameters to trigger a memory error if they have been
            // deleted. (ASAN build should catch it)
            EXPECT_FALSE(response_head.request_start.is_null());
            EXPECT_TRUE(final_url.is_valid());
            std::move(quit_closure).Run();
          },
          std::move(test_helper), run_loop.QuitClosure()));

  unowned_test_helper->StartSimpleLoader(url_loader_factory_.get());

  run_loop.Run();
}

// Check the case where the SimpleURLLoader is deleted in the completion
// callback.
TEST_P(SimpleURLLoaderTest, DestroyLoaderInOnComplete) {
  std::unique_ptr<network::ResourceRequest> resource_request =
      std::make_unique<network::ResourceRequest>();
  // Use a more interesting request than "/echo", just to verify more than the
  // request URL is hooked up.
  resource_request->url = test_server_.GetURL("/echoheader?foo");
  resource_request->headers.SetHeader("foo", kExpectedResponse);
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelper(std::move(resource_request));
  test_helper->set_destroy_loader_on_complete(true);
  test_helper->StartSimpleLoaderAndWait(url_loader_factory_.get());

  if (!IsHeadersOnly()) {
    ASSERT_TRUE(test_helper->response_body());
    EXPECT_EQ(kExpectedResponse, *test_helper->response_body());
  }
}

// Check the case where a URLLoaderFactory with a closed Mojo pipe was passed
// in.
TEST_P(SimpleURLLoaderTest, DisconnectedURLLoader) {
  // Destroy the NetworkContext, and wait for the Mojo URLLoaderFactory proxy to
  // be informed of the closed pipe.
  network_context_.reset();
  base::RunLoop().RunUntilIdle();

  std::unique_ptr<network::ResourceRequest> resource_request =
      std::make_unique<network::ResourceRequest>();
  resource_request->url = test_server_.GetURL("/echoheader?foo");
  resource_request->headers.SetHeader("foo", kExpectedResponse);
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelper(std::move(resource_request));
  test_helper->StartSimpleLoaderAndWait(url_loader_factory_.get());

  EXPECT_EQ(net::ERR_FAILED, test_helper->simple_url_loader()->NetError());
  EXPECT_FALSE(test_helper->simple_url_loader()->CompletionStatus());
  EXPECT_FALSE(test_helper->simple_url_loader()->ResponseInfo());
}

// Check that no body is returned with an HTTP error response.
TEST_P(SimpleURLLoaderTest, HttpErrorStatusCodeResponse) {
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(test_server_.GetURL("/echo?status=400"));
  test_helper->StartSimpleLoaderAndWait(url_loader_factory_.get());

  EXPECT_EQ(net::ERR_HTTP_RESPONSE_CODE_FAILURE,
            test_helper->simple_url_loader()->NetError());
  EXPECT_FALSE(test_helper->simple_url_loader()->CompletionStatus());
  EXPECT_EQ(400, test_helper->GetResponseCode());
  EXPECT_FALSE(test_helper->response_body());
  EXPECT_EQ(0, test_helper->simple_url_loader()->GetContentSize());
}

// Check that the body is returned with an HTTP error response, when
// SetAllowHttpErrorResults(true) is called.
TEST_P(SimpleURLLoaderTest, HttpErrorStatusCodeResponseAllowed) {
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(test_server_.GetURL("/echo?status=400"));
  test_helper->simple_url_loader()->SetAllowHttpErrorResults(true);
  test_helper->StartSimpleLoaderAndWait(url_loader_factory_.get());

  EXPECT_EQ(net::OK, test_helper->simple_url_loader()->NetError());
  ASSERT_TRUE(test_helper->simple_url_loader()->CompletionStatus());
  EXPECT_EQ(net::OK,
            test_helper->simple_url_loader()->CompletionStatus()->error_code);
  EXPECT_EQ(400, test_helper->GetResponseCode());

  if (!IsHeadersOnly()) {
    ASSERT_TRUE(test_helper->response_body());
    EXPECT_EQ("Echo", *test_helper->response_body());
    EXPECT_EQ(4, test_helper->simple_url_loader()->GetContentSize());
    EXPECT_EQ(4, test_helper->simple_url_loader()
                     ->CompletionStatus()
                     ->decoded_body_length);
  }
}

TEST_P(SimpleURLLoaderTest, EmptyResponseBody) {
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(test_server_.GetURL("/nocontent"));
  test_helper->StartSimpleLoaderAndWait(url_loader_factory_.get());

  EXPECT_EQ(net::OK, test_helper->simple_url_loader()->NetError());
  ASSERT_TRUE(test_helper->simple_url_loader()->CompletionStatus());
  EXPECT_EQ(net::OK,
            test_helper->simple_url_loader()->CompletionStatus()->error_code);
  EXPECT_EQ(204, test_helper->GetResponseCode());

  if (!IsHeadersOnly()) {
    ASSERT_TRUE(test_helper->response_body());
    // A response body is sent from the NetworkService, but it's empty.
    EXPECT_EQ("", *test_helper->response_body());
    EXPECT_EQ(0, test_helper->simple_url_loader()->GetContentSize());
    EXPECT_EQ(0, test_helper->simple_url_loader()
                     ->CompletionStatus()
                     ->decoded_body_length);
  }
}

TEST_P(SimpleURLLoaderTest, BigResponseBody) {
  // Big response that requires multiple reads, and exceeds the maximum size
  // limit of SimpleURLLoader::DownloadToString().  That is, this test make sure
  // that DownloadToStringOfUnboundedSizeUntilCrashAndDie() can receive strings
  // longer than DownloadToString() allows.
  const uint32_t kResponseSize = 2 * 1024 * 1024;
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(test_server_.GetURL(
          base::StringPrintf("/response-size?%u", kResponseSize)));
  test_helper->StartSimpleLoaderAndWait(url_loader_factory_.get());

  EXPECT_EQ(net::OK, test_helper->simple_url_loader()->NetError());
  ASSERT_TRUE(test_helper->simple_url_loader()->CompletionStatus());
  EXPECT_EQ(net::OK,
            test_helper->simple_url_loader()->CompletionStatus()->error_code);
  EXPECT_EQ(200, test_helper->GetResponseCode());

  if (!IsHeadersOnly()) {
    ASSERT_TRUE(test_helper->response_body());
    EXPECT_EQ(kResponseSize, test_helper->response_body()->length());
    EXPECT_EQ(std::string(kResponseSize, 'a'), *test_helper->response_body());
    EXPECT_EQ(kResponseSize,
              test_helper->simple_url_loader()->GetContentSize());
    EXPECT_EQ(kResponseSize, test_helper->simple_url_loader()
                                 ->CompletionStatus()
                                 ->decoded_body_length);
  }
}

#define GTEST_SKIP_IF_STREAM()                                                \
  if (GetDownloadType() == SimpleLoaderTestHelper::DownloadType::AS_STREAM) { \
    GTEST_SKIP() << "Download to stream doesn't support response sizes.";     \
  }

TEST_P(SimpleURLLoaderTest, ResponseBodyWithSizeMatchingLimit) {
  GTEST_SKIP_IF_STREAM();

  const uint32_t kResponseSize = 16;
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(test_server_.GetURL(
          base::StringPrintf("/response-size?%u", kResponseSize)));
  test_helper->StartSimpleLoaderAndWait(url_loader_factory_.get(),
                                        kResponseSize);

  EXPECT_EQ(net::OK, test_helper->simple_url_loader()->NetError());
  ASSERT_TRUE(test_helper->simple_url_loader()->CompletionStatus());
  EXPECT_EQ(net::OK,
            test_helper->simple_url_loader()->CompletionStatus()->error_code);
  EXPECT_EQ(200, test_helper->GetResponseCode());

  if (!IsHeadersOnly()) {
    ASSERT_TRUE(test_helper->response_body());
    EXPECT_EQ(kResponseSize, test_helper->response_body()->length());
    EXPECT_EQ(std::string(kResponseSize, 'a'), *test_helper->response_body());
    EXPECT_EQ(kResponseSize,
              test_helper->simple_url_loader()->GetContentSize());
    EXPECT_EQ(kResponseSize, test_helper->simple_url_loader()
                                 ->CompletionStatus()
                                 ->decoded_body_length);
  }
}

TEST_P(SimpleURLLoaderTest, ResponseBodyWithSizeBelowLimit) {
  GTEST_SKIP_IF_STREAM();

  const uint32_t kResponseSize = 16;
  const uint32_t kMaxResponseSize = kResponseSize + 1;
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(test_server_.GetURL(
          base::StringPrintf("/response-size?%u", kResponseSize)));
  test_helper->StartSimpleLoaderAndWait(url_loader_factory_.get(),
                                        kMaxResponseSize);

  EXPECT_EQ(net::OK, test_helper->simple_url_loader()->NetError());
  ASSERT_TRUE(test_helper->simple_url_loader()->CompletionStatus());
  EXPECT_EQ(net::OK,
            test_helper->simple_url_loader()->CompletionStatus()->error_code);
  EXPECT_EQ(200, test_helper->GetResponseCode());

  if (!IsHeadersOnly()) {
    ASSERT_TRUE(test_helper->response_body());
    EXPECT_EQ(kResponseSize, test_helper->response_body()->length());
    EXPECT_EQ(std::string(kResponseSize, 'a'), *test_helper->response_body());
    EXPECT_EQ(kResponseSize,
              test_helper->simple_url_loader()->GetContentSize());
    EXPECT_EQ(kResponseSize, test_helper->simple_url_loader()
                                 ->CompletionStatus()
                                 ->decoded_body_length);
  }
}

TEST_P(SimpleURLLoaderTest, ResponseBodyWithSizeAboveLimit) {
  GTEST_SKIP_IF_STREAM();

  const uint32_t kResponseSize = 16;
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(test_server_.GetURL(
          base::StringPrintf("/response-size?%u", kResponseSize)));
  test_helper->StartSimpleLoaderAndWait(url_loader_factory_.get(),
                                        kResponseSize - 1);

  if (IsHeadersOnly()) {
    EXPECT_EQ(net::OK, test_helper->simple_url_loader()->NetError());
    EXPECT_TRUE(test_helper->simple_url_loader()->CompletionStatus());
  } else {
    EXPECT_FALSE(test_helper->simple_url_loader()->CompletionStatus());
    EXPECT_EQ(net::ERR_INSUFFICIENT_RESOURCES,
              test_helper->simple_url_loader()->NetError());
    EXPECT_EQ(0, test_helper->simple_url_loader()->GetContentSize());
  }
  EXPECT_FALSE(test_helper->response_body());
}

// Same as above, but with setting allow_partial_results to true.
TEST_P(SimpleURLLoaderTest, ResponseBodyWithSizeAboveLimitPartialResponse) {
  GTEST_SKIP_IF_STREAM();

  const uint32_t kResponseSize = 16;
  const uint32_t kMaxResponseSize = kResponseSize - 1;
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(test_server_.GetURL(
          base::StringPrintf("/response-size?%u", kResponseSize)));
  test_helper->SetAllowPartialResults(true);
  test_helper->StartSimpleLoaderAndWait(url_loader_factory_.get(),
                                        kMaxResponseSize);

  if (IsHeadersOnly()) {
    EXPECT_TRUE(test_helper->simple_url_loader()->CompletionStatus());
    EXPECT_EQ(net::OK, test_helper->simple_url_loader()->NetError());
    EXPECT_FALSE(test_helper->response_body());
  } else {
    EXPECT_FALSE(test_helper->simple_url_loader()->CompletionStatus());
    EXPECT_EQ(net::ERR_INSUFFICIENT_RESOURCES,
              test_helper->simple_url_loader()->NetError());
    ASSERT_TRUE(test_helper->response_body());
    EXPECT_EQ(std::string(kMaxResponseSize, 'a'),
              *test_helper->response_body());
    EXPECT_EQ(kMaxResponseSize, test_helper->response_body()->length());
    EXPECT_EQ(kMaxResponseSize,
              test_helper->simple_url_loader()->GetContentSize());
  }
}

// The next 4 tests duplicate the above 4, but with larger response sizes. This
// means the size limit will not be exceeded on the first read.
TEST_P(SimpleURLLoaderTest, BigResponseBodyWithSizeMatchingLimit) {
  GTEST_SKIP_IF_STREAM();

  const uint32_t kResponseSize = 512 * 1024;
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(test_server_.GetURL(
          base::StringPrintf("/response-size?%u", kResponseSize)));
  test_helper->StartSimpleLoaderAndWait(url_loader_factory_.get(),
                                        kResponseSize);

  EXPECT_EQ(net::OK, test_helper->simple_url_loader()->NetError());
  ASSERT_TRUE(test_helper->simple_url_loader()->CompletionStatus());
  EXPECT_EQ(net::OK,
            test_helper->simple_url_loader()->CompletionStatus()->error_code);

  if (!IsHeadersOnly()) {
    ASSERT_TRUE(test_helper->response_body());
    EXPECT_EQ(kResponseSize, test_helper->response_body()->length());
    EXPECT_EQ(std::string(kResponseSize, 'a'), *test_helper->response_body());
    EXPECT_EQ(kResponseSize,
              test_helper->simple_url_loader()->GetContentSize());
    EXPECT_EQ(kResponseSize, test_helper->simple_url_loader()
                                 ->CompletionStatus()
                                 ->decoded_body_length);
  }
}

TEST_P(SimpleURLLoaderTest, BigResponseBodyWithSizeBelowLimit) {
  GTEST_SKIP_IF_STREAM();

  const uint32_t kResponseSize = 512 * 1024;
  const uint32_t kMaxResponseSize = kResponseSize + 1;
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(test_server_.GetURL(
          base::StringPrintf("/response-size?%u", kResponseSize)));
  test_helper->StartSimpleLoaderAndWait(url_loader_factory_.get(),
                                        kMaxResponseSize);

  EXPECT_EQ(net::OK, test_helper->simple_url_loader()->NetError());
  ASSERT_TRUE(test_helper->simple_url_loader()->CompletionStatus());
  EXPECT_EQ(net::OK,
            test_helper->simple_url_loader()->CompletionStatus()->error_code);

  if (!IsHeadersOnly()) {
    ASSERT_TRUE(test_helper->response_body());
    EXPECT_EQ(kResponseSize, test_helper->response_body()->length());
    EXPECT_EQ(std::string(kResponseSize, 'a'), *test_helper->response_body());
    EXPECT_EQ(kResponseSize,
              test_helper->simple_url_loader()->GetContentSize());
    EXPECT_EQ(kResponseSize, test_helper->simple_url_loader()
                                 ->CompletionStatus()
                                 ->decoded_body_length);
  }
}

TEST_P(SimpleURLLoaderTest, BigResponseBodyWithSizeAboveLimit) {
  GTEST_SKIP_IF_STREAM();

  const uint32_t kResponseSize = 512 * 1024;
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(test_server_.GetURL(
          base::StringPrintf("/response-size?%u", kResponseSize)));
  test_helper->StartSimpleLoaderAndWait(url_loader_factory_.get(),
                                        kResponseSize - 1);

  if (IsHeadersOnly()) {
    EXPECT_EQ(net::OK, test_helper->simple_url_loader()->NetError());
    ASSERT_TRUE(test_helper->simple_url_loader()->CompletionStatus());
    EXPECT_EQ(net::OK,
              test_helper->simple_url_loader()->CompletionStatus()->error_code);
  } else {
    EXPECT_EQ(net::ERR_INSUFFICIENT_RESOURCES,
              test_helper->simple_url_loader()->NetError());
    EXPECT_FALSE(test_helper->simple_url_loader()->CompletionStatus());
    EXPECT_EQ(0, test_helper->simple_url_loader()->GetContentSize());
  }
  EXPECT_FALSE(test_helper->response_body());
}

TEST_P(SimpleURLLoaderTest, BigResponseBodyWithSizeAboveLimitPartialResponse) {
  GTEST_SKIP_IF_STREAM();

  const uint32_t kResponseSize = 512 * 1024;
  const uint32_t kMaxResponseSize = kResponseSize - 1;
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(test_server_.GetURL(
          base::StringPrintf("/response-size?%u", kResponseSize)));
  test_helper->SetAllowPartialResults(true);
  test_helper->StartSimpleLoaderAndWait(url_loader_factory_.get(),
                                        kMaxResponseSize);

  if (IsHeadersOnly()) {
    EXPECT_EQ(net::OK, test_helper->simple_url_loader()->NetError());
    ASSERT_TRUE(test_helper->simple_url_loader()->CompletionStatus());
    EXPECT_EQ(net::OK,
              test_helper->simple_url_loader()->CompletionStatus()->error_code);
    EXPECT_FALSE(test_helper->response_body());
  } else {
    EXPECT_EQ(net::ERR_INSUFFICIENT_RESOURCES,
              test_helper->simple_url_loader()->NetError());
    EXPECT_FALSE(test_helper->simple_url_loader()->CompletionStatus());
    ASSERT_TRUE(test_helper->response_body());
    EXPECT_EQ(std::string(kMaxResponseSize, 'a'),
              *test_helper->response_body());
    EXPECT_EQ(kMaxResponseSize, test_helper->response_body()->length());
    EXPECT_EQ(kMaxResponseSize,
              test_helper->simple_url_loader()->GetContentSize());
  }
}

TEST_P(SimpleURLLoaderTest, NetErrorBeforeHeaders) {
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(test_server_.GetURL("/close-socket"));
  test_helper->StartSimpleLoaderAndWait(url_loader_factory_.get());

  EXPECT_EQ(net::ERR_EMPTY_RESPONSE,
            test_helper->simple_url_loader()->NetError());
  ASSERT_TRUE(test_helper->simple_url_loader()->CompletionStatus());
  EXPECT_EQ(net::ERR_EMPTY_RESPONSE,
            test_helper->simple_url_loader()->CompletionStatus()->error_code);
  EXPECT_FALSE(test_helper->simple_url_loader()->ResponseInfo());
  EXPECT_FALSE(test_helper->response_body());
  EXPECT_EQ(0, test_helper->simple_url_loader()->GetContentSize());
  EXPECT_EQ(0, test_helper->simple_url_loader()
                   ->CompletionStatus()
                   ->decoded_body_length);
}

TEST_P(SimpleURLLoaderTest, NetErrorBeforeHeadersWithPartialResults) {
  // Allow response body on error. There should still be no response body, since
  // the error is before body reading starts.
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(test_server_.GetURL("/close-socket"));
  test_helper->SetAllowPartialResults(true);
  test_helper->StartSimpleLoaderAndWait(url_loader_factory_.get());

  EXPECT_FALSE(test_helper->response_body());

  EXPECT_EQ(net::ERR_EMPTY_RESPONSE,
            test_helper->simple_url_loader()->NetError());
  ASSERT_TRUE(test_helper->simple_url_loader()->CompletionStatus());
  EXPECT_EQ(net::ERR_EMPTY_RESPONSE,
            test_helper->simple_url_loader()->CompletionStatus()->error_code);
  EXPECT_FALSE(test_helper->simple_url_loader()->ResponseInfo());
  EXPECT_EQ(0, test_helper->simple_url_loader()->GetContentSize());
  EXPECT_EQ(0, test_helper->simple_url_loader()
                   ->CompletionStatus()
                   ->decoded_body_length);
}

TEST_P(SimpleURLLoaderTest, NetErrorAfterHeaders) {
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(test_server_.GetURL(kInvalidGzipPath));
  test_helper->StartSimpleLoaderAndWait(url_loader_factory_.get());

  EXPECT_EQ(net::ERR_CONTENT_DECODING_FAILED,
            test_helper->simple_url_loader()->NetError());
  ASSERT_TRUE(test_helper->simple_url_loader()->CompletionStatus());
  EXPECT_EQ(net::ERR_CONTENT_DECODING_FAILED,
            test_helper->simple_url_loader()->CompletionStatus()->error_code);
  EXPECT_EQ(200, test_helper->GetResponseCode());
  EXPECT_FALSE(test_helper->response_body());
  EXPECT_EQ(0, test_helper->simple_url_loader()->GetContentSize());
  EXPECT_EQ(0, test_helper->simple_url_loader()
                   ->CompletionStatus()
                   ->decoded_body_length);
}

TEST_P(SimpleURLLoaderTest, NetErrorAfterHeadersWithPartialResults) {
  // Allow response body on error. This case results in a 0-byte response body.
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(test_server_.GetURL(kInvalidGzipPath));
  test_helper->SetAllowPartialResults(true);
  test_helper->StartSimpleLoaderAndWait(url_loader_factory_.get());

  EXPECT_EQ(net::ERR_CONTENT_DECODING_FAILED,
            test_helper->simple_url_loader()->NetError());
  ASSERT_TRUE(test_helper->simple_url_loader()->CompletionStatus());
  EXPECT_EQ(net::ERR_CONTENT_DECODING_FAILED,
            test_helper->simple_url_loader()->CompletionStatus()->error_code);
  EXPECT_EQ(200, test_helper->GetResponseCode());

  if (!IsHeadersOnly()) {
    ASSERT_TRUE(test_helper->response_body());
    EXPECT_EQ("", *test_helper->response_body());
    EXPECT_EQ(0, test_helper->simple_url_loader()->GetContentSize());
    EXPECT_EQ(0, test_helper->simple_url_loader()
                     ->CompletionStatus()
                     ->decoded_body_length);
  }
}

TEST_P(SimpleURLLoaderTest, TruncatedBody) {
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(test_server_.GetURL(kTruncatedBodyPath));
  test_helper->StartSimpleLoaderAndWait(url_loader_factory_.get());

  EXPECT_EQ(net::ERR_CONTENT_LENGTH_MISMATCH,
            test_helper->simple_url_loader()->NetError());
  ASSERT_TRUE(test_helper->simple_url_loader()->CompletionStatus());
  EXPECT_EQ(net::ERR_CONTENT_LENGTH_MISMATCH,
            test_helper->simple_url_loader()->CompletionStatus()->error_code);
  EXPECT_EQ(200, test_helper->GetResponseCode());
  EXPECT_FALSE(test_helper->response_body());
  EXPECT_EQ(static_cast<int64_t>(strlen(kTruncatedBody)),
            test_helper->simple_url_loader()->GetContentSize());
  EXPECT_EQ(static_cast<int64_t>(strlen(kTruncatedBody)),
            test_helper->simple_url_loader()
                ->CompletionStatus()
                ->decoded_body_length);
}

TEST_P(SimpleURLLoaderTest, TruncatedBodyWithPartialResults) {
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(test_server_.GetURL(kTruncatedBodyPath));
  test_helper->SetAllowPartialResults(true);
  test_helper->StartSimpleLoaderAndWait(url_loader_factory_.get());

  EXPECT_EQ(net::ERR_CONTENT_LENGTH_MISMATCH,
            test_helper->simple_url_loader()->NetError());
  ASSERT_TRUE(test_helper->simple_url_loader()->CompletionStatus());
  EXPECT_EQ(net::ERR_CONTENT_LENGTH_MISMATCH,
            test_helper->simple_url_loader()->CompletionStatus()->error_code);
  EXPECT_EQ(200, test_helper->GetResponseCode());

  if (!IsHeadersOnly()) {
    ASSERT_TRUE(test_helper->response_body());
    EXPECT_EQ(static_cast<int64_t>(strlen(kTruncatedBody)),
              test_helper->simple_url_loader()->GetContentSize());
    EXPECT_EQ(static_cast<int64_t>(strlen(kTruncatedBody)),
              test_helper->simple_url_loader()
                  ->CompletionStatus()
                  ->decoded_body_length);
  }
}

// Test case where NetworkService is destroyed before headers are received (and
// before the request is even made, for that matter).
TEST_P(SimpleURLLoaderTest, DestroyServiceBeforeResponseStarts) {
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(test_server_.GetURL("/hung"));
  test_helper->StartSimpleLoader(url_loader_factory_.get());
  {
    // Destroying the NetworkService may result in blocking operations on some
    // platforms, like joining threads.
    base::ScopedAllowBlockingForTesting allow_blocking;
    network_service_.reset();
  }
  test_helper->Wait();

  EXPECT_EQ(net::ERR_FAILED, test_helper->simple_url_loader()->NetError());
  EXPECT_FALSE(test_helper->simple_url_loader()->CompletionStatus());
  EXPECT_FALSE(test_helper->response_body());
  ASSERT_FALSE(test_helper->simple_url_loader()->ResponseInfo());
}

TEST_P(SimpleURLLoaderTest, UploadShortString) {
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(test_server_.GetURL("/echo"), "POST");
  test_helper->simple_url_loader()->AttachStringForUpload(kShortUploadBody,
                                                          "text/plain");
  test_helper->StartSimpleLoaderAndWait(url_loader_factory_.get());
  EXPECT_EQ(net::OK, test_helper->simple_url_loader()->NetError());
  ASSERT_TRUE(test_helper->simple_url_loader()->CompletionStatus());
  EXPECT_EQ(net::OK,
            test_helper->simple_url_loader()->CompletionStatus()->error_code);

  if (!IsHeadersOnly()) {
    ASSERT_TRUE(test_helper->response_body());
    EXPECT_EQ(kShortUploadBody, *test_helper->response_body());
  }
}

TEST_P(SimpleURLLoaderTest, UploadLongString) {
  std::string long_string = GetLongUploadBody();
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(test_server_.GetURL("/echo"), "POST");
  test_helper->simple_url_loader()->AttachStringForUpload(long_string,
                                                          "text/plain");
  test_helper->StartSimpleLoaderAndWait(url_loader_factory_.get());
  EXPECT_EQ(net::OK, test_helper->simple_url_loader()->NetError());
  ASSERT_TRUE(test_helper->simple_url_loader()->CompletionStatus());
  EXPECT_EQ(net::OK,
            test_helper->simple_url_loader()->CompletionStatus()->error_code);

  if (!IsHeadersOnly()) {
    ASSERT_TRUE(test_helper->response_body());
    EXPECT_EQ(long_string, *test_helper->response_body());
  }
}

TEST_P(SimpleURLLoaderTest, UploadEmptyString) {
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(test_server_.GetURL("/echo"), "POST");
  test_helper->simple_url_loader()->AttachStringForUpload("", "text/plain");
  test_helper->StartSimpleLoaderAndWait(url_loader_factory_.get());
  EXPECT_EQ(net::OK, test_helper->simple_url_loader()->NetError());
  ASSERT_TRUE(test_helper->simple_url_loader()->CompletionStatus());
  EXPECT_EQ(net::OK,
            test_helper->simple_url_loader()->CompletionStatus()->error_code);

  if (!IsHeadersOnly()) {
    ASSERT_TRUE(test_helper->response_body());
    EXPECT_EQ("", *test_helper->response_body());
  }

  // Also make sure the correct method was sent, with the right content-type.
  test_helper = CreateHelperForURL(test_server_.GetURL("/echoall"), "POST");
  test_helper->simple_url_loader()->AttachStringForUpload("", "text/plain");
  test_helper->StartSimpleLoaderAndWait(url_loader_factory_.get());
  EXPECT_EQ(net::OK, test_helper->simple_url_loader()->NetError());
  ASSERT_TRUE(test_helper->simple_url_loader()->CompletionStatus());
  EXPECT_EQ(net::OK,
            test_helper->simple_url_loader()->CompletionStatus()->error_code);

  if (!IsHeadersOnly()) {
    ASSERT_TRUE(test_helper->response_body());
    EXPECT_NE(std::string::npos,
              test_helper->response_body()->find("Content-Type: text/plain"));
    EXPECT_NE(std::string::npos, test_helper->response_body()->find("POST /"));
    EXPECT_EQ(std::string::npos, test_helper->response_body()->find("PUT /"));
  }
}

TEST_P(SimpleURLLoaderTest, UploadShortStringWithRetry) {
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(test_server_.GetURL(kFailOnceThenEchoBody), "POST");
  test_helper->simple_url_loader()->AttachStringForUpload(kShortUploadBody,
                                                          "text/plain");
  test_helper->simple_url_loader()->SetRetryOptions(
      1, SimpleURLLoader::RETRY_ON_5XX);
  test_helper->StartSimpleLoaderAndWait(url_loader_factory_.get());
  EXPECT_EQ(net::OK, test_helper->simple_url_loader()->NetError());
  ASSERT_TRUE(test_helper->simple_url_loader()->CompletionStatus());
  EXPECT_EQ(net::OK,
            test_helper->simple_url_loader()->CompletionStatus()->error_code);

  if (!IsHeadersOnly()) {
    ASSERT_TRUE(test_helper->response_body());
    EXPECT_EQ(kShortUploadBody, *test_helper->response_body());
  }

  if (GetDownloadType() == SimpleLoaderTestHelper::DownloadType::AS_STREAM) {
    EXPECT_EQ(1, test_helper->download_as_stream_retries());
  }
}

TEST_P(SimpleURLLoaderTest, UploadLongStringWithRetry) {
  std::string long_string = GetLongUploadBody();
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(test_server_.GetURL(kFailOnceThenEchoBody), "POST");
  test_helper->simple_url_loader()->AttachStringForUpload(long_string,
                                                          "text/plain");
  test_helper->simple_url_loader()->SetRetryOptions(
      1, SimpleURLLoader::RETRY_ON_5XX);
  test_helper->StartSimpleLoaderAndWait(url_loader_factory_.get());
  EXPECT_EQ(net::OK, test_helper->simple_url_loader()->NetError());
  ASSERT_TRUE(test_helper->simple_url_loader()->CompletionStatus());
  EXPECT_EQ(net::OK,
            test_helper->simple_url_loader()->CompletionStatus()->error_code);

  if (!IsHeadersOnly()) {
    ASSERT_TRUE(test_helper->response_body());
    EXPECT_EQ(long_string, *test_helper->response_body());
  }

  if (GetDownloadType() == SimpleLoaderTestHelper::DownloadType::AS_STREAM) {
    EXPECT_EQ(1, test_helper->download_as_stream_retries());
  }
}

TEST_P(SimpleURLLoaderTest, UploadFile) {
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(test_server_.GetURL("/echo"), "POST");
  test_helper->simple_url_loader()->AttachFileForUpload(GetTestFilePath(),
                                                        "text/plain");
  test_helper->StartSimpleLoaderAndWait(url_loader_factory_.get());
  EXPECT_EQ(net::OK, test_helper->simple_url_loader()->NetError());
  ASSERT_TRUE(test_helper->simple_url_loader()->CompletionStatus());
  EXPECT_EQ(net::OK,
            test_helper->simple_url_loader()->CompletionStatus()->error_code);

  if (!IsHeadersOnly()) {
    ASSERT_TRUE(test_helper->response_body());
    EXPECT_EQ(GetTestFileContents(), *test_helper->response_body());
  }

  // Also make sure the correct method was sent, with the right content-type.
  test_helper = CreateHelperForURL(test_server_.GetURL("/echoall"), "POST");
  test_helper->simple_url_loader()->AttachFileForUpload(GetTestFilePath(),
                                                        "text/plain");
  test_helper->StartSimpleLoaderAndWait(url_loader_factory_.get());
  EXPECT_EQ(net::OK, test_helper->simple_url_loader()->NetError());
  ASSERT_TRUE(test_helper->simple_url_loader()->CompletionStatus());
  EXPECT_EQ(net::OK,
            test_helper->simple_url_loader()->CompletionStatus()->error_code);

  if (!IsHeadersOnly()) {
    ASSERT_TRUE(test_helper->response_body());
    EXPECT_NE(std::string::npos,
              test_helper->response_body()->find("Content-Type: text/plain"));
    EXPECT_NE(std::string::npos, test_helper->response_body()->find("POST /"));
    EXPECT_EQ(std::string::npos, test_helper->response_body()->find("PUT /"));
  }
}

TEST_P(SimpleURLLoaderTest, UploadFileRange) {
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(test_server_.GetURL("/echo"), "POST");
  // These two values should return the second line of the test file.
  const uint64_t kOffset = 7;
  const uint64_t kLength = 13;
  test_helper->simple_url_loader()->AttachFileForUpload(
      GetTestFilePath(), "text/plain", kOffset, kLength);
  test_helper->StartSimpleLoaderAndWait(url_loader_factory_.get());
  EXPECT_EQ(net::OK, test_helper->simple_url_loader()->NetError());
  ASSERT_TRUE(test_helper->simple_url_loader()->CompletionStatus());
  EXPECT_EQ(net::OK,
            test_helper->simple_url_loader()->CompletionStatus()->error_code);

  if (!IsHeadersOnly()) {
    ASSERT_TRUE(test_helper->response_body());
    EXPECT_EQ(GetTestFileContents().substr(kOffset, kLength),
              *test_helper->response_body());
  }
}

TEST_P(SimpleURLLoaderTest, UploadFileWithPut) {
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(test_server_.GetURL("/echo"), "PUT");
  test_helper->simple_url_loader()->AttachFileForUpload(GetTestFilePath(),
                                                        "text/plain");
  test_helper->StartSimpleLoaderAndWait(url_loader_factory_.get());
  EXPECT_EQ(net::OK, test_helper->simple_url_loader()->NetError());
  ASSERT_TRUE(test_helper->simple_url_loader()->CompletionStatus());
  EXPECT_EQ(net::OK,
            test_helper->simple_url_loader()->CompletionStatus()->error_code);

  if (!IsHeadersOnly()) {
    ASSERT_TRUE(test_helper->response_body());
    EXPECT_EQ(GetTestFileContents(), *test_helper->response_body());
  }

  // Also make sure the correct method was sent, with the right content-type.
  test_helper = CreateHelperForURL(test_server_.GetURL("/echoall"), "PUT");
  test_helper->simple_url_loader()->AttachFileForUpload(GetTestFilePath(),
                                                        "text/salted");
  test_helper->StartSimpleLoaderAndWait(url_loader_factory_.get());
  EXPECT_EQ(net::OK, test_helper->simple_url_loader()->NetError());
  ASSERT_TRUE(test_helper->simple_url_loader()->CompletionStatus());
  EXPECT_EQ(net::OK,
            test_helper->simple_url_loader()->CompletionStatus()->error_code);

  if (!IsHeadersOnly()) {
    ASSERT_TRUE(test_helper->response_body());
    EXPECT_NE(std::string::npos,
              test_helper->response_body()->find("Content-Type: text/salted"));
    EXPECT_EQ(std::string::npos,
              test_helper->response_body()->find("Content-Type: text/plain"));
    EXPECT_EQ(std::string::npos, test_helper->response_body()->find("POST /"));
    EXPECT_NE(std::string::npos, test_helper->response_body()->find("PUT /"));
  }
}

TEST_P(SimpleURLLoaderTest, UploadFileWithRetry) {
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(test_server_.GetURL(kFailOnceThenEchoBody), "POST");
  test_helper->simple_url_loader()->AttachFileForUpload(GetTestFilePath(),
                                                        "text/plain");
  test_helper->simple_url_loader()->SetRetryOptions(
      1, SimpleURLLoader::RETRY_ON_5XX);
  test_helper->StartSimpleLoaderAndWait(url_loader_factory_.get());
  EXPECT_EQ(net::OK, test_helper->simple_url_loader()->NetError());
  ASSERT_TRUE(test_helper->simple_url_loader()->CompletionStatus());
  EXPECT_EQ(net::OK,
            test_helper->simple_url_loader()->CompletionStatus()->error_code);

  if (!IsHeadersOnly()) {
    ASSERT_TRUE(test_helper->response_body());
    EXPECT_EQ(GetTestFileContents(), *test_helper->response_body());
    EXPECT_EQ(static_cast<int64_t>(GetTestFileContents().size()),
              test_helper->simple_url_loader()->GetContentSize());
    EXPECT_EQ(static_cast<int64_t>(GetTestFileContents().size()),
              test_helper->simple_url_loader()
                  ->CompletionStatus()
                  ->decoded_body_length);
  }

  if (GetDownloadType() == SimpleLoaderTestHelper::DownloadType::AS_STREAM) {
    EXPECT_EQ(1, test_helper->download_as_stream_retries());
  }
}

TEST_P(SimpleURLLoaderTest, UploadNonexistentFile) {
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(test_server_.GetURL("/echo"), "POST");
  // Path to a file that doesn't exist.  Start with the test directory just to
  // get a valid absolute path the test has access to.
  base::FilePath path_to_nonexistent_file =
      GetTestFilePath().DirName().AppendASCII("this/file/does/not/exist");
  // Appending a path to the end of a file should guarantee no such file exists.
  test_helper->simple_url_loader()->AttachFileForUpload(
      path_to_nonexistent_file, "text/plain");
  test_helper->StartSimpleLoaderAndWait(url_loader_factory_.get());
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND,
            test_helper->simple_url_loader()->NetError());
  ASSERT_TRUE(test_helper->simple_url_loader()->CompletionStatus());
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND,
            test_helper->simple_url_loader()->CompletionStatus()->error_code);
  EXPECT_FALSE(test_helper->simple_url_loader()->ResponseInfo());
  EXPECT_FALSE(test_helper->response_body());
  EXPECT_EQ(0, test_helper->simple_url_loader()->GetContentSize());
  EXPECT_EQ(0, test_helper->simple_url_loader()
                   ->CompletionStatus()
                   ->decoded_body_length);
}

// Test case where uploading a file is canceled before the URLLoader is started
// (But after the SimpleURLLoader is started).
TEST_P(SimpleURLLoaderTest, UploadFileCanceledBeforeLoaderStarted) {
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(GURL("http://does_not_matter:7/"), "POST");
  test_helper->simple_url_loader()->AttachFileForUpload(GetTestFilePath(),
                                                        "text/plain");

  test_helper->StartSimpleLoader(url_loader_factory_.get());
  test_helper.reset();
  task_environment_.RunUntilIdle();
}

TEST_P(SimpleURLLoaderTest, UploadFileCanceledWithRetry) {
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(test_server_.GetURL("/hung"), "POST");
  test_helper->simple_url_loader()->AttachFileForUpload(GetTestFilePath(),
                                                        "text/plain");
  test_helper->simple_url_loader()->SetRetryOptions(
      2, SimpleURLLoader::RETRY_ON_5XX);
  test_helper->StartSimpleLoader(url_loader_factory_.get());
  task_environment_.RunUntilIdle();
  test_helper.reset();
  task_environment_.RunUntilIdle();
}

enum class TestLoaderEvent {
  // States related to reading the long upload body (Returned by
  // GetLongUploadBody()). They expect the ResourceRequest to have a request
  // body with a single DataPipeGetter.

  // Call Read() on the DataPipeGetter.
  kStartReadLongUploadBody,
  // Wait for Read() to complete, expecting it to succeed and return the size of
  // the string returned by GetLongUploadBody().
  kWaitForLongUploadBodySize,
  // Read the entire body, expecting it to equal the string returned by
  // GetLongUploadBody().
  kReadLongUploadBody,
  // Read the first byte of the upload body. Cannot be followed by a call to
  // kReadLongUploadBody.
  kReadFirstByteOfLongUploadBody,

  // DNS resolution error.
  kNameNotResolved,

  kReceivedRedirect,
  // Receive a response with a 200 status code.
  kReceivedResponse,
  // Receive a response with a 401 status code.
  kReceived401Response,
  // Receive a response with a 501 status code.
  kReceived501Response,
  // Receive a response with no body data.
  kReceivedResponseNoData,
  kBodyDataRead,
  // ResponseComplete indicates a success.
  kResponseComplete,
  // ResponseComplete is passed a network error (net::ERR_TIMED_OUT).
  kResponseCompleteFailed,
  // ResponseComplete is passed net::ERR_NETWORK_CHANGED.
  kResponseCompleteNetworkChanged,
  // Less body data is received than is expected.
  kResponseCompleteTruncated,
  // More body data is received than is expected.
  kResponseCompleteWithExtraData,
  kClientPipeClosed,
  kBodyBufferClosed,
  // Advances time by 1 second. Only callable when the test environment is
  // configured to be TimeSource::MOCK_TIME.
  kAdvanceOneSecond,
};

// URLLoader that the test fixture can control. This allows finer grained
// control over event order over when a pipe is closed, and in ordering of
// events where there are multiple pipes. It also allows sending events in
// unexpected order, to test handling of events from less trusted processes.
class MockURLLoader : public network::mojom::URLLoader {
 public:
  MockURLLoader(
      base::test::TaskEnvironment* task_environment,
      mojo::PendingReceiver<network::mojom::URLLoader> url_loader_receiver,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      std::vector<TestLoaderEvent> test_events,
      scoped_refptr<network::ResourceRequestBody> request_body,
      bool should_create_data_pipe)
      : task_environment_(task_environment),
        receiver_(this, std::move(url_loader_receiver)),
        client_(std::move(client)),
        test_events_(std::move(test_events)),
        should_create_data_pipe_(should_create_data_pipe) {
    if (request_body && request_body->elements()->size() == 1 &&
        (*request_body->elements())[0].type() ==
            network::mojom::DataElementDataView::Tag::kDataPipe) {
      const auto& element =
          (*request_body->elements())[0].As<network::DataElementDataPipe>();
      data_pipe_getter_.Bind(element.CloneDataPipeGetter());
      DCHECK(data_pipe_getter_);
    }
  }

  void RunTest() {
    for (auto test_event : test_events_) {
      switch (test_event) {
        case TestLoaderEvent::kStartReadLongUploadBody: {
          ASSERT_TRUE(data_pipe_getter_);
          upload_data_pipe_.reset();
          weak_factory_for_data_pipe_callbacks_.InvalidateWeakPtrs();
          read_run_loop_ = std::make_unique<base::RunLoop>();
          mojo::ScopedDataPipeProducerHandle producer_handle;
          ASSERT_EQ(
              mojo::CreateDataPipe(nullptr, producer_handle, upload_data_pipe_),
              MOJO_RESULT_OK);
          data_pipe_getter_->Read(
              std::move(producer_handle),
              base::BindOnce(
                  &MockURLLoader::OnReadComplete,
                  weak_factory_for_data_pipe_callbacks_.GetWeakPtr()));
          // Continue instead of break, to avoid spinning the message loop -
          // only wait for the response if next step indicates to do so.
          continue;
        }
        case TestLoaderEvent::kWaitForLongUploadBodySize: {
          ASSERT_TRUE(data_pipe_getter_);
          ASSERT_TRUE(read_run_loop_);
          read_run_loop_->Run();
          break;
        }
        case TestLoaderEvent::kReadLongUploadBody: {
          ASSERT_TRUE(data_pipe_getter_);
          ASSERT_TRUE(upload_data_pipe_.is_valid());
          std::string upload_body;
          while (true) {
            std::string read_buffer(32 * 1024, '\0');
            size_t actually_read_bytes = 0;
            MojoResult result = upload_data_pipe_->ReadData(
                MOJO_READ_DATA_FLAG_NONE,
                base::as_writable_byte_span(read_buffer), actually_read_bytes);
            if (result == MOJO_RESULT_SHOULD_WAIT) {
              base::RunLoop().RunUntilIdle();
              continue;
            }
            if (result != MOJO_RESULT_OK)
              break;
            upload_body.append(
                std::string_view(read_buffer).substr(0, actually_read_bytes));
          }
          EXPECT_EQ(GetLongUploadBody(), upload_body);
          break;
        }
        case TestLoaderEvent::kReadFirstByteOfLongUploadBody: {
          ASSERT_TRUE(data_pipe_getter_);
          ASSERT_TRUE(upload_data_pipe_.is_valid());
          MojoResult result;
          uint8_t byte;
          size_t read_size = 0;
          while (true) {
            result = upload_data_pipe_->ReadData(
                MOJO_READ_DATA_FLAG_NONE, base::span_from_ref(byte), read_size);
            if (result != MOJO_RESULT_SHOULD_WAIT)
              break;
            base::RunLoop().RunUntilIdle();
          }
          if (result != MOJO_RESULT_OK) {
            ADD_FAILURE() << "Expected to read one byte of data.";
            break;
          }
          EXPECT_EQ(1u, read_size);
          EXPECT_EQ(GetLongUploadBody()[0], byte);
          break;
        }
        case TestLoaderEvent::kNameNotResolved: {
          network::URLLoaderCompletionStatus status;
          status.error_code = net::ERR_NAME_NOT_RESOLVED;
          status.decoded_body_length = CountBytesToSend();
          client_->OnComplete(status);
          break;
        }
        case TestLoaderEvent::kReceivedRedirect: {
          net::RedirectInfo redirect_info;
          redirect_info.new_method = "GET";
          redirect_info.new_url = GURL("bar://foo/");
          redirect_info.status_code = 301;

          auto response_info = network::mojom::URLResponseHead::New();
          std::string headers(
              "HTTP/1.0 301 The Response Has Moved to Another Server\n"
              "Location: bar://foo/");
          response_info->headers =
              base::MakeRefCounted<net::HttpResponseHeaders>(
                  net::HttpUtil::AssembleRawHeaders(headers));
          client_->OnReceiveRedirect(redirect_info, std::move(response_info));
          break;
        }
        case TestLoaderEvent::kReceivedResponse: {
          auto response_info = network::mojom::URLResponseHead::New();
          std::string headers("HTTP/1.0 200 OK");
          response_info->headers =
              base::MakeRefCounted<net::HttpResponseHeaders>(
                  net::HttpUtil::AssembleRawHeaders(headers));
          mojo::ScopedDataPipeConsumerHandle consumer_handle;
          if (should_create_data_pipe_) {
            ASSERT_EQ(mojo::CreateDataPipe(1024, body_stream_, consumer_handle),
                      MOJO_RESULT_OK);
          }
          client_->OnReceiveResponse(std::move(response_info),
                                     std::move(consumer_handle), std::nullopt);
          break;
        }
        case TestLoaderEvent::kReceived401Response: {
          auto response_info = network::mojom::URLResponseHead::New();
          std::string headers("HTTP/1.0 401 Client Borkage");
          response_info->headers =
              base::MakeRefCounted<net::HttpResponseHeaders>(
                  net::HttpUtil::AssembleRawHeaders(headers));
          mojo::ScopedDataPipeConsumerHandle consumer_handle;
          if (should_create_data_pipe_) {
            ASSERT_EQ(mojo::CreateDataPipe(1024, body_stream_, consumer_handle),
                      MOJO_RESULT_OK);
          }
          client_->OnReceiveResponse(std::move(response_info),
                                     std::move(consumer_handle), std::nullopt);
          break;
        }
        case TestLoaderEvent::kReceived501Response: {
          auto response_info = network::mojom::URLResponseHead::New();
          std::string headers("HTTP/1.0 501 Server Borkage");
          response_info->headers =
              base::MakeRefCounted<net::HttpResponseHeaders>(
                  net::HttpUtil::AssembleRawHeaders(headers));
          mojo::ScopedDataPipeConsumerHandle consumer_handle;
          if (should_create_data_pipe_) {
            ASSERT_EQ(mojo::CreateDataPipe(1024, body_stream_, consumer_handle),
                      MOJO_RESULT_OK);
          }
          client_->OnReceiveResponse(std::move(response_info),
                                     std::move(consumer_handle), std::nullopt);
          break;
        }
        case TestLoaderEvent::kReceivedResponseNoData: {
          auto response_info = network::mojom::URLResponseHead::New();
          std::string headers("HTTP/1.0 200 OK");
          response_info->headers =
              base::MakeRefCounted<net::HttpResponseHeaders>(
                  net::HttpUtil::AssembleRawHeaders(headers));
          client_->OnReceiveResponse(std::move(response_info),
                                     mojo::ScopedDataPipeConsumerHandle(),
                                     std::nullopt);
          break;
        }
        case TestLoaderEvent::kBodyDataRead: {
          if (should_create_data_pipe_) {
            size_t actually_written_bytes = 0;
            // Writing one byte should always succeed synchronously, for the
            // amount of data these tests send.
            EXPECT_EQ(MOJO_RESULT_OK,
                      body_stream_->WriteData(base::byte_span_from_cstring("a"),
                                              MOJO_WRITE_DATA_FLAG_NONE,
                                              actually_written_bytes));
            EXPECT_EQ(actually_written_bytes, 1u);
          }
          break;
        }
        case TestLoaderEvent::kResponseComplete: {
          network::URLLoaderCompletionStatus status;
          status.error_code = net::OK;
          status.decoded_body_length = CountBytesToSend();
          client_->OnComplete(status);
          break;
        }
        case TestLoaderEvent::kResponseCompleteFailed: {
          network::URLLoaderCompletionStatus status;
          // Use an error that SimpleURLLoader doesn't create itself, so clear
          // when this is the source of the error code.
          status.error_code = net::ERR_TIMED_OUT;
          status.decoded_body_length = CountBytesToSend();
          client_->OnComplete(status);
          break;
        }
        case TestLoaderEvent::kResponseCompleteNetworkChanged: {
          network::URLLoaderCompletionStatus status;
          status.error_code = net::ERR_NETWORK_CHANGED;
          status.decoded_body_length = CountBytesToSend();
          client_->OnComplete(status);
          break;
        }
        case TestLoaderEvent::kResponseCompleteTruncated: {
          network::URLLoaderCompletionStatus status;
          status.error_code = net::OK;
          status.decoded_body_length = CountBytesToSend() + 1;
          client_->OnComplete(status);
          break;
        }
        case TestLoaderEvent::kResponseCompleteWithExtraData: {
          // Make sure |decoded_body_length| doesn't underflow.
          DCHECK_GT(CountBytesToSend(), 0u);
          network::URLLoaderCompletionStatus status;
          status.error_code = net::OK;
          status.decoded_body_length = CountBytesToSend() - 1;
          client_->OnComplete(status);
          break;
        }
        case TestLoaderEvent::kClientPipeClosed: {
          DCHECK(client_);
          EXPECT_TRUE(receiver_.is_bound());
          client_.reset();
          break;
        }
        case TestLoaderEvent::kBodyBufferClosed: {
          body_stream_.reset();
          break;
        }
        case TestLoaderEvent::kAdvanceOneSecond: {
          task_environment_->FastForwardBy(base::Seconds(1));
          break;
        }
      }
      // Wait for Mojo to pass along the message, to ensure expected ordering.
      task_environment_->RunUntilIdle();
    }
  }

  MockURLLoader(const MockURLLoader&) = delete;
  MockURLLoader& operator=(const MockURLLoader&) = delete;

  ~MockURLLoader() override {}

  // network::mojom::URLLoader implementation:
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers,
      const std::optional<GURL>& new_url) override {}
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override {
    NOTREACHED_IN_MIGRATION();
  }
  void PauseReadingBodyFromNet() override {}
  void ResumeReadingBodyFromNet() override {}

  network::mojom::URLLoaderClient* client() const { return client_.get(); }

 private:
  // Counts the total number of bytes that will be sent over the course of
  // running the request. Includes both those that have been sent already, and
  // those that have yet to be sent.
  uint32_t CountBytesToSend() const {
    int total_bytes = 0;
    for (auto test_event : test_events_) {
      if (test_event == TestLoaderEvent::kBodyDataRead)
        ++total_bytes;
    }
    return total_bytes;
  }

  void OnReadComplete(int32_t status, uint64_t size) {
    EXPECT_EQ(net::OK, status);
    EXPECT_EQ(GetLongUploadBody().size(), size);
    read_run_loop_->Quit();
  }

  raw_ptr<base::test::TaskEnvironment> task_environment_;

  std::unique_ptr<net::URLRequest> url_request_;
  mojo::Receiver<network::mojom::URLLoader> receiver_;
  mojo::Remote<network::mojom::URLLoaderClient> client_;

  std::vector<TestLoaderEvent> test_events_;

  mojo::ScopedDataPipeProducerHandle body_stream_;

  mojo::Remote<network::mojom::DataPipeGetter> data_pipe_getter_;
  mojo::ScopedDataPipeConsumerHandle upload_data_pipe_;

  std::unique_ptr<base::RunLoop> read_run_loop_;

  const bool should_create_data_pipe_;

  base::WeakPtrFactory<MockURLLoader> weak_factory_for_data_pipe_callbacks_{
      this};
};

class MockURLLoaderFactory : public network::mojom::URLLoaderFactory {
 public:
  explicit MockURLLoaderFactory(base::test::TaskEnvironment* task_environment,
                                URLLoaderFactoryTestConfig test_config)
      : task_environment_(task_environment), test_config_(test_config) {}

  MockURLLoaderFactory(const MockURLLoaderFactory&) = delete;
  MockURLLoaderFactory& operator=(const MockURLLoaderFactory&) = delete;

  ~MockURLLoaderFactory() override {}

  // network::mojom::URLLoaderFactory implementation:

  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> url_loader_receiver,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& url_request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override {
    ASSERT_FALSE(test_events_.empty());
    requested_urls_.push_back(url_request.url);
    const bool read_and_discard_body_option_set =
        options & mojom::kURLLoadOptionReadAndDiscardBody;
    if (test_config_.expect_read_and_discard_option) {
      EXPECT_TRUE(read_and_discard_body_option_set);
    } else {
      EXPECT_FALSE(read_and_discard_body_option_set);
    }
    bool should_create_data_pipe = true;
    if (read_and_discard_body_option_set &&
        test_config_.read_and_discard_body_type !=
            ReadAndDiscardBodyType::kEnabledButIgnored) {
      should_create_data_pipe = false;
    }
    url_loaders_.push_back(std::make_unique<MockURLLoader>(
        task_environment_, std::move(url_loader_receiver), std::move(client),
        test_events_.front(), url_request.request_body,
        should_create_data_pipe));
    test_events_.pop_front();

    url_loader_queue_.push_back(url_loaders_.back().get());
  }

  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override {
    mojo::ReceiverId id = receiver_set_.Add(this, std::move(receiver));
    if (close_new_binding_on_clone_)
      receiver_set_.Remove(id);
  }

  // Makes clone fail close the newly created binding after bining the request,
  // Simulating the case where the network service goes away before the cloned
  // interface is used.
  void set_close_new_binding_on_clone(bool close_new_binding_on_clone) {
    close_new_binding_on_clone_ = close_new_binding_on_clone;
  }

  // Adds a events that will be returned by a single MockURLLoader. Mutliple
  // calls mean multiple MockURLLoaders are expected to be created. Each will
  // run to completion before the next one is expected to be created.
  void AddEvents(const std::vector<TestLoaderEvent> events) {
    DCHECK(url_loaders_.empty());
    test_events_.push_back(events);
  }

  // Runs all events for all created URLLoaders, in order.
  void RunTest(SimpleLoaderTestHelper* test_helper,
               bool wait_for_completion = true) {
    mojo::Remote<network::mojom::URLLoaderFactory> factory;
    receiver_set_.Add(this, factory.BindNewPipeAndPassReceiver());

    test_helper->StartSimpleLoader(factory.get());

    // Wait for the first URLLoader to start receiving messages.
    base::RunLoop().RunUntilIdle();

    while (!url_loader_queue_.empty()) {
      url_loader_queue_.front()->RunTest();
      url_loader_queue_.pop_front();
    }

    if (wait_for_completion)
      test_helper->Wait();

    // All loads with added events should have been created and had their
    // RunTest() methods called by the time the above loop completes.
    EXPECT_TRUE(test_events_.empty());
  }

  const std::list<GURL>& requested_urls() const { return requested_urls_; }

 private:
  const raw_ptr<base::test::TaskEnvironment> task_environment_;
  std::list<std::unique_ptr<MockURLLoader>> url_loaders_;
  std::list<std::vector<TestLoaderEvent>> test_events_;

  bool close_new_binding_on_clone_ = false;

  const URLLoaderFactoryTestConfig test_config_;

  // Queue of URLLoaders that have yet to had their RunTest method called.
  // Separate list than |url_loaders_| so that old pipes aren't destroyed.
  std::list<raw_ptr<MockURLLoader, CtnExperimental>> url_loader_queue_;

  std::list<GURL> requested_urls_;

  mojo::ReceiverSet<network::mojom::URLLoaderFactory> receiver_set_;
};

// Check that the request fails if OnComplete() is called before anything else.
TEST_P(SimpleURLLoaderTest, ResponseCompleteBeforeReceivedResponse) {
  MockURLLoaderFactory loader_factory(&task_environment_,
                                      GetURLLoaderFactoryTestConfig());
  loader_factory.AddEvents({TestLoaderEvent::kResponseComplete});
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(GURL("foo://bar/"));
  loader_factory.RunTest(test_helper.get());

  EXPECT_EQ(net::ERR_UNEXPECTED, test_helper->simple_url_loader()->NetError());
  EXPECT_FALSE(test_helper->simple_url_loader()->CompletionStatus());
  EXPECT_FALSE(test_helper->simple_url_loader()->ResponseInfo());
  EXPECT_FALSE(test_helper->response_body());
}

// Check that the request fails if OnComplete() is called before the body pipe
// is received.
TEST_P(SimpleURLLoaderTest, ResponseCompleteAfterReceivedResponse) {
  MockURLLoaderFactory loader_factory(&task_environment_,
                                      GetURLLoaderFactoryTestConfig());
  loader_factory.AddEvents({TestLoaderEvent::kReceivedResponseNoData,
                            TestLoaderEvent::kResponseComplete});
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(GURL("foo://bar/"));
  loader_factory.RunTest(test_helper.get());

  if (IsHeadersOnly()) {
    EXPECT_EQ(net::OK, test_helper->simple_url_loader()->NetError());
    EXPECT_TRUE(test_helper->simple_url_loader()->CompletionStatus());
  } else {
    EXPECT_EQ(net::ERR_UNEXPECTED,
              test_helper->simple_url_loader()->NetError());
    EXPECT_FALSE(test_helper->simple_url_loader()->CompletionStatus());
  }
  EXPECT_EQ(200, test_helper->GetResponseCode());
  EXPECT_FALSE(test_helper->response_body());
}

TEST_P(SimpleURLLoaderTest, CloseClientPipeBeforeBodyStarts) {
  MockURLLoaderFactory loader_factory(&task_environment_,
                                      GetURLLoaderFactoryTestConfig());
  loader_factory.AddEvents({TestLoaderEvent::kReceivedResponseNoData,
                            TestLoaderEvent::kClientPipeClosed});
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(GURL("foo://bar/"));
  loader_factory.RunTest(test_helper.get());

  EXPECT_EQ(net::ERR_FAILED, test_helper->simple_url_loader()->NetError());
  EXPECT_FALSE(test_helper->simple_url_loader()->CompletionStatus());
  EXPECT_EQ(200, test_helper->GetResponseCode());
  EXPECT_FALSE(test_helper->response_body());
}

// This test tries closing the client pipe / completing the request in most
// possible valid orders relative to read events (Which always occur in the same
// order).
// TODO(crbug.com/40815508): Flakes on ios simulator.
#if BUILDFLAG(IS_IOS)
#define MAYBE_CloseClientPipeOrder DISABLED_CloseClientPipeOrder
#else
#define MAYBE_CloseClientPipeOrder CloseClientPipeOrder
#endif
TEST_P(SimpleURLLoaderTest, MAYBE_CloseClientPipeOrder) {
  if (GetReadAndDiscardBodyType() == ReadAndDiscardBodyType::kEnabled &&
      IsHeadersOnly()) {
    GTEST_SKIP() << "There is no client pipe in this case";
  }

  enum class ClientCloseOrder {
    kBeforeData,
    kDuringData,
    kAfterData,
    kAfterBufferClosed,
  };

  // In what order the URLLoaderClient pipe is closed, relative to read events.
  // Order of other main events can't vary, relative to each other (Getting body
  // pipe, reading body bytes, closing body pipe).
  const ClientCloseOrder kClientCloseOrder[] = {
      ClientCloseOrder::kBeforeData,
      ClientCloseOrder::kDuringData,
      ClientCloseOrder::kAfterData,
      ClientCloseOrder::kAfterBufferClosed,
  };

  const TestLoaderEvent kClientCloseEvents[] = {
      TestLoaderEvent::kResponseComplete,
      TestLoaderEvent::kResponseCompleteFailed,
      TestLoaderEvent::kResponseCompleteTruncated,
      TestLoaderEvent::kClientPipeClosed,
  };

  for (const auto close_client_order : kClientCloseOrder) {
    for (const auto close_client_event : kClientCloseEvents) {
      for (uint32_t bytes_received = 0; bytes_received < 3; bytes_received++) {
        for (int allow_partial_results = 0; allow_partial_results < 2;
             allow_partial_results++) {
          if (close_client_order == ClientCloseOrder::kDuringData &&
              bytes_received < 2) {
            continue;
          }
          MockURLLoaderFactory loader_factory(&task_environment_,
                                              GetURLLoaderFactoryTestConfig());
          std::vector<TestLoaderEvent> events;
          events.push_back(TestLoaderEvent::kReceivedResponse);
          if (close_client_order == ClientCloseOrder::kBeforeData)
            events.push_back(close_client_event);

          for (uint32_t i = 0; i < bytes_received; ++i) {
            events.push_back(TestLoaderEvent::kBodyDataRead);
            if (i == 0 && close_client_order == ClientCloseOrder::kDuringData)
              events.push_back(close_client_event);
          }

          if (close_client_order == ClientCloseOrder::kAfterData)
            events.push_back(close_client_event);
          events.push_back(TestLoaderEvent::kBodyBufferClosed);
          if (close_client_order == ClientCloseOrder::kAfterBufferClosed)
            events.push_back(close_client_event);
          loader_factory.AddEvents(events);

          std::unique_ptr<SimpleLoaderTestHelper> test_helper =
              CreateHelperForURL(GURL("foo://bar/"));
          test_helper->SetAllowPartialResults(allow_partial_results);
          loader_factory.RunTest(test_helper.get());

          EXPECT_EQ(200, test_helper->GetResponseCode());
          if (close_client_event != TestLoaderEvent::kResponseComplete) {
            if (close_client_event ==
                TestLoaderEvent::kResponseCompleteFailed) {
              EXPECT_EQ(net::ERR_TIMED_OUT,
                        test_helper->simple_url_loader()->NetError());
              ASSERT_TRUE(test_helper->simple_url_loader()->CompletionStatus());
              EXPECT_EQ(net::ERR_TIMED_OUT, test_helper->simple_url_loader()
                                                ->CompletionStatus()
                                                ->error_code);
            } else {
              EXPECT_EQ(net::ERR_FAILED,
                        test_helper->simple_url_loader()->NetError());
              EXPECT_FALSE(
                  test_helper->simple_url_loader()->CompletionStatus());
            }
            if (!allow_partial_results) {
              EXPECT_FALSE(test_helper->response_body());
            } else if (!IsHeadersOnly()) {
              ASSERT_TRUE(test_helper->response_body());
              EXPECT_EQ(std::string(bytes_received, 'a'),
                        *test_helper->response_body());
            }
          } else {
            EXPECT_EQ(net::OK, test_helper->simple_url_loader()->NetError());
            ASSERT_TRUE(test_helper->simple_url_loader()->CompletionStatus());
            EXPECT_EQ(net::OK, test_helper->simple_url_loader()
                                   ->CompletionStatus()
                                   ->error_code);

            if (!IsHeadersOnly()) {
              ASSERT_TRUE(test_helper->response_body());
              EXPECT_EQ(std::string(bytes_received, 'a'),
                        *test_helper->response_body());
            }
          }
        }
      }
    }
  }
}

// Make sure the close client pipe message doesn't cause any issues.
TEST_P(SimpleURLLoaderTest, ErrorAndCloseClientPipeBeforeBodyStarts) {
  MockURLLoaderFactory loader_factory(&task_environment_,
                                      GetURLLoaderFactoryTestConfig());
  loader_factory.AddEvents({TestLoaderEvent::kReceivedResponseNoData,
                            TestLoaderEvent::kResponseCompleteFailed,
                            TestLoaderEvent::kClientPipeClosed});
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(GURL("foo://bar/"));
  loader_factory.RunTest(test_helper.get());

  EXPECT_EQ(net::ERR_TIMED_OUT, test_helper->simple_url_loader()->NetError());
  ASSERT_TRUE(test_helper->simple_url_loader()->CompletionStatus());
  EXPECT_EQ(net::ERR_TIMED_OUT,
            test_helper->simple_url_loader()->CompletionStatus()->error_code);
  EXPECT_EQ(200, test_helper->GetResponseCode());
  EXPECT_FALSE(test_helper->response_body());
}

// Make sure the close client pipe message doesn't cause any issues.
TEST_P(SimpleURLLoaderTest, SuccessAndCloseClientPipeBeforeBodyComplete) {
  MockURLLoaderFactory loader_factory(&task_environment_,
                                      GetURLLoaderFactoryTestConfig());
  loader_factory.AddEvents(
      {TestLoaderEvent::kReceivedResponse, TestLoaderEvent::kResponseComplete,
       TestLoaderEvent::kClientPipeClosed, TestLoaderEvent::kBodyDataRead,
       TestLoaderEvent::kBodyBufferClosed});
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(GURL("foo://bar/"));
  loader_factory.RunTest(test_helper.get());

  EXPECT_EQ(net::OK, test_helper->simple_url_loader()->NetError());
  ASSERT_TRUE(test_helper->simple_url_loader()->CompletionStatus());
  EXPECT_EQ(net::OK,
            test_helper->simple_url_loader()->CompletionStatus()->error_code);
  EXPECT_EQ(200, test_helper->GetResponseCode());

  if (!IsHeadersOnly()) {
    ASSERT_TRUE(test_helper->response_body());
    EXPECT_EQ("a", *test_helper->response_body());
  }
}

// Make sure the close client pipe message doesn't cause any issues.
TEST_P(SimpleURLLoaderTest, SuccessAndCloseClientPipeAfterBodyComplete) {
  MockURLLoaderFactory loader_factory(&task_environment_,
                                      GetURLLoaderFactoryTestConfig());
  loader_factory.AddEvents(
      {TestLoaderEvent::kReceivedResponse, TestLoaderEvent::kBodyDataRead,
       TestLoaderEvent::kBodyBufferClosed, TestLoaderEvent::kResponseComplete,
       TestLoaderEvent::kClientPipeClosed});
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(GURL("foo://bar/"));
  loader_factory.RunTest(test_helper.get());

  EXPECT_EQ(net::OK, test_helper->simple_url_loader()->NetError());
  ASSERT_TRUE(test_helper->simple_url_loader()->CompletionStatus());
  EXPECT_EQ(net::OK,
            test_helper->simple_url_loader()->CompletionStatus()->error_code);
  EXPECT_EQ(200, test_helper->GetResponseCode());

  if (!IsHeadersOnly()) {
    ASSERT_TRUE(test_helper->response_body());
    EXPECT_EQ("a", *test_helper->response_body());
  }
}

TEST_P(SimpleURLLoaderTest, DoubleReceivedResponse) {
  MockURLLoaderFactory loader_factory(&task_environment_,
                                      GetURLLoaderFactoryTestConfig());
  loader_factory.AddEvents(
      {TestLoaderEvent::kReceivedResponse, TestLoaderEvent::kReceivedResponse});
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(GURL("foo://bar/"));
  loader_factory.RunTest(test_helper.get());

  EXPECT_EQ(net::ERR_UNEXPECTED, test_helper->simple_url_loader()->NetError());
  EXPECT_FALSE(test_helper->simple_url_loader()->CompletionStatus());
  EXPECT_EQ(200, test_helper->GetResponseCode());
  EXPECT_FALSE(test_helper->response_body());
}

TEST_P(SimpleURLLoaderTest, RedirectAfterReceivedResponse) {
  MockURLLoaderFactory loader_factory(&task_environment_,
                                      GetURLLoaderFactoryTestConfig());
  loader_factory.AddEvents(
      {TestLoaderEvent::kReceivedResponse, TestLoaderEvent::kReceivedRedirect});
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(GURL("foo://bar/"));
  loader_factory.RunTest(test_helper.get());

  EXPECT_EQ(net::ERR_UNEXPECTED, test_helper->simple_url_loader()->NetError());
  EXPECT_FALSE(test_helper->simple_url_loader()->CompletionStatus());
  EXPECT_EQ(200, test_helper->GetResponseCode());
  EXPECT_FALSE(test_helper->response_body());
}

TEST_P(SimpleURLLoaderTest, DoubleBodyBufferReceived) {
  MockURLLoaderFactory loader_factory(&task_environment_,
                                      GetURLLoaderFactoryTestConfig());
  loader_factory.AddEvents(
      {TestLoaderEvent::kReceivedResponse, TestLoaderEvent::kReceivedResponse});
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(GURL("foo://bar/"));
  loader_factory.RunTest(test_helper.get());

  EXPECT_EQ(net::ERR_UNEXPECTED, test_helper->simple_url_loader()->NetError());
  EXPECT_FALSE(test_helper->simple_url_loader()->CompletionStatus());
  EXPECT_EQ(200, test_helper->GetResponseCode());
  EXPECT_FALSE(test_helper->response_body());
}

TEST_P(SimpleURLLoaderTest, UnexpectedMessageAfterBodyStarts) {
  MockURLLoaderFactory loader_factory(&task_environment_,
                                      GetURLLoaderFactoryTestConfig());
  loader_factory.AddEvents({TestLoaderEvent::kReceivedResponse,
                            TestLoaderEvent::kBodyDataRead,
                            TestLoaderEvent::kReceivedRedirect});
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(GURL("foo://bar/"));
  loader_factory.RunTest(test_helper.get());

  EXPECT_EQ(net::ERR_UNEXPECTED, test_helper->simple_url_loader()->NetError());
  EXPECT_FALSE(test_helper->simple_url_loader()->CompletionStatus());
  EXPECT_EQ(200, test_helper->GetResponseCode());
  EXPECT_FALSE(test_helper->response_body());
}

TEST_P(SimpleURLLoaderTest, UnexpectedMessageAfterBodyStarts2) {
  MockURLLoaderFactory loader_factory(&task_environment_,
                                      GetURLLoaderFactoryTestConfig());
  loader_factory.AddEvents({TestLoaderEvent::kReceivedResponse,
                            TestLoaderEvent::kBodyDataRead,
                            TestLoaderEvent::kReceivedResponse});
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(GURL("foo://bar/"));
  loader_factory.RunTest(test_helper.get());

  EXPECT_EQ(net::ERR_UNEXPECTED, test_helper->simple_url_loader()->NetError());
  EXPECT_FALSE(test_helper->simple_url_loader()->CompletionStatus());
  EXPECT_EQ(200, test_helper->GetResponseCode());
  EXPECT_FALSE(test_helper->response_body());
}

TEST_P(SimpleURLLoaderTest, UnexpectedMessageAfterBodyComplete) {
  MockURLLoaderFactory loader_factory(&task_environment_,
                                      GetURLLoaderFactoryTestConfig());
  loader_factory.AddEvents(
      {TestLoaderEvent::kReceivedResponse, TestLoaderEvent::kBodyDataRead,
       TestLoaderEvent::kBodyBufferClosed, TestLoaderEvent::kReceivedResponse});
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(GURL("foo://bar/"));
  loader_factory.RunTest(test_helper.get());

  EXPECT_EQ(net::ERR_UNEXPECTED, test_helper->simple_url_loader()->NetError());
  EXPECT_FALSE(test_helper->simple_url_loader()->CompletionStatus());
  EXPECT_EQ(200, test_helper->GetResponseCode());
  EXPECT_FALSE(test_helper->response_body());
}

TEST_P(SimpleURLLoaderTest, MoreDataThanExpected) {
  if (GetReadAndDiscardBodyType() == ReadAndDiscardBodyType::kEnabled &&
      IsHeadersOnly()) {
    GTEST_SKIP() << "No data is actually sent in this case.";
  }

  MockURLLoaderFactory loader_factory(&task_environment_,
                                      GetURLLoaderFactoryTestConfig());
  loader_factory.AddEvents(
      {TestLoaderEvent::kReceivedResponse, TestLoaderEvent::kBodyDataRead,
       TestLoaderEvent::kBodyDataRead, TestLoaderEvent::kBodyBufferClosed,
       TestLoaderEvent::kResponseCompleteWithExtraData});
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(GURL("foo://bar/"));
  loader_factory.RunTest(test_helper.get());

  EXPECT_EQ(net::ERR_UNEXPECTED, test_helper->simple_url_loader()->NetError());
  EXPECT_FALSE(test_helper->simple_url_loader()->CompletionStatus());
  EXPECT_EQ(200, test_helper->GetResponseCode());
  EXPECT_FALSE(test_helper->response_body());
}

TEST_P(SimpleURLLoaderTest, DownloadProgressCallbackIncremental) {
  // Make sure that intermediate states of download are reported to the
  // progress callback.
  MockURLLoaderFactory loader_factory(&task_environment_,
                                      GetURLLoaderFactoryTestConfig());
  loader_factory.AddEvents({TestLoaderEvent::kReceivedResponse,
                            TestLoaderEvent::kBodyDataRead,
                            TestLoaderEvent::kBodyDataRead});
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(GURL("foo://bar/"));

  std::vector<uint64_t> progress;
  test_helper->simple_url_loader()->SetOnDownloadProgressCallback(
      base::BindLambdaForTesting(
          [&](uint64_t current) { progress.push_back(current); }));
  loader_factory.RunTest(test_helper.get(), false);

  if (IsHeadersOnly()) {
    EXPECT_EQ(0u, progress.size());
  } else {
    // Since the request doesn't complete this is guaranteed to receive
    // all the in-progress events, as there is no risk of completion happening
    // beforehand and cancelling them.
    ASSERT_EQ(2u, progress.size());
    EXPECT_EQ(1u, progress[0]);
    EXPECT_EQ(2u, progress[1]);
  }

  // Clean the file up.
  test_helper->DestroySimpleURLLoader();
  task_environment_.RunUntilIdle();
}

TEST_P(SimpleURLLoaderTest, RetryOn5xx) {
  const GURL kInitialURL("foo://bar/initial");
  struct TestCase {
    // Parameters passed to SetRetryOptions.
    int max_retries;
    int retry_mode;

    // Number of 5xx responses before a successful response.
    int num_5xx;

    // Whether the request is expected to succeed in the end.
    bool expect_success;

    // Expected times the url should be requested.
    int expected_num_requests;
  } const kTestCases[] = {
      // No retry on 5xx when retries disabled.
      {0, SimpleURLLoader::RETRY_NEVER, 1, false, 1},

      // No retry on 5xx when retries enabled on network change.
      {1, SimpleURLLoader::RETRY_ON_NETWORK_CHANGE, 1, false, 1},

      // As many retries allowed as 5xx errors.
      {1, SimpleURLLoader::RETRY_ON_5XX, 1, true, 2},
      {1,
       SimpleURLLoader::RETRY_ON_5XX | SimpleURLLoader::RETRY_ON_NETWORK_CHANGE,
       1, true, 2},
      {2, SimpleURLLoader::RETRY_ON_5XX, 2, true, 3},

      // More retries than 5xx errors.
      {2, SimpleURLLoader::RETRY_ON_5XX, 1, true, 2},

      // Fewer retries than 5xx errors.
      {1, SimpleURLLoader::RETRY_ON_5XX, 2, false, 2},
  };

  for (const auto& test_case : kTestCases) {
    MockURLLoaderFactory loader_factory(&task_environment_,
                                        GetURLLoaderFactoryTestConfig());
    for (int i = 0; i < test_case.num_5xx; i++) {
      loader_factory.AddEvents({TestLoaderEvent::kReceived501Response});
    }

    if (test_case.expect_success) {
      // Valid response with a 1-byte body.
      loader_factory.AddEvents({TestLoaderEvent::kReceivedResponse,
                                TestLoaderEvent::kBodyDataRead,
                                TestLoaderEvent::kBodyBufferClosed,
                                TestLoaderEvent::kResponseComplete});
    }

    std::unique_ptr<SimpleLoaderTestHelper> test_helper =
        CreateHelperForURL(GURL(kInitialURL));
    test_helper->simple_url_loader()->SetRetryOptions(test_case.max_retries,
                                                      test_case.retry_mode);
    loader_factory.RunTest(test_helper.get());

    if (test_case.expect_success) {
      EXPECT_EQ(net::OK, test_helper->simple_url_loader()->NetError());
      ASSERT_TRUE(test_helper->simple_url_loader()->CompletionStatus());
      EXPECT_EQ(
          net::OK,
          test_helper->simple_url_loader()->CompletionStatus()->error_code);
      EXPECT_EQ(200, test_helper->GetResponseCode());

      if (!IsHeadersOnly()) {
        ASSERT_TRUE(test_helper->response_body());
        EXPECT_EQ(1u, test_helper->response_body()->size());
      }
    } else {
      EXPECT_EQ(501, test_helper->GetResponseCode());
      EXPECT_FALSE(test_helper->response_body());
    }

    EXPECT_EQ(static_cast<size_t>(test_case.expected_num_requests),
              loader_factory.requested_urls().size());
    for (const auto& url : loader_factory.requested_urls()) {
      EXPECT_EQ(kInitialURL, url);
    }

    if (GetDownloadType() == SimpleLoaderTestHelper::DownloadType::AS_STREAM) {
      EXPECT_EQ(test_case.expected_num_requests - 1,
                test_helper->download_as_stream_retries());
    }

    EXPECT_EQ(test_case.expected_num_requests - 1,
              test_helper->simple_url_loader()->GetNumRetries());
  }
}

TEST_P(SimpleURLLoaderTest, RetryOnNameNotResolved) {
  const GURL kInitialURL("foo://bar/initial");
  struct TestCase {
    // Parameters passed to SetRetryOptions.
    int max_retries;
    int retry_mode;

    // Number of resolution errors before a successful response.
    int num_name_not_resolved;

    // Whether the request is expected to succeed in the end.
    bool expect_success;

    // Expected times the url should be requested.
    int expected_num_requests;
  } const kTestCases[] = {
      // No retry when retries disabled.
      {0, SimpleURLLoader::RETRY_NEVER, 1, false, 1},

      // No retry on name resolution when retries enabled on network change.
      {1, SimpleURLLoader::RETRY_ON_NETWORK_CHANGE, 1, false, 1},

      // As many retries allowed as resolution errors.
      {1, SimpleURLLoader::RETRY_ON_NAME_NOT_RESOLVED, 1, true, 2},
      {1,
       SimpleURLLoader::RETRY_ON_NAME_NOT_RESOLVED |
           SimpleURLLoader::RETRY_ON_NETWORK_CHANGE,
       1, true, 2},
      {2, SimpleURLLoader::RETRY_ON_NAME_NOT_RESOLVED, 2, true, 3},

      // More retries than resolution errors.
      {2, SimpleURLLoader::RETRY_ON_NAME_NOT_RESOLVED, 1, true, 2},

      // Fewer retries than resolution errors.
      {1, SimpleURLLoader::RETRY_ON_NAME_NOT_RESOLVED, 2, false, 2},
  };

  for (const auto& test_case : kTestCases) {
    MockURLLoaderFactory loader_factory(&task_environment_,
                                        GetURLLoaderFactoryTestConfig());
    for (int i = 0; i < test_case.num_name_not_resolved; i++) {
      loader_factory.AddEvents({TestLoaderEvent::kNameNotResolved});
    }

    if (test_case.expect_success) {
      // Valid response with a 1-byte body.
      loader_factory.AddEvents({TestLoaderEvent::kReceivedResponse,
                                TestLoaderEvent::kBodyDataRead,
                                TestLoaderEvent::kBodyBufferClosed,
                                TestLoaderEvent::kResponseComplete});
    }

    std::unique_ptr<SimpleLoaderTestHelper> test_helper =
        CreateHelperForURL(GURL(kInitialURL));
    test_helper->simple_url_loader()->SetRetryOptions(test_case.max_retries,
                                                      test_case.retry_mode);
    loader_factory.RunTest(test_helper.get());

    if (test_case.expect_success) {
      EXPECT_EQ(net::OK, test_helper->simple_url_loader()->NetError());
      ASSERT_TRUE(test_helper->simple_url_loader()->CompletionStatus());
      EXPECT_EQ(
          net::OK,
          test_helper->simple_url_loader()->CompletionStatus()->error_code);
      EXPECT_EQ(200, test_helper->GetResponseCode());

      if (!IsHeadersOnly()) {
        ASSERT_TRUE(test_helper->response_body());
        EXPECT_EQ(1u, test_helper->response_body()->size());
      }
    } else {
      EXPECT_EQ(net::ERR_NAME_NOT_RESOLVED,
                test_helper->simple_url_loader()->NetError());
      ASSERT_TRUE(test_helper->simple_url_loader()->CompletionStatus());
      EXPECT_EQ(
          net::ERR_NAME_NOT_RESOLVED,
          test_helper->simple_url_loader()->CompletionStatus()->error_code);
    }

    EXPECT_EQ(static_cast<size_t>(test_case.expected_num_requests),
              loader_factory.requested_urls().size());
    for (const auto& url : loader_factory.requested_urls()) {
      EXPECT_EQ(kInitialURL, url);
    }

    if (GetDownloadType() == SimpleLoaderTestHelper::DownloadType::AS_STREAM) {
      EXPECT_EQ(test_case.expected_num_requests - 1,
                test_helper->download_as_stream_retries());
    }

    EXPECT_EQ(test_case.expected_num_requests - 1,
              test_helper->simple_url_loader()->GetNumRetries());
  }
}

// Test that when retrying on 5xx is enabled, there's no retry on a 4xx error.
TEST_P(SimpleURLLoaderTest, NoRetryOn4xx) {
  MockURLLoaderFactory loader_factory(&task_environment_,
                                      GetURLLoaderFactoryTestConfig());
  loader_factory.AddEvents({TestLoaderEvent::kReceived401Response});

  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(GURL("foo://bar/"));
  test_helper->simple_url_loader()->SetRetryOptions(
      1, SimpleURLLoader::RETRY_ON_5XX);
  loader_factory.RunTest(test_helper.get());

  EXPECT_EQ(401, test_helper->GetResponseCode());
  EXPECT_FALSE(test_helper->response_body());
  EXPECT_EQ(1u, loader_factory.requested_urls().size());

  if (GetDownloadType() == SimpleLoaderTestHelper::DownloadType::AS_STREAM) {
    EXPECT_EQ(0, test_helper->download_as_stream_retries());
  }
}

// Checks that retrying after a redirect works. The original URL should be
// re-requested.
TEST_P(SimpleURLLoaderTest, RetryAfterRedirect) {
  const GURL kInitialURL("foo://bar/initial");
  MockURLLoaderFactory loader_factory(&task_environment_,
                                      GetURLLoaderFactoryTestConfig());
  loader_factory.AddEvents({TestLoaderEvent::kReceivedRedirect,
                            TestLoaderEvent::kReceived501Response});
  loader_factory.AddEvents(
      {TestLoaderEvent::kReceivedRedirect, TestLoaderEvent::kReceivedResponse,
       TestLoaderEvent::kBodyBufferClosed, TestLoaderEvent::kResponseComplete});

  int num_redirects = 0;
  std::vector<GURL> urls_before_redirect;

  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(kInitialURL);
  test_helper->simple_url_loader()->SetRetryOptions(
      1, SimpleURLLoader::RETRY_ON_5XX);
  test_helper->simple_url_loader()->SetOnRedirectCallback(base::BindRepeating(
      [](int* num_redirects, std::vector<GURL>* urls_before_redirect,
         const GURL& url_before_redirect,
         const net::RedirectInfo& redirect_info,
         const network::mojom::URLResponseHead& response_head,
         std::vector<std::string>* to_be_removed_headers) {
        ++*num_redirects;
        urls_before_redirect->push_back(url_before_redirect);
      },
      base::Unretained(&num_redirects),
      base::Unretained(&urls_before_redirect)));
  loader_factory.RunTest(test_helper.get());

  EXPECT_EQ(200, test_helper->GetResponseCode());
  if (!IsHeadersOnly()) {
    EXPECT_TRUE(test_helper->response_body());
  }
  EXPECT_EQ(2, num_redirects);

  EXPECT_EQ(2u, loader_factory.requested_urls().size());
  for (const auto& url : loader_factory.requested_urls()) {
    EXPECT_EQ(kInitialURL, url);
  }

  EXPECT_THAT(urls_before_redirect, ElementsAre(kInitialURL, kInitialURL));

  if (GetDownloadType() == SimpleLoaderTestHelper::DownloadType::AS_STREAM) {
    EXPECT_EQ(1, test_helper->download_as_stream_retries());
  }
}

TEST_P(SimpleURLLoaderTest, RetryOnNetworkChange) {
  // TestLoaderEvents up to (and including) a network change. Since
  // SimpleURLLoader always waits for the body buffer to be closed before
  // retrying, everything that has a KReceiveResponse message must also have
  // a kBodyBufferClosed message. Each test case will be tried against each of
  // these event sequences.
  const std::vector<std::vector<TestLoaderEvent>> kNetworkChangedEvents = {
      {TestLoaderEvent::kResponseCompleteNetworkChanged},
      {TestLoaderEvent::kReceivedResponseNoData,
       TestLoaderEvent::kResponseCompleteNetworkChanged},
      {TestLoaderEvent::kReceivedResponse, TestLoaderEvent::kBodyBufferClosed,
       TestLoaderEvent::kResponseCompleteNetworkChanged},
      {TestLoaderEvent::kReceivedResponse,
       TestLoaderEvent::kResponseCompleteNetworkChanged,
       TestLoaderEvent::kBodyBufferClosed},
      {TestLoaderEvent::kReceivedResponse, TestLoaderEvent::kBodyDataRead,
       TestLoaderEvent::kBodyBufferClosed,
       TestLoaderEvent::kResponseCompleteNetworkChanged},
      {TestLoaderEvent::kReceivedResponse, TestLoaderEvent::kBodyDataRead,
       TestLoaderEvent::kResponseCompleteNetworkChanged,
       TestLoaderEvent::kBodyBufferClosed},
      {TestLoaderEvent::kReceivedRedirect,
       TestLoaderEvent::kResponseCompleteNetworkChanged},
  };

  const GURL kInitialURL("foo://bar/initial");

  // Test cases in which to try each entry in kNetworkChangedEvents.
  struct TestCase {
    // Parameters passed to SetRetryOptions.
    int max_retries;
    int retry_mode;

    // Number of network changes responses before a successful response.
    // For each network change, the entire sequence of an entry in
    // kNetworkChangedEvents is repeated.
    int num_network_changes;

    // Whether the request is expected to succeed in the end.
    bool expect_success;

    // Expected times the url should be requested.
    int expected_num_requests;
  } const kTestCases[] = {
      // No retry on network change when retries disabled.
      {0, SimpleURLLoader::RETRY_NEVER, 1, false, 1},

      // No retry on network change when retries enabled on 5xx response.
      {1, SimpleURLLoader::RETRY_ON_5XX, 1, false, 1},

      // As many retries allowed as network changes.
      {1, SimpleURLLoader::RETRY_ON_NETWORK_CHANGE, 1, true, 2},
      {1,
       SimpleURLLoader::RETRY_ON_NETWORK_CHANGE | SimpleURLLoader::RETRY_ON_5XX,
       1, true, 2},
      {2, SimpleURLLoader::RETRY_ON_NETWORK_CHANGE, 2, true, 3},

      // More retries than network changes.
      {2, SimpleURLLoader::RETRY_ON_NETWORK_CHANGE, 1, true, 2},

      // Fewer retries than network changes.
      {1, SimpleURLLoader::RETRY_ON_NETWORK_CHANGE, 2, false, 2},
  };

  for (const auto& network_events : kNetworkChangedEvents) {
    for (const auto& test_case : kTestCases) {
      MockURLLoaderFactory loader_factory(&task_environment_,
                                          GetURLLoaderFactoryTestConfig());
      for (int i = 0; i < test_case.num_network_changes; i++) {
        loader_factory.AddEvents(network_events);
      }

      if (test_case.expect_success) {
        // Valid response with a 1-byte body.
        loader_factory.AddEvents({TestLoaderEvent::kReceivedResponse,
                                  TestLoaderEvent::kBodyDataRead,
                                  TestLoaderEvent::kBodyBufferClosed,
                                  TestLoaderEvent::kResponseComplete});
      }

      std::unique_ptr<SimpleLoaderTestHelper> test_helper =
          CreateHelperForURL(GURL(kInitialURL));
      test_helper->simple_url_loader()->SetRetryOptions(test_case.max_retries,
                                                        test_case.retry_mode);
      loader_factory.RunTest(test_helper.get());

      if (test_case.expect_success) {
        EXPECT_EQ(net::OK, test_helper->simple_url_loader()->NetError());
        ASSERT_TRUE(test_helper->simple_url_loader()->CompletionStatus());
        EXPECT_EQ(
            net::OK,
            test_helper->simple_url_loader()->CompletionStatus()->error_code);
        EXPECT_EQ(200, test_helper->GetResponseCode());

        if (!IsHeadersOnly()) {
          ASSERT_TRUE(test_helper->response_body());
          EXPECT_EQ(1u, test_helper->response_body()->size());
        }
      } else {
        EXPECT_EQ(net::ERR_NETWORK_CHANGED,
                  test_helper->simple_url_loader()->NetError());
        ASSERT_TRUE(test_helper->simple_url_loader()->CompletionStatus());
        EXPECT_EQ(
            net::ERR_NETWORK_CHANGED,
            test_helper->simple_url_loader()->CompletionStatus()->error_code);
        EXPECT_FALSE(test_helper->response_body());
      }

      EXPECT_EQ(static_cast<size_t>(test_case.expected_num_requests),
                loader_factory.requested_urls().size());
      for (const auto& url : loader_factory.requested_urls()) {
        EXPECT_EQ(kInitialURL, url);
      }

      if (GetDownloadType() ==
          SimpleLoaderTestHelper::DownloadType::AS_STREAM) {
        EXPECT_EQ(test_case.expected_num_requests - 1,
                  test_helper->download_as_stream_retries());
      }

      EXPECT_EQ(test_case.expected_num_requests - 1,
                test_helper->simple_url_loader()->GetNumRetries());
    }
  }

  // Check that there's no retry for each entry in kNetworkChangedEvents when an
  // error other than a network change is received.
  for (const auto& network_events : kNetworkChangedEvents) {
    std::vector<TestLoaderEvent> modifed_network_events = network_events;
    for (auto& test_loader_event : modifed_network_events) {
      if (test_loader_event == TestLoaderEvent::kResponseCompleteNetworkChanged)
        test_loader_event = TestLoaderEvent::kResponseCompleteFailed;
    }
    MockURLLoaderFactory loader_factory(&task_environment_,
                                        GetURLLoaderFactoryTestConfig());
    loader_factory.AddEvents(modifed_network_events);

    std::unique_ptr<SimpleLoaderTestHelper> test_helper =
        CreateHelperForURL(GURL(kInitialURL));
    test_helper->simple_url_loader()->SetRetryOptions(
        1, SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);
    loader_factory.RunTest(test_helper.get());

    EXPECT_EQ(net::ERR_TIMED_OUT, test_helper->simple_url_loader()->NetError());
    ASSERT_TRUE(test_helper->simple_url_loader()->CompletionStatus());
    EXPECT_EQ(net::ERR_TIMED_OUT,
              test_helper->simple_url_loader()->CompletionStatus()->error_code);
    EXPECT_FALSE(test_helper->response_body());
    EXPECT_EQ(1u, loader_factory.requested_urls().size());

    if (GetDownloadType() == SimpleLoaderTestHelper::DownloadType::AS_STREAM) {
      EXPECT_EQ(0, test_helper->download_as_stream_retries());
    }
  }
}

// Check the case where the URLLoaderFactory has been disconnected before the
// request is retried.
TEST_P(SimpleURLLoaderTest, RetryWithUnboundFactory) {
  MockURLLoaderFactory loader_factory(&task_environment_,
                                      GetURLLoaderFactoryTestConfig());
  loader_factory.AddEvents({TestLoaderEvent::kResponseCompleteNetworkChanged});
  // Since clone fails asynchronously, this shouldn't be any different from
  // the new URLLoaderFactory being disconnected in some other way.
  loader_factory.set_close_new_binding_on_clone(true);

  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(GURL("foo://bar/"));
  test_helper->simple_url_loader()->SetRetryOptions(
      1, SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);
  loader_factory.RunTest(test_helper.get());
  EXPECT_EQ(net::ERR_FAILED, test_helper->simple_url_loader()->NetError());
  EXPECT_FALSE(test_helper->simple_url_loader()->CompletionStatus());
  EXPECT_FALSE(test_helper->response_body());

  // Retry is still called, before the dead factory is discovered.
  if (GetDownloadType() == SimpleLoaderTestHelper::DownloadType::AS_STREAM) {
    EXPECT_EQ(1, test_helper->download_as_stream_retries());
  }
}

// Test the case where DataPipeGetter::Read is called twice in a row,
// with no intervening reads of the data on the pipe.
TEST_P(SimpleURLLoaderTest, UploadLongStringStartReadTwice) {
  std::string long_string = GetLongUploadBody();
  MockURLLoaderFactory loader_factory(&task_environment_,
                                      GetURLLoaderFactoryTestConfig());
  loader_factory.AddEvents(
      {TestLoaderEvent::kStartReadLongUploadBody,
       TestLoaderEvent::kStartReadLongUploadBody,
       TestLoaderEvent::kWaitForLongUploadBodySize,
       TestLoaderEvent::kReadLongUploadBody, TestLoaderEvent::kReceivedResponse,
       TestLoaderEvent::kResponseComplete, TestLoaderEvent::kBodyBufferClosed});
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(GURL("foo://bar/"), "POST");
  test_helper->simple_url_loader()->AttachStringForUpload(long_string,
                                                          "text/plain");
  loader_factory.RunTest(test_helper.get());

  EXPECT_EQ(net::OK, test_helper->simple_url_loader()->NetError());
  ASSERT_TRUE(test_helper->simple_url_loader()->CompletionStatus());
  EXPECT_EQ(net::OK,
            test_helper->simple_url_loader()->CompletionStatus()->error_code);
  if (!IsHeadersOnly()) {
    ASSERT_TRUE(test_helper->response_body());
    EXPECT_EQ("", *test_helper->response_body());
  }
}

// Test the case where DataPipeGetter::Read is called a second time, after only
// reading part of the response, with no intervening reads of the data on the
// pipe.
TEST_P(SimpleURLLoaderTest,
       UploadLongStringReadPartOfUploadBodyBeforeRestartBodyRead) {
  std::string long_string = GetLongUploadBody();
  MockURLLoaderFactory loader_factory(&task_environment_,
                                      GetURLLoaderFactoryTestConfig());
  loader_factory.AddEvents(
      {TestLoaderEvent::kStartReadLongUploadBody,
       TestLoaderEvent::kWaitForLongUploadBodySize,
       TestLoaderEvent::kReadFirstByteOfLongUploadBody,
       TestLoaderEvent::kStartReadLongUploadBody,
       TestLoaderEvent::kWaitForLongUploadBodySize,
       TestLoaderEvent::kReadLongUploadBody, TestLoaderEvent::kReceivedResponse,
       TestLoaderEvent::kResponseComplete, TestLoaderEvent::kBodyBufferClosed});
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(GURL("foo://bar/"), "POST");
  test_helper->simple_url_loader()->AttachStringForUpload(long_string,
                                                          "text/plain");
  loader_factory.RunTest(test_helper.get());

  EXPECT_EQ(net::OK, test_helper->simple_url_loader()->NetError());
  ASSERT_TRUE(test_helper->simple_url_loader()->CompletionStatus());
  EXPECT_EQ(net::OK,
            test_helper->simple_url_loader()->CompletionStatus()->error_code);

  if (!IsHeadersOnly()) {
    ASSERT_TRUE(test_helper->response_body());
    EXPECT_EQ("", *test_helper->response_body());
  }
}

// Test for GetFinalURL.
TEST_P(SimpleURLLoaderTest, GetFinalURL) {
  GURL url = test_server_.GetURL("/echo");
  std::unique_ptr<network::ResourceRequest> resource_request =
      std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelper(std::move(resource_request));
  test_helper->StartSimpleLoaderAndWait(url_loader_factory_.get());

  EXPECT_EQ(net::OK, test_helper->simple_url_loader()->NetError());
  ASSERT_TRUE(test_helper->simple_url_loader()->CompletionStatus());
  EXPECT_EQ(net::OK,
            test_helper->simple_url_loader()->CompletionStatus()->error_code);
  EXPECT_EQ(url, test_helper->simple_url_loader()->GetFinalURL());
}

// Test for GetFinalURL with a redirect.
TEST_P(SimpleURLLoaderTest, GetFinalURLAfterRedirect) {
  GURL url = test_server_.GetURL("/echo");
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(test_server_.GetURL("/server-redirect?" + url.spec()));
  test_helper->StartSimpleLoaderAndWait(url_loader_factory_.get());

  EXPECT_EQ(net::OK, test_helper->simple_url_loader()->NetError());
  ASSERT_TRUE(test_helper->simple_url_loader()->CompletionStatus());
  EXPECT_EQ(net::OK,
            test_helper->simple_url_loader()->CompletionStatus()->error_code);
  EXPECT_EQ(url, test_helper->simple_url_loader()->GetFinalURL());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SimpleURLLoaderTest,
    testing::Combine(
        testing::Values(SimpleLoaderTestHelper::DownloadType::TO_STRING,
                        SimpleLoaderTestHelper::DownloadType::TO_FILE,
                        SimpleLoaderTestHelper::DownloadType::TO_TEMP_FILE,
                        SimpleLoaderTestHelper::DownloadType::HEADERS_ONLY,
                        SimpleLoaderTestHelper::DownloadType::AS_STREAM),
        testing::Values(ReadAndDiscardBodyType::kDisabled,
                        ReadAndDiscardBodyType::kEnabled,
                        ReadAndDiscardBodyType::kEnabledButIgnored)),
    ::testing::PrintToStringParamName());

class SimpleURLLoaderFileTest : public SimpleURLLoaderTestBase,
                                public testing::Test {
 public:
  SimpleURLLoaderFileTest() = default;
  ~SimpleURLLoaderFileTest() override = default;

  std::unique_ptr<SimpleLoaderTestHelper> CreateHelperForURL(const GURL& url) {
    std::unique_ptr<network::ResourceRequest> resource_request =
        std::make_unique<network::ResourceRequest>();
    resource_request->url = url;
    return std::make_unique<SimpleLoaderTestHelper>(
        std::move(resource_request),
        SimpleLoaderTestHelper::DownloadType::TO_FILE);
  }
};

// Make sure that an existing file will be completely overwritten.
TEST_F(SimpleURLLoaderFileTest, OverwriteFile) {
  std::string junk_data(100, '!');
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(test_server_.GetURL("/echo"));
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(base::WriteFile(test_helper->dest_path(), junk_data));
    ASSERT_TRUE(base::PathExists(test_helper->dest_path()));
  }

  test_helper->StartSimpleLoaderAndWait(url_loader_factory_.get());

  EXPECT_EQ(net::OK, test_helper->simple_url_loader()->NetError());
  ASSERT_TRUE(test_helper->simple_url_loader()->CompletionStatus());
  EXPECT_EQ(net::OK,
            test_helper->simple_url_loader()->CompletionStatus()->error_code);
  ASSERT_TRUE(test_helper->simple_url_loader()->ResponseInfo());
  ASSERT_TRUE(test_helper->response_body());
  EXPECT_EQ("Echo", *test_helper->response_body());
}

// Make sure that file creation errors are handled correctly.
TEST_F(SimpleURLLoaderFileTest, FileCreateError) {
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(test_server_.GetURL("/echo"));
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::CreateDirectory(test_helper->dest_path()));
    ASSERT_TRUE(base::PathExists(test_helper->dest_path()));
  }

  // The directory should still exist after the download fails.
  test_helper->set_expect_path_exists_on_error(true);
  test_helper->StartSimpleLoaderAndWait(url_loader_factory_.get());

  EXPECT_EQ(net::ERR_ACCESS_DENIED,
            test_helper->simple_url_loader()->NetError());
  EXPECT_FALSE(test_helper->simple_url_loader()->CompletionStatus());
  EXPECT_TRUE(test_helper->simple_url_loader()->ResponseInfo());
  EXPECT_FALSE(test_helper->response_body());
}

// Make sure that destroying the loader destroys a partially downloaded file.
TEST_F(SimpleURLLoaderFileTest, DeleteLoaderDuringRequestDestroysFile) {
  for (bool body_data_read : {false, true}) {
    for (bool body_buffer_closed : {false, true}) {
      for (bool client_pipe_closed : {false, true}) {
        // If both pipes were closed cleanly, the file shouldn't be deleted, as
        // that indicates success.
        if (body_buffer_closed && client_pipe_closed)
          continue;

        MockURLLoaderFactory loader_factory(
            &task_environment_,
            {ReadAndDiscardBodyType::kEnabledButIgnored, false});
        std::vector<TestLoaderEvent> events;
        events.push_back(TestLoaderEvent::kReceivedResponse);
        if (body_data_read)
          events.push_back(TestLoaderEvent::kBodyDataRead);
        if (body_buffer_closed)
          events.push_back(TestLoaderEvent::kBodyBufferClosed);
        if (client_pipe_closed)
          events.push_back(TestLoaderEvent::kClientPipeClosed);
        loader_factory.AddEvents(events);

        std::unique_ptr<SimpleLoaderTestHelper> test_helper =
            CreateHelperForURL(GURL("foo://bar/"));

        // Run events without waiting for the request to complete, since the
        // request will hang.
        loader_factory.RunTest(test_helper.get(),
                               false /* wait_for_completion */);

        // Wait for the request to advance as far as it's going to.
        task_environment_.RunUntilIdle();

        // Destination file should have been created, and request should still
        // be in progress.
        base::FilePath dest_path = test_helper->dest_path();
        {
          base::ScopedAllowBlockingForTesting allow_blocking;
          EXPECT_TRUE(base::PathExists(dest_path));
          EXPECT_FALSE(test_helper->done());
        }

        // Destroying the SimpleURLLoader now should post a task to destroy the
        // file.
        test_helper->DestroySimpleURLLoader();
        task_environment_.RunUntilIdle();
        {
          base::ScopedAllowBlockingForTesting allow_blocking;
          EXPECT_FALSE(base::PathExists(dest_path));
        }
      }
    }
  }
}

// Used for testing stream-specific features.
class SimpleURLLoaderStreamTest : public SimpleURLLoaderTestBase,
                                  public testing::Test {
 public:
  SimpleURLLoaderStreamTest() {}
  ~SimpleURLLoaderStreamTest() override {}

  std::unique_ptr<SimpleLoaderTestHelper> CreateHelperForURL(
      const GURL& url,
      const std::string& method = "GET") {
    std::unique_ptr<network::ResourceRequest> resource_request =
        std::make_unique<network::ResourceRequest>();
    resource_request->url = url;
    resource_request->method = method;
    return std::make_unique<SimpleLoaderTestHelper>(
        std::move(resource_request),
        SimpleLoaderTestHelper::DownloadType::AS_STREAM);
  }
};

TEST_F(SimpleURLLoaderStreamTest, OnDataReceivedCompletesAsync) {
  const uint32_t kResponseSize = 2 * 1024 * 1024;
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(test_server_.GetURL(
          base::StringPrintf("/response-size?%u", kResponseSize)));
  test_helper->set_download_to_stream_async_resume(true);
  test_helper->StartSimpleLoaderAndWait(url_loader_factory_.get());

  EXPECT_EQ(net::OK, test_helper->simple_url_loader()->NetError());
  ASSERT_TRUE(test_helper->simple_url_loader()->CompletionStatus());
  EXPECT_EQ(net::OK,
            test_helper->simple_url_loader()->CompletionStatus()->error_code);
  EXPECT_EQ(200, test_helper->GetResponseCode());
  ASSERT_TRUE(test_helper->response_body());
  EXPECT_EQ(kResponseSize, test_helper->response_body()->length());
  EXPECT_EQ(std::string(kResponseSize, 'a'), *test_helper->response_body());
  EXPECT_EQ(0, test_helper->download_as_stream_retries());
}

// Test case where class is destroyed during OnDataReceived. Main purpose is to
// make sure there's not a crash.
TEST_F(SimpleURLLoaderStreamTest, OnDataReceivedDestruction) {
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(test_server_.GetURL("/response-size?1"));
  test_helper->set_download_to_stream_destroy_on_data_received(true);
  test_helper->StartSimpleLoaderAndWait(url_loader_factory_.get());

  EXPECT_FALSE(test_helper->simple_url_loader());
  // Make sure no pending task results in a crash.
  base::RunLoop().RunUntilIdle();
}

TEST_F(SimpleURLLoaderStreamTest, OnRetryCompletesAsync) {
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(test_server_.GetURL(kFailOnceThenEchoBody), "POST");
  test_helper->simple_url_loader()->AttachStringForUpload(kShortUploadBody,
                                                          "text/plain");
  test_helper->simple_url_loader()->SetRetryOptions(
      1, SimpleURLLoader::RETRY_ON_5XX);
  test_helper->StartSimpleLoaderAndWait(url_loader_factory_.get());
  EXPECT_EQ(net::OK, test_helper->simple_url_loader()->NetError());
  ASSERT_TRUE(test_helper->simple_url_loader()->CompletionStatus());
  EXPECT_EQ(net::OK,
            test_helper->simple_url_loader()->CompletionStatus()->error_code);
  ASSERT_TRUE(test_helper->response_body());
  EXPECT_EQ(kShortUploadBody, *test_helper->response_body());
  EXPECT_EQ(1, test_helper->download_as_stream_retries());
}

// Test case where class is destroyed during OnRetry. While setting the loader
// to retry and then destroying it on retry is perhaps a bit strange, seems best
// to be consistent in the provided API. Main purpose of this test is to make
// sure there's not a crash.
TEST_F(SimpleURLLoaderStreamTest, OnRetryDestruction) {
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(test_server_.GetURL(kFailOnceThenEchoBody), "POST");
  test_helper->simple_url_loader()->AttachStringForUpload(kShortUploadBody,
                                                          "text/plain");
  test_helper->set_download_to_stream_destroy_on_retry(true);
  test_helper->simple_url_loader()->SetRetryOptions(
      1, SimpleURLLoader::RETRY_ON_5XX);
  test_helper->StartSimpleLoaderAndWait(url_loader_factory_.get());

  EXPECT_FALSE(test_helper->simple_url_loader());
  // Make sure no pending task results in a crash.
  base::RunLoop().RunUntilIdle();
}

// Don't inherit from SimpleURLLoaderTestBase so that we can initialize our
// |task_environment_| different namely with TimeSource::MOCK_TIME.
class SimpleURLLoaderMockTimeTest : public testing::Test {
 public:
  SimpleURLLoaderMockTimeTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        disallow_blocking_(std::make_unique<base::ScopedDisallowBlocking>()) {}
  ~SimpleURLLoaderMockTimeTest() override = default;

  void SetUp() override {
    SimpleURLLoader::SetTimeoutTickClockForTest(
        task_environment_.GetMockTickClock());
  }

  void TearDown() override {
    SimpleURLLoader::SetTimeoutTickClockForTest(nullptr);
  }

  std::unique_ptr<SimpleLoaderTestHelper> CreateHelper() {
    std::unique_ptr<network::ResourceRequest> resource_request =
        std::make_unique<network::ResourceRequest>();
    resource_request->url = GURL("foo://bar/");
    resource_request->method = "GET";
    resource_request->enable_upload_progress = true;
    return std::make_unique<SimpleLoaderTestHelper>(
        std::move(resource_request),
        SimpleLoaderTestHelper::DownloadType::TO_STRING);
  }

  std::unique_ptr<SimpleLoaderTestHelper> CreateStreamHelper() {
    std::unique_ptr<network::ResourceRequest> resource_request =
        std::make_unique<network::ResourceRequest>();
    resource_request->url = GURL("foo://bar/");
    resource_request->method = "GET";
    resource_request->enable_upload_progress = true;
    return std::make_unique<SimpleLoaderTestHelper>(
        std::move(resource_request),
        SimpleLoaderTestHelper::DownloadType::AS_STREAM);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<base::ScopedDisallowBlocking> disallow_blocking_;
};

// The amount of time that's simulated passing is equal to the timeout value
// specified, so the request should fail.
TEST_F(SimpleURLLoaderMockTimeTest, TimeoutTriggered) {
  MockURLLoaderFactory loader_factory(
      &task_environment_, {ReadAndDiscardBodyType::kEnabled, false});
  loader_factory.AddEvents(
      {TestLoaderEvent::kAdvanceOneSecond, TestLoaderEvent::kReceivedResponse,
       TestLoaderEvent::kResponseComplete, TestLoaderEvent::kBodyBufferClosed});
  std::unique_ptr<SimpleLoaderTestHelper> test_helper = CreateHelper();
  test_helper->simple_url_loader()->SetTimeoutDuration(base::Seconds(1));

  loader_factory.RunTest(test_helper.get());

  EXPECT_EQ(net::ERR_TIMED_OUT, test_helper->simple_url_loader()->NetError());
  EXPECT_FALSE(test_helper->simple_url_loader()->CompletionStatus());
}

// Request fails with a timeout like in TimeoutTriggered, and the stream resume
// closure is called after the timeout. The loader is alive throughout.
TEST_F(SimpleURLLoaderMockTimeTest, StreamResumeAfterTimeout) {
  MockURLLoaderFactory loader_factory(
      &task_environment_, {ReadAndDiscardBodyType::kEnabled, false});
  loader_factory.AddEvents(
      {TestLoaderEvent::kReceivedResponse, TestLoaderEvent::kBodyDataRead,
       TestLoaderEvent::kAdvanceOneSecond, TestLoaderEvent::kResponseComplete,
       TestLoaderEvent::kBodyBufferClosed});
  std::unique_ptr<SimpleLoaderTestHelper> test_helper = CreateStreamHelper();
  test_helper->simple_url_loader()->SetTimeoutDuration(base::Seconds(1));
  test_helper->set_download_to_stream_capture_resume(true);

  loader_factory.RunTest(test_helper.get());

  EXPECT_EQ(net::ERR_TIMED_OUT, test_helper->simple_url_loader()->NetError());
  EXPECT_FALSE(test_helper->simple_url_loader()->CompletionStatus());

  base::OnceClosure captured_resume = test_helper->TakeCapturedStreamResume();
  ASSERT_TRUE(captured_resume);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, std::move(captured_resume));
  // Make sure no pending task results in a crash.
  base::RunLoop().RunUntilIdle();
}

// Less time is simulated passing than the timeout value, so this request should
// succeed normally.
TEST_F(SimpleURLLoaderMockTimeTest, TimeoutNotTriggered) {
  MockURLLoaderFactory loader_factory(
      &task_environment_, {ReadAndDiscardBodyType::kEnabled, false});
  loader_factory.AddEvents(
      {TestLoaderEvent::kAdvanceOneSecond, TestLoaderEvent::kReceivedResponse,
       TestLoaderEvent::kResponseComplete, TestLoaderEvent::kBodyBufferClosed});
  std::unique_ptr<SimpleLoaderTestHelper> test_helper = CreateHelper();
  test_helper->simple_url_loader()->SetTimeoutDuration(base::Seconds(2));

  loader_factory.RunTest(test_helper.get());

  EXPECT_EQ(net::OK, test_helper->simple_url_loader()->NetError());
  ASSERT_TRUE(test_helper->simple_url_loader()->CompletionStatus());
  EXPECT_EQ(net::OK,
            test_helper->simple_url_loader()->CompletionStatus()->error_code);
  EXPECT_EQ(200, test_helper->GetResponseCode());
}

// Simulate time passing, without setting the timeout. This should result in no
// timer being started, and request should succeed.
TEST_F(SimpleURLLoaderMockTimeTest, TimeNotSetAndTimeAdvanced) {
  MockURLLoaderFactory loader_factory(
      &task_environment_, {ReadAndDiscardBodyType::kEnabled, false});
  loader_factory.AddEvents(
      {TestLoaderEvent::kAdvanceOneSecond, TestLoaderEvent::kReceivedResponse,
       TestLoaderEvent::kResponseComplete, TestLoaderEvent::kBodyBufferClosed});
  std::unique_ptr<SimpleLoaderTestHelper> test_helper = CreateHelper();

  loader_factory.RunTest(test_helper.get());

  EXPECT_EQ(net::OK, test_helper->simple_url_loader()->NetError());
  ASSERT_TRUE(test_helper->simple_url_loader()->CompletionStatus());
  EXPECT_EQ(net::OK,
            test_helper->simple_url_loader()->CompletionStatus()->error_code);
  EXPECT_EQ(200, test_helper->GetResponseCode());
}

// Simulate time passing before and after a redirect. The redirect should not
// reset the timeout timer, and the request should timeout.
TEST_F(SimpleURLLoaderMockTimeTest, TimeoutAfterRedirectTriggered) {
  MockURLLoaderFactory loader_factory(
      &task_environment_, {ReadAndDiscardBodyType::kEnabled, false});
  loader_factory.AddEvents(
      {TestLoaderEvent::kAdvanceOneSecond, TestLoaderEvent::kReceivedRedirect,
       TestLoaderEvent::kAdvanceOneSecond, TestLoaderEvent::kReceivedResponse,
       TestLoaderEvent::kBodyBufferClosed, TestLoaderEvent::kResponseComplete});
  std::unique_ptr<SimpleLoaderTestHelper> test_helper = CreateHelper();
  test_helper->simple_url_loader()->SetTimeoutDuration(base::Seconds(2));

  loader_factory.RunTest(test_helper.get());

  EXPECT_EQ(net::ERR_TIMED_OUT, test_helper->simple_url_loader()->NetError());
  EXPECT_FALSE(test_helper->simple_url_loader()->CompletionStatus());
}

// Simulate time passing after a failure. The retry restarts the timeout timer,
// so the second attempt gets a full two seconds and it is not exhausted.
TEST_F(SimpleURLLoaderMockTimeTest, TimeoutAfterRetryNotTriggered) {
  MockURLLoaderFactory loader_factory(
      &task_environment_, {ReadAndDiscardBodyType::kEnabled, false});
  loader_factory.AddEvents({TestLoaderEvent::kAdvanceOneSecond,
                            TestLoaderEvent::kReceived501Response});
  loader_factory.AddEvents(
      {TestLoaderEvent::kAdvanceOneSecond, TestLoaderEvent::kReceivedResponse,
       TestLoaderEvent::kBodyBufferClosed, TestLoaderEvent::kResponseComplete});
  std::unique_ptr<SimpleLoaderTestHelper> test_helper = CreateHelper();
  test_helper->simple_url_loader()->SetTimeoutDuration(base::Seconds(2));
  test_helper->simple_url_loader()->SetRetryOptions(
      1, SimpleURLLoader::RETRY_ON_5XX);

  loader_factory.RunTest(test_helper.get());

  EXPECT_EQ(net::OK, test_helper->simple_url_loader()->NetError());
  ASSERT_TRUE(test_helper->simple_url_loader()->CompletionStatus());
  EXPECT_EQ(net::OK,
            test_helper->simple_url_loader()->CompletionStatus()->error_code);
  EXPECT_EQ(200, test_helper->GetResponseCode());
}

// Trigger a failure and retry, and then simulate enough time passing to trigger
// the timeout. The retry should have correctly started its timeout timer.
TEST_F(SimpleURLLoaderMockTimeTest, TimeoutAfterRetryTriggered) {
  MockURLLoaderFactory loader_factory(
      &task_environment_, {ReadAndDiscardBodyType::kEnabled, false});
  loader_factory.AddEvents({TestLoaderEvent::kAdvanceOneSecond,
                            TestLoaderEvent::kReceived501Response});
  loader_factory.AddEvents(
      {TestLoaderEvent::kAdvanceOneSecond, TestLoaderEvent::kAdvanceOneSecond,
       TestLoaderEvent::kBodyBufferClosed, TestLoaderEvent::kResponseComplete});
  std::unique_ptr<SimpleLoaderTestHelper> test_helper = CreateHelper();
  test_helper->simple_url_loader()->SetTimeoutDuration(
      base::Milliseconds(1900));
  test_helper->simple_url_loader()->SetRetryOptions(
      1, SimpleURLLoader::RETRY_ON_5XX);

  loader_factory.RunTest(test_helper.get());

  EXPECT_EQ(net::ERR_TIMED_OUT, test_helper->simple_url_loader()->NetError());
  EXPECT_FALSE(test_helper->simple_url_loader()->CompletionStatus());
}

TEST_P(SimpleURLLoaderTest, OnUploadProgressCallback) {
  std::string long_string;
  if (SimpleLoaderTestHelper::IsDownloadTypeToFile(GetDownloadType())) {
    // Use a smaller upload body when writing to disk - sometimes creating a
    // large file takes a while on the bots (and, strangely, sometimes it
    // performs fine on the exact same bot).
    long_string = GetLongUploadBody();
  } else {
    // The size of the payload cannot be bigger than
    // net::test_server::<anonymous>::kRequestSizeLimit which is
    // 64Mb. We set a pretty large value in order to ensure multiple
    // progress update calls even on fast machines.
    long_string = GetLongUploadBody(31 * 1024 * 1024);
  }
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(test_server_.GetURL("/echo"), "POST");
  test_helper->simple_url_loader()->AttachStringForUpload(long_string,
                                                          "text/plain");

  uint64_t progress = 0;
  test_helper->simple_url_loader()->SetOnUploadProgressCallback(
      base::BindLambdaForTesting([&](uint64_t current, uint64_t total) {
        EXPECT_GT(current, progress);
        EXPECT_GE(total, current);
        progress = current;
      }));
  test_helper->StartSimpleLoaderAndWait(url_loader_factory_.get());
  EXPECT_EQ(net::OK, test_helper->simple_url_loader()->NetError());
  ASSERT_TRUE(test_helper->simple_url_loader()->CompletionStatus());
  EXPECT_EQ(net::OK,
            test_helper->simple_url_loader()->CompletionStatus()->error_code);
  EXPECT_EQ(long_string.size(), progress);
  if (!IsHeadersOnly()) {
    ASSERT_TRUE(test_helper->response_body());
    EXPECT_EQ(long_string, *test_helper->response_body());
  }
}

TEST_P(SimpleURLLoaderTest, OnDownloadProgressCallback) {
  const uint32_t kResponseSize = 512 * 1024;
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(test_server_.GetURL(
          base::StringPrintf("/response-size?%u", kResponseSize)));
  uint64_t progress = 0;
  test_helper->simple_url_loader()->SetOnDownloadProgressCallback(
      base::BindLambdaForTesting([&](uint64_t current) {
        EXPECT_GE(current, progress);
        progress = current;
      }));
  test_helper->StartSimpleLoaderAndWait(url_loader_factory_.get());
  if (IsHeadersOnly()) {
    EXPECT_EQ(0u, progress);
  } else {
    EXPECT_EQ(kResponseSize, progress);
  }
}

// Ensure that deleting the SimpleURLLoader in the upload progress
// callback is safe
TEST_P(SimpleURLLoaderTest, DeleteInOnUploadProgressCallback) {
  std::string long_string = GetLongUploadBody();
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(test_server_.GetURL("/echo"), "POST");
  test_helper->simple_url_loader()->AttachStringForUpload(long_string,
                                                          "text/plain");

  base::RunLoop run_loop;
  test_helper->simple_url_loader()->SetOnUploadProgressCallback(
      base::BindLambdaForTesting([&](uint64_t current, uint64_t total) {
        test_helper.reset();
        run_loop.Quit();
      }));
  test_helper->StartSimpleLoader(url_loader_factory_.get());
  run_loop.Run();
}

// Ensure that deleting the SimpleURLLoader in the upload progress
// callback is safe --- first invocation.
TEST_P(SimpleURLLoaderTest, DeleteInDownloadProgressCallback) {
  if (IsHeadersOnly()) {
    GTEST_SKIP() << "No progress callback to delete stuff in.";
  }

  const uint32_t kResponseSize = 512 * 1024;
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(test_server_.GetURL(
          base::StringPrintf("/response-size?%u", kResponseSize)));

  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting([&](uint64_t current) {
    test_helper->DestroySimpleURLLoader();  // cleanup files.
    task_environment_.RunUntilIdle();
    test_helper.reset();
    run_loop.Quit();
  });

  test_helper->simple_url_loader()->SetOnDownloadProgressCallback(callback);
  test_helper->StartSimpleLoader(url_loader_factory_.get());
  run_loop.Run();
}

// Ensure that deleting the SimpleURLLoader in the upload progress
// callback is safe --- completion invocation.
TEST_P(SimpleURLLoaderTest, DeleteInDownloadProgressCallback2) {
  if (IsHeadersOnly()) {
    GTEST_SKIP() << "No progress callback to delete stuff in.";
  }

  const uint32_t kResponseSize = 512 * 1024;
  std::unique_ptr<SimpleLoaderTestHelper> test_helper =
      CreateHelperForURL(test_server_.GetURL(
          base::StringPrintf("/response-size?%u", kResponseSize)));

  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting([&](uint64_t current) {
    if (current == kResponseSize) {
      test_helper->DestroySimpleURLLoader();  // cleanup files.
      task_environment_.RunUntilIdle();
      test_helper.reset();
      run_loop.Quit();
    }
  });
  test_helper->simple_url_loader()->SetOnDownloadProgressCallback(callback);
  test_helper->StartSimpleLoader(url_loader_factory_.get());
  run_loop.Run();
}

}  // namespace
}  // namespace network
