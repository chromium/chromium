// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/drive/drive_api_requests.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "google_apis/drive/drive_api_parser.h"
#include "google_apis/drive/drive_api_url_generator.h"
#include "google_apis/drive/dummy_auth_service.h"
#include "google_apis/drive/request_sender.h"
#include "google_apis/drive/test_util.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_network_context_client.h"
#include "services/network/test/test_network_service_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace google_apis {

namespace {

const char kTestETag[] = "test_etag";
const char kTestUserAgent[] = "test-user-agent";

const char kTestChildrenResponse[] =
    "{\n"
    "\"kind\": \"drive#childReference\",\n"
    "\"id\": \"resource_id\",\n"
    "\"selfLink\": \"self_link\",\n"
    "\"childLink\": \"child_link\",\n"
    "}\n";

const char kTestPermissionResponse[] =
    "{\n"
    "\"kind\": \"drive#permission\",\n"
    "\"id\": \"resource_id\",\n"
    "\"selfLink\": \"self_link\",\n"
    "}\n";

const char kTestUploadExistingFilePath[] = "/upload/existingfile/path";
const char kTestUploadNewFilePath[] = "/upload/newfile/path";
const char kTestDownloadPathPrefix[] = "/drive/v2/files/";
const char kTestDownloadFileQuery[] = "alt=media&supportsTeamDrives=true";

// Used as a GetContentCallback.
void AppendContent(std::string* out,
                   DriveApiErrorCode error,
                   std::unique_ptr<std::string> content) {
  EXPECT_EQ(HTTP_SUCCESS, error);
  out->append(*content);
}

class TestBatchableDelegate : public BatchableDelegate {
 public:
  TestBatchableDelegate(const GURL url,
                        const std::string& content_type,
                        const std::string& content_data,
                        const base::Closure& callback)
      : url_(url),
        content_type_(content_type),
        content_data_(content_data),
        callback_(callback) {}
  GURL GetURL() const override { return url_; }
  std::string GetRequestType() const override { return "PUT"; }
  std::vector<std::string> GetExtraRequestHeaders() const override {
    return std::vector<std::string>();
  }
  void Prepare(const PrepareCallback& callback) override {
    callback.Run(HTTP_SUCCESS);
  }
  bool GetContentData(std::string* upload_content_type,
                      std::string* upload_content) override {
    upload_content_type->assign(content_type_);
    upload_content->assign(content_data_);
    return true;
  }
  void NotifyError(DriveApiErrorCode code) override { callback_.Run(); }
  void NotifyResult(DriveApiErrorCode code,
                    const std::string& body,
                    const base::Closure& closure) override {
    callback_.Run();
    closure.Run();
  }
  void NotifyUploadProgress(int64_t current, int64_t total) override {
    progress_values_.push_back(current);
  }
  const std::vector<int64_t>& progress_values() const {
    return progress_values_;
  }

 private:
  GURL url_;
  std::string content_type_;
  std::string content_data_;
  base::Closure callback_;
  std::vector<int64_t> progress_values_;
};

}  // namespace

class DriveApiRequestsTest : public testing::Test {
 public:
  DriveApiRequestsTest() {
    mojo::Remote<network::mojom::NetworkService> network_service_remote;
    network_service_ = network::NetworkService::Create(
        network_service_remote.BindNewPipeAndPassReceiver());
    network::mojom::NetworkContextParamsPtr context_params =
        network::mojom::NetworkContextParams::New();
    network_service_remote->CreateNetworkContext(
        network_context_.BindNewPipeAndPassReceiver(),
        std::move(context_params));

    mojo::PendingRemote<network::mojom::NetworkServiceClient>
        network_service_client_remote;
    network_service_client_ =
        std::make_unique<network::TestNetworkServiceClient>(
            network_service_client_remote.InitWithNewPipeAndPassReceiver());
    network_service_remote->SetClient(
        std::move(network_service_client_remote),
        network::mojom::NetworkServiceParams::New());

    mojo::PendingRemote<network::mojom::NetworkContextClient>
        network_context_client_remote;
    network_context_client_ =
        std::make_unique<network::TestNetworkContextClient>(
            network_context_client_remote.InitWithNewPipeAndPassReceiver());
    network_context_->SetClient(std::move(network_context_client_remote));

    network::mojom::URLLoaderFactoryParamsPtr params =
        network::mojom::URLLoaderFactoryParams::New();
    params->process_id = network::mojom::kBrowserProcessId;
    params->is_corb_enabled = false;
    network_context_->CreateURLLoaderFactory(
        url_loader_factory_.BindNewPipeAndPassReceiver(), std::move(params));
    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            url_loader_factory_.get());
  }

  void SetUp() override {
    request_sender_ = std::make_unique<RequestSender>(
        std::make_unique<DummyAuthService>(), test_shared_loader_factory_,
        task_environment_.GetMainThreadTaskRunner(), kTestUserAgent,
        TRAFFIC_ANNOTATION_FOR_TESTS);

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    test_server_.RegisterRequestHandler(
        base::Bind(&DriveApiRequestsTest::HandleChildrenDeleteRequest,
                   base::Unretained(this)));
    test_server_.RegisterRequestHandler(
        base::Bind(&DriveApiRequestsTest::HandleDataFileRequest,
                   base::Unretained(this)));
    test_server_.RegisterRequestHandler(
        base::Bind(&DriveApiRequestsTest::HandleDeleteRequest,
                   base::Unretained(this)));
    test_server_.RegisterRequestHandler(
        base::Bind(&DriveApiRequestsTest::HandlePreconditionFailedRequest,
                   base::Unretained(this)));
    test_server_.RegisterRequestHandler(
        base::Bind(&DriveApiRequestsTest::HandleResumeUploadRequest,
                   base::Unretained(this)));
    test_server_.RegisterRequestHandler(
        base::Bind(&DriveApiRequestsTest::HandleInitiateUploadRequest,
                   base::Unretained(this)));
    test_server_.RegisterRequestHandler(
        base::Bind(&DriveApiRequestsTest::HandleContentResponse,
                   base::Unretained(this)));
    test_server_.RegisterRequestHandler(
        base::Bind(&DriveApiRequestsTest::HandleDownloadRequest,
                   base::Unretained(this)));
    test_server_.RegisterRequestHandler(
        base::Bind(&DriveApiRequestsTest::HandleBatchUploadRequest,
                   base::Unretained(this)));
    ASSERT_TRUE(test_server_.Start());

    GURL test_base_url = test_util::GetBaseUrlForTesting(test_server_.port());
    url_generator_.reset(
        new DriveApiUrlGenerator(test_base_url, test_base_url));

    // Reset the server's expected behavior just in case.
    ResetExpectedResponse();
    received_bytes_ = 0;
    content_length_ = 0;

    // Testing properties used by multiple test cases.
    drive::Property private_property;
    private_property.set_key("key1");
    private_property.set_value("value1");

    drive::Property public_property;
    public_property.set_visibility(drive::Property::VISIBILITY_PUBLIC);
    public_property.set_key("key2");
    public_property.set_value("value2");

    testing_properties_.clear();
    testing_properties_.push_back(private_property);
    testing_properties_.push_back(public_property);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  net::EmbeddedTestServer test_server_;
  std::unique_ptr<RequestSender> request_sender_;
  std::unique_ptr<DriveApiUrlGenerator> url_generator_;
  std::unique_ptr<network::mojom::NetworkService> network_service_;
  std::unique_ptr<network::mojom::NetworkServiceClient> network_service_client_;
  std::unique_ptr<network::mojom::NetworkContextClient> network_context_client_;
  mojo::Remote<network::mojom::NetworkContext> network_context_;
  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory_;
  scoped_refptr<network::WeakWrapperSharedURLLoaderFactory>
      test_shared_loader_factory_;
  base::ScopedTempDir temp_dir_;

  // This is a path to the file which contains expected response from
  // the server. See also HandleDataFileRequest below.
  base::FilePath expected_data_file_path_;

  // This is a path string in the expected response header from the server
  // for initiating file uploading.
  std::string expected_upload_path_;

  // This is a path to the file which contains expected response for
  // PRECONDITION_FAILED response.
  base::FilePath expected_precondition_failed_file_path_;

  // These are content and its type in the expected response from the server.
  // See also HandleContentResponse below.
  std::string expected_content_type_;
  std::string expected_content_;

  // The incoming HTTP request is saved so tests can verify the request
  // parameters like HTTP method (ex. some requests should use DELETE
  // instead of GET).
  net::test_server::HttpRequest http_request_;

  // Testing properties used by multiple test cases.
  drive::Properties testing_properties_;

 private:
  void ResetExpectedResponse() {
    expected_data_file_path_.clear();
    expected_upload_path_.clear();
    expected_content_type_.clear();
    expected_content_.clear();
  }

  // For "Children: delete" request, the server will return "204 No Content"
  // response meaning "success".
  std::unique_ptr<net::test_server::HttpResponse> HandleChildrenDeleteRequest(
      const net::test_server::HttpRequest& request) {
    if (request.method != net::test_server::METHOD_DELETE ||
        request.relative_url.find("/children/") == std::string::npos) {
      // The request is not the "Children: delete" request. Delegate the
      // processing to the next handler.
      return std::unique_ptr<net::test_server::HttpResponse>();
    }

    http_request_ = request;

    // Return the response with just "204 No Content" status code.
    std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
        new net::test_server::BasicHttpResponse);
    http_response->set_code(net::HTTP_NO_CONTENT);
    return std::move(http_response);
  }

  // Reads the data file of |expected_data_file_path_| and returns its content
  // for the request.
  // To use this method, it is necessary to set |expected_data_file_path_|
  // to the appropriate file path before sending the request to the server.
  std::unique_ptr<net::test_server::HttpResponse> HandleDataFileRequest(
      const net::test_server::HttpRequest& request) {
    if (expected_data_file_path_.empty()) {
      // The file is not specified. Delegate the processing to the next
      // handler.
      return std::unique_ptr<net::test_server::HttpResponse>();
    }

    http_request_ = request;

    // Return the response from the data file.
    return test_util::CreateHttpResponseFromFile(expected_data_file_path_);
  }

  // Deletes the resource and returns no content with HTTP_NO_CONTENT status
  // code.
  std::unique_ptr<net::test_server::HttpResponse> HandleDeleteRequest(
      const net::test_server::HttpRequest& request) {
    if (request.method != net::test_server::METHOD_DELETE ||
        request.relative_url.find("/files/") == std::string::npos) {
      // The file is not file deletion request. Delegate the processing to the
      // next handler.
      return std::unique_ptr<net::test_server::HttpResponse>();
    }

    http_request_ = request;

    std::unique_ptr<net::test_server::BasicHttpResponse> response(
        new net::test_server::BasicHttpResponse);
    response->set_code(net::HTTP_NO_CONTENT);

    return std::move(response);
  }

  // Returns PRECONDITION_FAILED response for ETag mismatching with error JSON
  // content specified by |expected_precondition_failed_file_path_|.
  // To use this method, it is necessary to set the variable to the appropriate
  // file path before sending the request to the server.
  std::unique_ptr<net::test_server::HttpResponse>
  HandlePreconditionFailedRequest(
      const net::test_server::HttpRequest& request) {
    if (expected_precondition_failed_file_path_.empty()) {
      // The file is not specified. Delegate the process to the next handler.
      return std::unique_ptr<net::test_server::HttpResponse>();
    }

    http_request_ = request;

    std::unique_ptr<net::test_server::BasicHttpResponse> response(
        new net::test_server::BasicHttpResponse);
    response->set_code(net::HTTP_PRECONDITION_FAILED);

    std::string content;
    if (base::ReadFileToString(expected_precondition_failed_file_path_,
                               &content)) {
      response->set_content(content);
      response->set_content_type("application/json");
    }

    return std::move(response);
  }

  // Returns the response based on set expected upload url.
  // The response contains the url in its "Location: " header. Also, it doesn't
  // have any content.
  // To use this method, it is necessary to set |expected_upload_path_|
  // to the string representation of the url to be returned.
  std::unique_ptr<net::test_server::HttpResponse> HandleInitiateUploadRequest(
      const net::test_server::HttpRequest& request) {
    if (request.relative_url == expected_upload_path_ ||
        expected_upload_path_.empty()) {
      // The request is for resume uploading or the expected upload url is not
      // set. Delegate the processing to the next handler.
      return std::unique_ptr<net::test_server::HttpResponse>();
    }

    http_request_ = request;

    std::unique_ptr<net::test_server::BasicHttpResponse> response(
        new net::test_server::BasicHttpResponse);

    // Check if the X-Upload-Content-Length is present. If yes, store the
    // length of the file.
    auto found = request.headers.find("X-Upload-Content-Length");
    if (found == request.headers.end() ||
        !base::StringToInt64(found->second, &content_length_)) {
      return std::unique_ptr<net::test_server::HttpResponse>();
    }
    received_bytes_ = 0;

    response->set_code(net::HTTP_OK);
    response->AddCustomHeader(
        "Location",
        test_server_.base_url().Resolve(expected_upload_path_).spec());
    return std::move(response);
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleResumeUploadRequest(
      const net::test_server::HttpRequest& request) {
    if (request.relative_url != expected_upload_path_) {
      // The request path is different from the expected path for uploading.
      // Delegate the processing to the next handler.
      return std::unique_ptr<net::test_server::HttpResponse>();
    }

    http_request_ = request;

    if (!request.content.empty()) {
      auto iter = request.headers.find("Content-Range");
      if (iter == request.headers.end()) {
        // The range must be set.
        return std::unique_ptr<net::test_server::HttpResponse>();
      }

      int64_t length = 0;
      int64_t start_position = 0;
      int64_t end_position = 0;
      if (!test_util::ParseContentRangeHeader(
              iter->second, &start_position, &end_position, &length)) {
        // Invalid "Content-Range" value.
        return std::unique_ptr<net::test_server::HttpResponse>();
      }

      EXPECT_EQ(start_position, received_bytes_);
      EXPECT_EQ(length, content_length_);

      // end_position is inclusive, but so +1 to change the range to byte size.
      received_bytes_ = end_position + 1;
    }

    if (received_bytes_ < content_length_) {
      std::unique_ptr<net::test_server::BasicHttpResponse> response(
          new net::test_server::BasicHttpResponse);
      // Set RESUME INCOMPLETE (308) status code.
      response->set_code(static_cast<net::HttpStatusCode>(308));

      // Add Range header to the response, based on the values of
      // Content-Range header in the request.
      // The header is annotated only when at least one byte is received.
      if (received_bytes_ > 0) {
        response->AddCustomHeader(
            "Range", "bytes=0-" + base::NumberToString(received_bytes_ - 1));
      }

      return std::move(response);
    }

    // All bytes are received. Return the "success" response with the file's
    // (dummy) metadata.
    std::unique_ptr<net::test_server::BasicHttpResponse> response =
        test_util::CreateHttpResponseFromFile(
            test_util::GetTestFilePath("drive/file_entry.json"));

    // The response code is CREATED if it is new file uploading.
    if (http_request_.relative_url == kTestUploadNewFilePath) {
      response->set_code(net::HTTP_CREATED);
    }

    return std::move(response);
  }

  // Returns the response based on set expected content and its type.
  // To use this method, both |expected_content_type_| and |expected_content_|
  // must be set in advance.
  std::unique_ptr<net::test_server::HttpResponse> HandleContentResponse(
      const net::test_server::HttpRequest& request) {
    if (expected_content_type_.empty() || expected_content_.empty()) {
      // Expected content is not set. Delegate the processing to the next
      // handler.
      return std::unique_ptr<net::test_server::HttpResponse>();
    }

    http_request_ = request;

    std::unique_ptr<net::test_server::BasicHttpResponse> response(
        new net::test_server::BasicHttpResponse);
    response->set_code(net::HTTP_OK);
    response->set_content_type(expected_content_type_);
    response->set_content(expected_content_);
    return std::move(response);
  }

  // Handles a request for downloading a file.
  std::unique_ptr<net::test_server::HttpResponse> HandleDownloadRequest(
      const net::test_server::HttpRequest& request) {
    http_request_ = request;

    const GURL absolute_url = test_server_.GetURL(request.relative_url);
    std::string id;
    if (!test_util::RemovePrefix(
          absolute_url.path(), kTestDownloadPathPrefix, &id) ||
        absolute_url.query() != kTestDownloadFileQuery) {
      return std::unique_ptr<net::test_server::HttpResponse>();
    }

    // For testing, returns a text with |id| repeated 3 times.
    std::unique_ptr<net::test_server::BasicHttpResponse> response(
        new net::test_server::BasicHttpResponse);
    response->set_code(net::HTTP_OK);
    response->set_content(id + id + id);
    response->set_content_type("text/plain");
    return std::move(response);
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleBatchUploadRequest(
      const net::test_server::HttpRequest& request) {
    http_request_ = request;

    const GURL absolute_url = test_server_.GetURL(request.relative_url);
    std::string id;
    if (absolute_url.path() != "/upload/drive")
      return std::unique_ptr<net::test_server::HttpResponse>();

    std::unique_ptr<net::test_server::BasicHttpResponse> response(
        new net::test_server::BasicHttpResponse);
    response->set_code(net::HTTP_OK);
    response->set_content_type("multipart/mixed; boundary=BOUNDARY");
    response->set_content(
        "--BOUNDARY\r\n"
        "Content-Type: application/http\r\n"
        "\r\n"
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json; charset=UTF-8\r\n"
        "\r\n"
        "{\r\n"
        " \"kind\": \"drive#file\",\r\n"
        " \"id\": \"file_id_1\"\r\n"
        "}\r\n"
        "\r\n"
        "--BOUNDARY\r\n"
        "Content-Type: application/http\r\n"
        "\r\n"
        "HTTP/1.1 403 Forbidden\r\n"
        "Content-Type: application/json; charset=UTF-8\r\n"
        "\r\n"
        "{\"error\":{\"errors\": ["
        " {\"reason\": \"userRateLimitExceeded\"}]}}\r\n"
        "\r\n"
        "--BOUNDARY--\r\n");
    return std::move(response);
  }

  // These are for the current upload file status.
  int64_t received_bytes_;
  int64_t content_length_;
};

TEST_F(DriveApiRequestsTest, DriveApiDataRequest_Fields) {
  // Make sure that "fields" query param is supported by using its subclass,
  // AboutGetRequest.

  // Set an expected data file containing valid result.
  expected_data_file_path_ = test_util::GetTestFilePath(
      "drive/about.json");

  DriveApiErrorCode error = DRIVE_OTHER_ERROR;
  std::unique_ptr<AboutResource> about_resource;

  {
    base::RunLoop run_loop;
    std::unique_ptr<drive::AboutGetRequest> request =
        std::make_unique<drive::AboutGetRequest>(
            request_sender_.get(), *url_generator_,
            test_util::CreateQuitCallback(
                &run_loop,
                test_util::CreateCopyResultCallback(&error, &about_resource)));
    request->set_fields("kind,quotaBytesTotal,quotaBytesUsedAggregate,"
                        "largestChangeId,rootFolderId");
    request_sender_->StartRequestWithAuthRetry(std::move(request));
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(net::test_server::METHOD_GET, http_request_.method);
  EXPECT_EQ("/drive/v2/about?"
            "fields=kind%2CquotaBytesTotal%2CquotaBytesUsedAggregate%2C"
            "largestChangeId%2CrootFolderId",
            http_request_.relative_url);

  std::unique_ptr<AboutResource> expected(
      AboutResource::CreateFrom(*test_util::LoadJSONFile("drive/about.json")));
  ASSERT_TRUE(about_resource.get());
  EXPECT_EQ(expected->largest_change_id(), about_resource->largest_change_id());
  EXPECT_EQ(expected->quota_bytes_total(), about_resource->quota_bytes_total());
  EXPECT_EQ(expected->quota_bytes_used_aggregate(),
            about_resource->quota_bytes_used_aggregate());
  EXPECT_EQ(expected->root_folder_id(), about_resource->root_folder_id());
}

TEST_F(DriveApiRequestsTest, FilesInsertRequest) {
  const base::Time::Exploded kModifiedDate = {2012, 7, 0, 19, 15, 59, 13, 123};
  const base::Time::Exploded kLastViewedByMeDate =
      {2013, 7, 0, 19, 15, 59, 13, 123};

  // Set an expected data file containing the directory's entry data.
  expected_data_file_path_ =
      test_util::GetTestFilePath("drive/directory_entry.json");

  DriveApiErrorCode error = DRIVE_OTHER_ERROR;
  std::unique_ptr<FileResource> file_resource;

  // Create "new directory" in the root directory.
  {
    base::RunLoop run_loop;
    std::unique_ptr<drive::FilesInsertRequest> request =
        std::make_unique<drive::FilesInsertRequest>(
            request_sender_.get(), *url_generator_,
            test_util::CreateQuitCallback(
                &run_loop,
                test_util::CreateCopyResultCallback(&error, &file_resource)));
    request->set_visibility(drive::FILE_VISIBILITY_PRIVATE);

    base::Time last_viewed_by_me_date_utc;
    ASSERT_TRUE(base::Time::FromUTCExploded(kLastViewedByMeDate,
                                            &last_viewed_by_me_date_utc));
    request->set_last_viewed_by_me_date(last_viewed_by_me_date_utc);

    base::Time modified_date_utc;
    ASSERT_TRUE(base::Time::FromUTCExploded(kModifiedDate, &modified_date_utc));
    request->set_modified_date(modified_date_utc);

    request->set_mime_type("application/vnd.google-apps.folder");
    request->add_parent("root");
    request->set_title("new directory");
    request->set_properties(testing_properties_);
    request_sender_->StartRequestWithAuthRetry(std::move(request));
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(net::test_server::METHOD_POST, http_request_.method);
  EXPECT_EQ("/drive/v2/files?supportsTeamDrives=true&visibility=PRIVATE",
            http_request_.relative_url);
  EXPECT_EQ("application/json", http_request_.headers["Content-Type"]);

  EXPECT_TRUE(http_request_.has_content);
  EXPECT_EQ(
      "{\"lastViewedByMeDate\":\"2013-07-19T15:59:13.123Z\","
      "\"mimeType\":\"application/vnd.google-apps.folder\","
      "\"modifiedDate\":\"2012-07-19T15:59:13.123Z\","
      "\"parents\":[{\"id\":\"root\"}],"
      "\"properties\":["
      "{\"key\":\"key1\",\"value\":\"value1\",\"visibility\":\"PRIVATE\"},"
      "{\"key\":\"key2\",\"value\":\"value2\",\"visibility\":\"PUBLIC\"}],"
      "\"title\":\"new directory\"}",
      http_request_.content);

  std::unique_ptr<FileResource> expected(FileResource::CreateFrom(
      *test_util::LoadJSONFile("drive/directory_entry.json")));

  // Sanity check.
  ASSERT_TRUE(file_resource.get());

  EXPECT_EQ(expected->file_id(), file_resource->file_id());
  EXPECT_EQ(expected->title(), file_resource->title());
  EXPECT_EQ(expected->mime_type(), file_resource->mime_type());
  EXPECT_EQ(expected->parents().size(), file_resource->parents().size());
}

TEST_F(DriveApiRequestsTest, FilesPatchRequest) {
  const base::Time::Exploded kModifiedDate = {2012, 7, 0, 19, 15, 59, 13, 123};
  const base::Time::Exploded kLastViewedByMeDate =
      {2013, 7, 0, 19, 15, 59, 13, 123};

  // Set an expected data file containing valid result.
  expected_data_file_path_ =
      test_util::GetTestFilePath("drive/file_entry.json");

  DriveApiErrorCode error = DRIVE_OTHER_ERROR;
  std::unique_ptr<FileResource> file_resource;

  {
    base::RunLoop run_loop;
    std::unique_ptr<drive::FilesPatchRequest> request =
        std::make_unique<drive::FilesPatchRequest>(
            request_sender_.get(), *url_generator_,
            test_util::CreateQuitCallback(
                &run_loop,
                test_util::CreateCopyResultCallback(&error, &file_resource)));
    request->set_file_id("resource_id");
    request->set_set_modified_date(true);
    request->set_update_viewed_date(false);

    request->set_title("new title");
    base::Time modified_date_utc;
    ASSERT_TRUE(base::Time::FromUTCExploded(kModifiedDate, &modified_date_utc));
    request->set_modified_date(modified_date_utc);

    base::Time last_viewed_by_me_date_utc;
    ASSERT_TRUE(base::Time::FromUTCExploded(kLastViewedByMeDate,
                                            &last_viewed_by_me_date_utc));
    request->set_last_viewed_by_me_date(last_viewed_by_me_date_utc);
    request->add_parent("parent_resource_id");
    request->set_properties(testing_properties_);
    request_sender_->StartRequestWithAuthRetry(std::move(request));
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(net::test_server::METHOD_PATCH, http_request_.method);
  EXPECT_EQ(
      "/drive/v2/files/resource_id"
      "?supportsTeamDrives=true&setModifiedDate=true"
      "&updateViewedDate=false",
      http_request_.relative_url);

  EXPECT_EQ("application/json", http_request_.headers["Content-Type"]);
  EXPECT_TRUE(http_request_.has_content);
  EXPECT_EQ(
      "{\"lastViewedByMeDate\":\"2013-07-19T15:59:13.123Z\","
      "\"modifiedDate\":\"2012-07-19T15:59:13.123Z\","
      "\"parents\":[{\"id\":\"parent_resource_id\"}],"
      "\"properties\":["
      "{\"key\":\"key1\",\"value\":\"value1\",\"visibility\":\"PRIVATE\"},"
      "{\"key\":\"key2\",\"value\":\"value2\",\"visibility\":\"PUBLIC\"}],"
      "\"title\":\"new title\"}",
      http_request_.content);
  EXPECT_TRUE(file_resource);
}

TEST_F(DriveApiRequestsTest, AboutGetRequest_ValidJson) {
  // Set an expected data file containing valid result.
  expected_data_file_path_ = test_util::GetTestFilePath(
      "drive/about.json");

  DriveApiErrorCode error = DRIVE_OTHER_ERROR;
  std::unique_ptr<AboutResource> about_resource;

  {
    base::RunLoop run_loop;
    std::unique_ptr<drive::AboutGetRequest> request =
        std::make_unique<drive::AboutGetRequest>(
            request_sender_.get(), *url_generator_,
            test_util::CreateQuitCallback(
                &run_loop,
                test_util::CreateCopyResultCallback(&error, &about_resource)));
    request_sender_->StartRequestWithAuthRetry(std::move(request));
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(net::test_server::METHOD_GET, http_request_.method);
  EXPECT_EQ("/drive/v2/about", http_request_.relative_url);

  std::unique_ptr<AboutResource> expected(
      AboutResource::CreateFrom(*test_util::LoadJSONFile("drive/about.json")));
  ASSERT_TRUE(about_resource.get());
  EXPECT_EQ(expected->largest_change_id(), about_resource->largest_change_id());
  EXPECT_EQ(expected->quota_bytes_total(), about_resource->quota_bytes_total());
  EXPECT_EQ(expected->quota_bytes_used_aggregate(),
            about_resource->quota_bytes_used_aggregate());
  EXPECT_EQ(expected->root_folder_id(), about_resource->root_folder_id());
}

TEST_F(DriveApiRequestsTest, AboutGetRequest_InvalidJson) {
  // Set an expected data file containing invalid result.
  expected_data_file_path_ = test_util::GetTestFilePath(
      "drive/testfile.txt");

  DriveApiErrorCode error = DRIVE_OTHER_ERROR;
  std::unique_ptr<AboutResource> about_resource;

  {
    base::RunLoop run_loop;
    std::unique_ptr<drive::AboutGetRequest> request =
        std::make_unique<drive::AboutGetRequest>(
            request_sender_.get(), *url_generator_,
            test_util::CreateQuitCallback(
                &run_loop,
                test_util::CreateCopyResultCallback(&error, &about_resource)));
    request_sender_->StartRequestWithAuthRetry(std::move(request));
    run_loop.Run();
  }

  // "parse error" should be returned, and the about resource should be NULL.
  EXPECT_EQ(DRIVE_PARSE_ERROR, error);
  EXPECT_EQ(net::test_server::METHOD_GET, http_request_.method);
  EXPECT_EQ("/drive/v2/about", http_request_.relative_url);
  EXPECT_FALSE(about_resource);
}

TEST_F(DriveApiRequestsTest, ChangesListRequest) {
  // Set an expected data file containing valid result.
  expected_data_file_path_ = test_util::GetTestFilePath(
      "drive/changelist.json");

  DriveApiErrorCode error = DRIVE_OTHER_ERROR;
  std::unique_ptr<ChangeList> result;

  {
    base::RunLoop run_loop;
    std::unique_ptr<drive::ChangesListRequest> request =
        std::make_unique<drive::ChangesListRequest>(
            request_sender_.get(), *url_generator_,
            test_util::CreateQuitCallback(
                &run_loop,
                test_util::CreateCopyResultCallback(&error, &result)));
    request->set_include_deleted(true);
    request->set_start_change_id(100);
    request->set_max_results(500);
    request->set_team_drive_id("TEAM_DRIVE_ID");
    request_sender_->StartRequestWithAuthRetry(std::move(request));
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(net::test_server::METHOD_GET, http_request_.method);
  EXPECT_EQ(
      "/drive/v2/changes?supportsTeamDrives=true&"
      "includeTeamDriveItems=true&teamDriveId=TEAM_DRIVE_ID&"
      "maxResults=500&startChangeId=100",
      http_request_.relative_url);
  EXPECT_TRUE(result);
}

TEST_F(DriveApiRequestsTest, ChangesListNextPageRequest) {
  // Set an expected data file containing valid result.
  expected_data_file_path_ = test_util::GetTestFilePath(
      "drive/changelist.json");

  DriveApiErrorCode error = DRIVE_OTHER_ERROR;
  std::unique_ptr<ChangeList> result;

  {
    base::RunLoop run_loop;
    std::unique_ptr<drive::ChangesListNextPageRequest> request =
        std::make_unique<drive::ChangesListNextPageRequest>(
            request_sender_.get(),
            test_util::CreateQuitCallback(
                &run_loop,
                test_util::CreateCopyResultCallback(&error, &result)));
    request->set_next_link(test_server_.GetURL("/continue/get/change/list"));
    request_sender_->StartRequestWithAuthRetry(std::move(request));
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(net::test_server::METHOD_GET, http_request_.method);
  EXPECT_EQ("/continue/get/change/list", http_request_.relative_url);
  EXPECT_TRUE(result);
}

TEST_F(DriveApiRequestsTest, FilesCopyRequest) {
  const base::Time::Exploded kModifiedDate = {2012, 7, 0, 19, 15, 59, 13, 123};

  // Set an expected data file containing the dummy file entry data.
  // It'd be returned if we copy a file.
  expected_data_file_path_ =
      test_util::GetTestFilePath("drive/file_entry.json");

  DriveApiErrorCode error = DRIVE_OTHER_ERROR;
  std::unique_ptr<FileResource> file_resource;

  // Copy the file to a new file named "new title".
  {
    base::RunLoop run_loop;
    std::unique_ptr<drive::FilesCopyRequest> request =
        std::make_unique<drive::FilesCopyRequest>(
            request_sender_.get(), *url_generator_,
            test_util::CreateQuitCallback(
                &run_loop,
                test_util::CreateCopyResultCallback(&error, &file_resource)));
    request->set_visibility(drive::FILE_VISIBILITY_PRIVATE);
    request->set_file_id("resource_id");

    base::Time modified_date_utc;
    ASSERT_TRUE(base::Time::FromUTCExploded(kModifiedDate, &modified_date_utc));

    request->set_modified_date(modified_date_utc);
    request->add_parent("parent_resource_id");
    request->set_title("new title");
    request_sender_->StartRequestWithAuthRetry(std::move(request));
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(net::test_server::METHOD_POST, http_request_.method);
  EXPECT_EQ(
      "/drive/v2/files/resource_id/copy"
      "?supportsTeamDrives=true&visibility=PRIVATE",
      http_request_.relative_url);
  EXPECT_EQ("application/json", http_request_.headers["Content-Type"]);

  EXPECT_TRUE(http_request_.has_content);
  EXPECT_EQ(
      "{\"modifiedDate\":\"2012-07-19T15:59:13.123Z\","
      "\"parents\":[{\"id\":\"parent_resource_id\"}],\"title\":\"new title\"}",
      http_request_.content);
  EXPECT_TRUE(file_resource);
}

TEST_F(DriveApiRequestsTest, FilesCopyRequest_EmptyParentResourceId) {
  // Set an expected data file containing the dummy file entry data.
  // It'd be returned if we copy a file.
  expected_data_file_path_ =
      test_util::GetTestFilePath("drive/file_entry.json");

  DriveApiErrorCode error = DRIVE_OTHER_ERROR;
  std::unique_ptr<FileResource> file_resource;

  // Copy the file to a new file named "new title".
  {
    base::RunLoop run_loop;
    std::unique_ptr<drive::FilesCopyRequest> request =
        std::make_unique<drive::FilesCopyRequest>(
            request_sender_.get(), *url_generator_,
            test_util::CreateQuitCallback(
                &run_loop,
                test_util::CreateCopyResultCallback(&error, &file_resource)));
    request->set_file_id("resource_id");
    request->set_title("new title");
    request_sender_->StartRequestWithAuthRetry(std::move(request));
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(net::test_server::METHOD_POST, http_request_.method);
  EXPECT_EQ("/drive/v2/files/resource_id/copy?supportsTeamDrives=true",
            http_request_.relative_url);
  EXPECT_EQ("application/json", http_request_.headers["Content-Type"]);

  EXPECT_TRUE(http_request_.has_content);
  EXPECT_EQ("{\"title\":\"new title\"}", http_request_.content);
  EXPECT_TRUE(file_resource);
}

TEST_F(DriveApiRequestsTest, TeamDriveListRequest) {
  // Set an expected data file containing valid result.
  expected_data_file_path_ =
      test_util::GetTestFilePath("drive/team_drive_list.json");

  DriveApiErrorCode error = DRIVE_OTHER_ERROR;
  std::unique_ptr<TeamDriveList> result;

  {
    base::RunLoop run_loop;
    std::unique_ptr<drive::TeamDriveListRequest> request =
        std::make_unique<drive::TeamDriveListRequest>(
            request_sender_.get(), *url_generator_,
            test_util::CreateQuitCallback(
                &run_loop,
                test_util::CreateCopyResultCallback(&error, &result)));
    request->set_max_results(50);
    request->set_page_token("PAGE_TOKEN");
    request_sender_->StartRequestWithAuthRetry(std::move(request));
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(net::test_server::METHOD_GET, http_request_.method);
  EXPECT_EQ("/drive/v2/teamdrives?maxResults=50&pageToken=PAGE_TOKEN",
            http_request_.relative_url);
  EXPECT_TRUE(result);
}

TEST_F(DriveApiRequestsTest, StartPageTokenRequest) {
  // Set an expected data file containing valid result
  expected_data_file_path_ =
      test_util::GetTestFilePath("drive/start_page_token.json");

  DriveApiErrorCode error = DRIVE_OTHER_ERROR;
  std::unique_ptr<StartPageToken> result;

  {
    base::RunLoop run_loop;
    std::unique_ptr<drive::StartPageTokenRequest> request =
        std::make_unique<drive::StartPageTokenRequest>(
            request_sender_.get(), *url_generator_,
            test_util::CreateQuitCallback(
                &run_loop,
                test_util::CreateCopyResultCallback(&error, &result)));
    request_sender_->StartRequestWithAuthRetry(std::move(request));
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(net::test_server::METHOD_GET, http_request_.method);
  EXPECT_EQ("/drive/v2/changes/startPageToken?supportsTeamDrives=true",
            http_request_.relative_url);
  EXPECT_EQ("15734", result->start_page_token());
  EXPECT_TRUE(result);
}

TEST_F(DriveApiRequestsTest, FilesListRequest) {
  // Set an expected data file containing valid result.
  expected_data_file_path_ = test_util::GetTestFilePath(
      "drive/filelist.json");

  DriveApiErrorCode error = DRIVE_OTHER_ERROR;
  std::unique_ptr<FileList> result;

  {
    base::RunLoop run_loop;
    std::unique_ptr<drive::FilesListRequest> request =
        std::make_unique<drive::FilesListRequest>(
            request_sender_.get(), *url_generator_,
            test_util::CreateQuitCallback(
                &run_loop,
                test_util::CreateCopyResultCallback(&error, &result)));
    request->set_max_results(50);
    request->set_q("\"abcde\" in parents");
    request_sender_->StartRequestWithAuthRetry(std::move(request));
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(net::test_server::METHOD_GET, http_request_.method);
  EXPECT_EQ(
      "/drive/v2/files?supportsTeamDrives=true"
      "&includeTeamDriveItems=true&corpora=default&maxResults=50"
      "&q=%22abcde%22+in+parents",
      http_request_.relative_url);
  EXPECT_TRUE(result);
}

TEST_F(DriveApiRequestsTest, FilesListNextPageRequest) {
  // Set an expected data file containing valid result.
  expected_data_file_path_ = test_util::GetTestFilePath(
      "drive/filelist.json");

  DriveApiErrorCode error = DRIVE_OTHER_ERROR;
  std::unique_ptr<FileList> result;

  {
    base::RunLoop run_loop;
    std::unique_ptr<drive::FilesListNextPageRequest> request =
        std::make_unique<drive::FilesListNextPageRequest>(
            request_sender_.get(),
            test_util::CreateQuitCallback(
                &run_loop,
                test_util::CreateCopyResultCallback(&error, &result)));
    request->set_next_link(test_server_.GetURL("/continue/get/file/list"));
    request_sender_->StartRequestWithAuthRetry(std::move(request));
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(net::test_server::METHOD_GET, http_request_.method);
  EXPECT_EQ("/continue/get/file/list", http_request_.relative_url);
  EXPECT_TRUE(result);
}

TEST_F(DriveApiRequestsTest, FilesDeleteRequest) {
  DriveApiErrorCode error = DRIVE_OTHER_ERROR;

  // Delete a resource with the given resource id.
  {
    base::RunLoop run_loop;
    std::unique_ptr<drive::FilesDeleteRequest> request =
        std::make_unique<drive::FilesDeleteRequest>(
            request_sender_.get(), *url_generator_,
            test_util::CreateQuitCallback(
                &run_loop, test_util::CreateCopyResultCallback(&error)));
    request->set_file_id("resource_id");
    request->set_etag(kTestETag);
    request_sender_->StartRequestWithAuthRetry(std::move(request));
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_NO_CONTENT, error);
  EXPECT_EQ(net::test_server::METHOD_DELETE, http_request_.method);
  EXPECT_EQ(kTestETag, http_request_.headers["If-Match"]);
  EXPECT_EQ("/drive/v2/files/resource_id?supportsTeamDrives=true",
            http_request_.relative_url);
  EXPECT_FALSE(http_request_.has_content);
}

TEST_F(DriveApiRequestsTest, FilesTrashRequest) {
  // Set data for the expected result. Directory entry should be returned
  // if the trashing entry is a directory, so using it here should be fine.
  expected_data_file_path_ =
      test_util::GetTestFilePath("drive/directory_entry.json");

  DriveApiErrorCode error = DRIVE_OTHER_ERROR;
  std::unique_ptr<FileResource> file_resource;

  // Trash a resource with the given resource id.
  {
    base::RunLoop run_loop;
    std::unique_ptr<drive::FilesTrashRequest> request =
        std::make_unique<drive::FilesTrashRequest>(
            request_sender_.get(), *url_generator_,
            test_util::CreateQuitCallback(
                &run_loop,
                test_util::CreateCopyResultCallback(&error, &file_resource)));
    request->set_file_id("resource_id");
    request_sender_->StartRequestWithAuthRetry(std::move(request));
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(net::test_server::METHOD_POST, http_request_.method);
  EXPECT_EQ("/drive/v2/files/resource_id/trash?supportsTeamDrives=true",
            http_request_.relative_url);
  EXPECT_TRUE(http_request_.has_content);
  EXPECT_TRUE(http_request_.content.empty());
}

TEST_F(DriveApiRequestsTest, ChildrenInsertRequest) {
  // Set an expected data file containing the children entry.
  expected_content_type_ = "application/json";
  expected_content_ = kTestChildrenResponse;

  DriveApiErrorCode error = DRIVE_OTHER_ERROR;

  // Add a resource with "resource_id" to a directory with
  // "parent_resource_id".
  {
    base::RunLoop run_loop;
    std::unique_ptr<drive::ChildrenInsertRequest> request =
        std::make_unique<drive::ChildrenInsertRequest>(
            request_sender_.get(), *url_generator_,
            test_util::CreateQuitCallback(
                &run_loop, test_util::CreateCopyResultCallback(&error)));
    request->set_folder_id("parent_resource_id");
    request->set_id("resource_id");
    request_sender_->StartRequestWithAuthRetry(std::move(request));
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(net::test_server::METHOD_POST, http_request_.method);
  EXPECT_EQ(
      "/drive/v2/files/parent_resource_id/children"
      "?supportsTeamDrives=true",
      http_request_.relative_url);
  EXPECT_EQ("application/json", http_request_.headers["Content-Type"]);

  EXPECT_TRUE(http_request_.has_content);
  EXPECT_EQ("{\"id\":\"resource_id\"}", http_request_.content);
}

TEST_F(DriveApiRequestsTest, ChildrenDeleteRequest) {
  DriveApiErrorCode error = DRIVE_OTHER_ERROR;

  // Remove a resource with "resource_id" from a directory with
  // "parent_resource_id".
  {
    base::RunLoop run_loop;
    std::unique_ptr<drive::ChildrenDeleteRequest> request =
        std::make_unique<drive::ChildrenDeleteRequest>(
            request_sender_.get(), *url_generator_,
            test_util::CreateQuitCallback(
                &run_loop, test_util::CreateCopyResultCallback(&error)));
    request->set_child_id("resource_id");
    request->set_folder_id("parent_resource_id");
    request_sender_->StartRequestWithAuthRetry(std::move(request));
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_NO_CONTENT, error);
  EXPECT_EQ(net::test_server::METHOD_DELETE, http_request_.method);
  EXPECT_EQ("/drive/v2/files/parent_resource_id/children/resource_id",
            http_request_.relative_url);
  EXPECT_FALSE(http_request_.has_content);
}

TEST_F(DriveApiRequestsTest, UploadNewFileRequest) {
  // Set an expected url for uploading.
  expected_upload_path_ = kTestUploadNewFilePath;

  const char kTestContentType[] = "text/plain";
  const std::string kTestContent(100, 'a');
  const base::FilePath kTestFilePath =
      temp_dir_.GetPath().AppendASCII("upload_file.txt");
  ASSERT_TRUE(test_util::WriteStringToFile(kTestFilePath, kTestContent));

  DriveApiErrorCode error = DRIVE_OTHER_ERROR;
  GURL upload_url;

  // Initiate uploading a new file to the directory with
  // "parent_resource_id".
  {
    base::RunLoop run_loop;
    std::unique_ptr<drive::InitiateUploadNewFileRequest> request =
        std::make_unique<drive::InitiateUploadNewFileRequest>(
            request_sender_.get(), *url_generator_, kTestContentType,
            kTestContent.size(),
            "parent_resource_id",  // The resource id of the parent directory.
            "new file title",      // The title of the file being uploaded.
            test_util::CreateQuitCallback(
                &run_loop,
                test_util::CreateCopyResultCallback(&error, &upload_url)));
    request->set_properties(testing_properties_);
    request_sender_->StartRequestWithAuthRetry(std::move(request));
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(kTestUploadNewFilePath, upload_url.path());
  EXPECT_EQ(kTestContentType, http_request_.headers["X-Upload-Content-Type"]);
  EXPECT_EQ(base::NumberToString(kTestContent.size()),
            http_request_.headers["X-Upload-Content-Length"]);

  EXPECT_EQ(net::test_server::METHOD_POST, http_request_.method);
  EXPECT_EQ(
      "/upload/drive/v2/files?uploadType=resumable"
      "&supportsTeamDrives=true",
      http_request_.relative_url);
  EXPECT_EQ("application/json", http_request_.headers["Content-Type"]);
  EXPECT_TRUE(http_request_.has_content);
  EXPECT_EQ(
      "{\"parents\":[{"
      "\"id\":\"parent_resource_id\","
      "\"kind\":\"drive#fileLink\""
      "}],"
      "\"properties\":["
      "{\"key\":\"key1\",\"value\":\"value1\",\"visibility\":\"PRIVATE\"},"
      "{\"key\":\"key2\",\"value\":\"value2\",\"visibility\":\"PUBLIC\"}],"
      "\"title\":\"new file title\"}",
      http_request_.content);

  // Upload the content to the upload URL.
  UploadRangeResponse response;
  std::unique_ptr<FileResource> new_entry;

  {
    base::RunLoop run_loop;
    std::unique_ptr<drive::ResumeUploadRequest> request =
        std::make_unique<drive::ResumeUploadRequest>(
            request_sender_.get(), upload_url,
            0,                    // start_position
            kTestContent.size(),  // end_position (exclusive)
            kTestContent.size(),  // content_length,
            kTestContentType, kTestFilePath,
            test_util::CreateQuitCallback(
                &run_loop,
                test_util::CreateCopyResultCallback(&response, &new_entry)),
            ProgressCallback());
    request_sender_->StartRequestWithAuthRetry(std::move(request));
    run_loop.Run();
  }

  // METHOD_PUT should be used to upload data.
  EXPECT_EQ(net::test_server::METHOD_PUT, http_request_.method);
  // Request should go to the upload URL.
  EXPECT_EQ(upload_url.path(), http_request_.relative_url);
  // Content-Range header should be added.
  EXPECT_EQ("bytes 0-" + base::NumberToString(kTestContent.size() - 1) + "/" +
                base::NumberToString(kTestContent.size()),
            http_request_.headers["Content-Range"]);
  // The upload content should be set in the HTTP request.
  EXPECT_TRUE(http_request_.has_content);
  EXPECT_EQ(kTestContent, http_request_.content);

  // Check the response.
  EXPECT_EQ(HTTP_CREATED, response.code);  // Because it's a new file
  // The start and end positions should be set to -1, if an upload is complete.
  EXPECT_EQ(-1, response.start_position_received);
  EXPECT_EQ(-1, response.end_position_received);
}

TEST_F(DriveApiRequestsTest, UploadNewEmptyFileRequest) {
  // Set an expected url for uploading.
  expected_upload_path_ = kTestUploadNewFilePath;

  const char kTestContentType[] = "text/plain";
  const char kTestContent[] = "";
  const base::FilePath kTestFilePath =
      temp_dir_.GetPath().AppendASCII("empty_file.txt");
  ASSERT_TRUE(test_util::WriteStringToFile(kTestFilePath, kTestContent));

  DriveApiErrorCode error = DRIVE_OTHER_ERROR;
  GURL upload_url;

  // Initiate uploading a new file to the directory with "parent_resource_id".
  {
    base::RunLoop run_loop;
    std::unique_ptr<drive::InitiateUploadNewFileRequest> request =
        std::make_unique<drive::InitiateUploadNewFileRequest>(
            request_sender_.get(), *url_generator_, kTestContentType, 0,
            "parent_resource_id",  // The resource id of the parent directory.
            "new file title",      // The title of the file being uploaded.
            test_util::CreateQuitCallback(
                &run_loop,
                test_util::CreateCopyResultCallback(&error, &upload_url)));
    request_sender_->StartRequestWithAuthRetry(std::move(request));
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(kTestUploadNewFilePath, upload_url.path());
  EXPECT_EQ(kTestContentType, http_request_.headers["X-Upload-Content-Type"]);
  EXPECT_EQ("0", http_request_.headers["X-Upload-Content-Length"]);

  EXPECT_EQ(net::test_server::METHOD_POST, http_request_.method);
  EXPECT_EQ(
      "/upload/drive/v2/files?uploadType=resumable"
      "&supportsTeamDrives=true",
      http_request_.relative_url);
  EXPECT_EQ("application/json", http_request_.headers["Content-Type"]);
  EXPECT_TRUE(http_request_.has_content);
  EXPECT_EQ("{\"parents\":[{"
            "\"id\":\"parent_resource_id\","
            "\"kind\":\"drive#fileLink\""
            "}],"
            "\"title\":\"new file title\"}",
            http_request_.content);

  // Upload the content to the upload URL.
  UploadRangeResponse response;
  std::unique_ptr<FileResource> new_entry;

  {
    base::RunLoop run_loop;
    std::unique_ptr<drive::ResumeUploadRequest> request =
        std::make_unique<drive::ResumeUploadRequest>(
            request_sender_.get(), upload_url,
            0,  // start_position
            0,  // end_position (exclusive)
            0,  // content_length,
            kTestContentType, kTestFilePath,
            test_util::CreateQuitCallback(
                &run_loop,
                test_util::CreateCopyResultCallback(&response, &new_entry)),
            ProgressCallback());
    request_sender_->StartRequestWithAuthRetry(std::move(request));
    run_loop.Run();
  }

  // METHOD_PUT should be used to upload data.
  EXPECT_EQ(net::test_server::METHOD_PUT, http_request_.method);
  // Request should go to the upload URL.
  EXPECT_EQ(upload_url.path(), http_request_.relative_url);
  // Content-Range header should NOT be added.
  EXPECT_EQ(0U, http_request_.headers.count("Content-Range"));
  // The upload content should be set in the HTTP request.
  EXPECT_TRUE(http_request_.has_content);
  EXPECT_EQ(kTestContent, http_request_.content);

  // Check the response.
  EXPECT_EQ(HTTP_CREATED, response.code);  // Because it's a new file
  // The start and end positions should be set to -1, if an upload is complete.
  EXPECT_EQ(-1, response.start_position_received);
  EXPECT_EQ(-1, response.end_position_received);
}

TEST_F(DriveApiRequestsTest, UploadNewLargeFileRequest) {
  // Set an expected url for uploading.
  expected_upload_path_ = kTestUploadNewFilePath;

  const char kTestContentType[] = "text/plain";
  const size_t kNumChunkBytes = 10;  // Num bytes in a chunk.
  const std::string kTestContent(100, 'a');
  const base::FilePath kTestFilePath =
      temp_dir_.GetPath().AppendASCII("upload_file.txt");
  ASSERT_TRUE(test_util::WriteStringToFile(kTestFilePath, kTestContent));

  DriveApiErrorCode error = DRIVE_OTHER_ERROR;
  GURL upload_url;

  // Initiate uploading a new file to the directory with "parent_resource_id".
  {
    base::RunLoop run_loop;
    std::unique_ptr<drive::InitiateUploadNewFileRequest> request =
        std::make_unique<drive::InitiateUploadNewFileRequest>(
            request_sender_.get(), *url_generator_, kTestContentType,
            kTestContent.size(),
            "parent_resource_id",  // The resource id of the parent directory.
            "new file title",      // The title of the file being uploaded.
            test_util::CreateQuitCallback(
                &run_loop,
                test_util::CreateCopyResultCallback(&error, &upload_url)));
    request_sender_->StartRequestWithAuthRetry(std::move(request));
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(kTestUploadNewFilePath, upload_url.path());
  EXPECT_EQ(kTestContentType, http_request_.headers["X-Upload-Content-Type"]);
  EXPECT_EQ(base::NumberToString(kTestContent.size()),
            http_request_.headers["X-Upload-Content-Length"]);

  EXPECT_EQ(net::test_server::METHOD_POST, http_request_.method);
  EXPECT_EQ(
      "/upload/drive/v2/files?uploadType=resumable"
      "&supportsTeamDrives=true",
      http_request_.relative_url);
  EXPECT_EQ("application/json", http_request_.headers["Content-Type"]);
  EXPECT_TRUE(http_request_.has_content);
  EXPECT_EQ("{\"parents\":[{"
            "\"id\":\"parent_resource_id\","
            "\"kind\":\"drive#fileLink\""
            "}],"
            "\"title\":\"new file title\"}",
            http_request_.content);

  // Before sending any data, check the current status.
  // This is an edge case test for GetUploadStatusRequest.
  {
    UploadRangeResponse response;
    std::unique_ptr<FileResource> new_entry;

    // Check the response by GetUploadStatusRequest.
    {
      base::RunLoop run_loop;
      std::unique_ptr<drive::GetUploadStatusRequest> request =
          std::make_unique<drive::GetUploadStatusRequest>(
              request_sender_.get(), upload_url, kTestContent.size(),
              test_util::CreateQuitCallback(
                  &run_loop,
                  test_util::CreateCopyResultCallback(&response, &new_entry)));
      request_sender_->StartRequestWithAuthRetry(std::move(request));
      run_loop.Run();
    }

    // METHOD_PUT should be used to upload data.
    EXPECT_EQ(net::test_server::METHOD_PUT, http_request_.method);
    // Request should go to the upload URL.
    EXPECT_EQ(upload_url.path(), http_request_.relative_url);
    // Content-Range header should be added.
    EXPECT_EQ("bytes */" + base::NumberToString(kTestContent.size()),
              http_request_.headers["Content-Range"]);
    EXPECT_TRUE(http_request_.has_content);
    EXPECT_TRUE(http_request_.content.empty());

    // Check the response.
    EXPECT_EQ(HTTP_RESUME_INCOMPLETE, response.code);
    EXPECT_EQ(0, response.start_position_received);
    EXPECT_EQ(0, response.end_position_received);
  }

  // Upload the content to the upload URL.
  for (size_t start_position = 0; start_position < kTestContent.size();
       start_position += kNumChunkBytes) {
    const std::string payload = kTestContent.substr(
        start_position,
        std::min(kNumChunkBytes, kTestContent.size() - start_position));
    const size_t end_position = start_position + payload.size();

    UploadRangeResponse response;
    std::unique_ptr<FileResource> new_entry;

    {
      base::RunLoop run_loop;
      std::unique_ptr<drive::ResumeUploadRequest> request =
          std::make_unique<drive::ResumeUploadRequest>(
              request_sender_.get(), upload_url, start_position, end_position,
              kTestContent.size(),  // content_length,
              kTestContentType, kTestFilePath,
              test_util::CreateQuitCallback(
                  &run_loop,
                  test_util::CreateCopyResultCallback(&response, &new_entry)),
              ProgressCallback());
      request_sender_->StartRequestWithAuthRetry(std::move(request));
      run_loop.Run();
    }

    // METHOD_PUT should be used to upload data.
    EXPECT_EQ(net::test_server::METHOD_PUT, http_request_.method);
    // Request should go to the upload URL.
    EXPECT_EQ(upload_url.path(), http_request_.relative_url);
    // Content-Range header should be added.
    EXPECT_EQ("bytes " + base::NumberToString(start_position) + "-" +
                  base::NumberToString(end_position - 1) + "/" +
                  base::NumberToString(kTestContent.size()),
              http_request_.headers["Content-Range"]);
    // The upload content should be set in the HTTP request.
    EXPECT_TRUE(http_request_.has_content);
    EXPECT_EQ(payload, http_request_.content);

    if (end_position == kTestContent.size()) {
      // Check the response.
      EXPECT_EQ(HTTP_CREATED, response.code);  // Because it's a new file
      // The start and end positions should be set to -1, if an upload is
      // complete.
      EXPECT_EQ(-1, response.start_position_received);
      EXPECT_EQ(-1, response.end_position_received);
      break;
    }

    // Check the response.
    EXPECT_EQ(HTTP_RESUME_INCOMPLETE, response.code);
    EXPECT_EQ(0, response.start_position_received);
    EXPECT_EQ(static_cast<int64_t>(end_position),
              response.end_position_received);

    // Check the response by GetUploadStatusRequest.
    {
      base::RunLoop run_loop;
      std::unique_ptr<drive::GetUploadStatusRequest> request =
          std::make_unique<drive::GetUploadStatusRequest>(
              request_sender_.get(), upload_url, kTestContent.size(),
              test_util::CreateQuitCallback(
                  &run_loop,
                  test_util::CreateCopyResultCallback(&response, &new_entry)));
      request_sender_->StartRequestWithAuthRetry(std::move(request));
      run_loop.Run();
    }

    // METHOD_PUT should be used to upload data.
    EXPECT_EQ(net::test_server::METHOD_PUT, http_request_.method);
    // Request should go to the upload URL.
    EXPECT_EQ(upload_url.path(), http_request_.relative_url);
    // Content-Range header should be added.
    EXPECT_EQ("bytes */" + base::NumberToString(kTestContent.size()),
              http_request_.headers["Content-Range"]);
    EXPECT_TRUE(http_request_.has_content);
    EXPECT_TRUE(http_request_.content.empty());

    // Check the response.
    EXPECT_EQ(HTTP_RESUME_INCOMPLETE, response.code);
    EXPECT_EQ(0, response.start_position_received);
    EXPECT_EQ(static_cast<int64_t>(end_position),
              response.end_position_received);
  }
}

TEST_F(DriveApiRequestsTest, UploadNewFileWithMetadataRequest) {
  const base::Time::Exploded kModifiedDate = {2012, 7, 0, 19, 15, 59, 13, 123};
  const base::Time::Exploded kLastViewedByMeDate =
      {2013, 7, 0, 19, 15, 59, 13, 123};

  // Set an expected url for uploading.
  expected_upload_path_ = kTestUploadNewFilePath;

  const char kTestContentType[] = "text/plain";
  const std::string kTestContent(100, 'a');

  DriveApiErrorCode error = DRIVE_OTHER_ERROR;
  GURL upload_url;

  // Initiate uploading a new file to the directory with "parent_resource_id".
  {
    base::RunLoop run_loop;
    std::unique_ptr<drive::InitiateUploadNewFileRequest> request =
        std::make_unique<drive::InitiateUploadNewFileRequest>(
            request_sender_.get(), *url_generator_, kTestContentType,
            kTestContent.size(),
            "parent_resource_id",  // The resource id of the parent directory.
            "new file title",      // The title of the file being uploaded.
            test_util::CreateQuitCallback(
                &run_loop,
                test_util::CreateCopyResultCallback(&error, &upload_url)));
    base::Time modified_date_utc;
    ASSERT_TRUE(base::Time::FromUTCExploded(kModifiedDate, &modified_date_utc));

    request->set_modified_date(modified_date_utc);

    base::Time last_viewed_by_me_date_utc;
    ASSERT_TRUE(base::Time::FromUTCExploded(kLastViewedByMeDate,
                                            &last_viewed_by_me_date_utc));
    request->set_last_viewed_by_me_date(last_viewed_by_me_date_utc);
    request_sender_->StartRequestWithAuthRetry(std::move(request));
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(kTestUploadNewFilePath, upload_url.path());
  EXPECT_EQ(kTestContentType, http_request_.headers["X-Upload-Content-Type"]);
  EXPECT_EQ(base::NumberToString(kTestContent.size()),
            http_request_.headers["X-Upload-Content-Length"]);

  EXPECT_EQ(net::test_server::METHOD_POST, http_request_.method);
  EXPECT_EQ(
      "/upload/drive/v2/files?uploadType=resumable"
      "&supportsTeamDrives=true&setModifiedDate=true",
      http_request_.relative_url);
  EXPECT_EQ("application/json", http_request_.headers["Content-Type"]);
  EXPECT_TRUE(http_request_.has_content);
  EXPECT_EQ("{\"lastViewedByMeDate\":\"2013-07-19T15:59:13.123Z\","
            "\"modifiedDate\":\"2012-07-19T15:59:13.123Z\","
            "\"parents\":[{\"id\":\"parent_resource_id\","
            "\"kind\":\"drive#fileLink\"}],"
            "\"title\":\"new file title\"}",
            http_request_.content);
}

TEST_F(DriveApiRequestsTest, UploadExistingFileRequest) {
  // Set an expected url for uploading.
  expected_upload_path_ = kTestUploadExistingFilePath;

  const char kTestContentType[] = "text/plain";
  const std::string kTestContent(100, 'a');
  const base::FilePath kTestFilePath =
      temp_dir_.GetPath().AppendASCII("upload_file.txt");
  ASSERT_TRUE(test_util::WriteStringToFile(kTestFilePath, kTestContent));

  DriveApiErrorCode error = DRIVE_OTHER_ERROR;
  GURL upload_url;

  // Initiate uploading a new file to the directory with "parent_resource_id".
  {
    base::RunLoop run_loop;
    std::unique_ptr<drive::InitiateUploadExistingFileRequest> request =
        std::make_unique<drive::InitiateUploadExistingFileRequest>(
            request_sender_.get(), *url_generator_, kTestContentType,
            kTestContent.size(),
            "resource_id",  // The resource id of the file to be overwritten.
            std::string(),  // No etag.
            test_util::CreateQuitCallback(
                &run_loop,
                test_util::CreateCopyResultCallback(&error, &upload_url)));
    request->set_properties(testing_properties_);
    request_sender_->StartRequestWithAuthRetry(std::move(request));
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(kTestUploadExistingFilePath, upload_url.path());
  EXPECT_EQ(kTestContentType, http_request_.headers["X-Upload-Content-Type"]);
  EXPECT_EQ(base::NumberToString(kTestContent.size()),
            http_request_.headers["X-Upload-Content-Length"]);
  EXPECT_EQ("*", http_request_.headers["If-Match"]);

  EXPECT_EQ(net::test_server::METHOD_PUT, http_request_.method);
  EXPECT_EQ(
      "/upload/drive/v2/files/resource_id?uploadType=resumable"
      "&supportsTeamDrives=true",
      http_request_.relative_url);
  EXPECT_TRUE(http_request_.has_content);
  EXPECT_EQ(
      "{\"properties\":["
      "{\"key\":\"key1\",\"value\":\"value1\",\"visibility\":\"PRIVATE\"},"
      "{\"key\":\"key2\",\"value\":\"value2\",\"visibility\":\"PUBLIC\"}]}",
      http_request_.content);

  // Upload the content to the upload URL.
  UploadRangeResponse response;
  std::unique_ptr<FileResource> new_entry;

  {
    base::RunLoop run_loop;
    std::unique_ptr<drive::ResumeUploadRequest> request =
        std::make_unique<drive::ResumeUploadRequest>(
            request_sender_.get(), upload_url,
            0,                    // start_position
            kTestContent.size(),  // end_position (exclusive)
            kTestContent.size(),  // content_length,
            kTestContentType, kTestFilePath,
            test_util::CreateQuitCallback(
                &run_loop,
                test_util::CreateCopyResultCallback(&response, &new_entry)),
            ProgressCallback());
    request_sender_->StartRequestWithAuthRetry(std::move(request));
    run_loop.Run();
  }

  // METHOD_PUT should be used to upload data.
  EXPECT_EQ(net::test_server::METHOD_PUT, http_request_.method);
  // Request should go to the upload URL.
  EXPECT_EQ(upload_url.path(), http_request_.relative_url);
  // Content-Range header should be added.
  EXPECT_EQ("bytes 0-" + base::NumberToString(kTestContent.size() - 1) + "/" +
                base::NumberToString(kTestContent.size()),
            http_request_.headers["Content-Range"]);
  // The upload content should be set in the HTTP request.
  EXPECT_TRUE(http_request_.has_content);
  EXPECT_EQ(kTestContent, http_request_.content);

  // Check the response.
  EXPECT_EQ(HTTP_SUCCESS, response.code);  // Because it's an existing file
  // The start and end positions should be set to -1, if an upload is complete.
  EXPECT_EQ(-1, response.start_position_received);
  EXPECT_EQ(-1, response.end_position_received);
}

TEST_F(DriveApiRequestsTest, UploadExistingFileRequestWithETag) {
  // Set an expected url for uploading.
  expected_upload_path_ = kTestUploadExistingFilePath;

  const char kTestContentType[] = "text/plain";
  const std::string kTestContent(100, 'a');
  const base::FilePath kTestFilePath =
      temp_dir_.GetPath().AppendASCII("upload_file.txt");
  ASSERT_TRUE(test_util::WriteStringToFile(kTestFilePath, kTestContent));

  DriveApiErrorCode error = DRIVE_OTHER_ERROR;
  GURL upload_url;

  // Initiate uploading a new file to the directory with "parent_resource_id".
  {
    base::RunLoop run_loop;
    std::unique_ptr<drive::InitiateUploadExistingFileRequest> request =
        std::make_unique<drive::InitiateUploadExistingFileRequest>(
            request_sender_.get(), *url_generator_, kTestContentType,
            kTestContent.size(),
            "resource_id",  // The resource id of the file to be overwritten.
            kTestETag,
            test_util::CreateQuitCallback(
                &run_loop,
                test_util::CreateCopyResultCallback(&error, &upload_url)));
    request_sender_->StartRequestWithAuthRetry(std::move(request));
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(kTestUploadExistingFilePath, upload_url.path());
  EXPECT_EQ(kTestContentType, http_request_.headers["X-Upload-Content-Type"]);
  EXPECT_EQ(base::NumberToString(kTestContent.size()),
            http_request_.headers["X-Upload-Content-Length"]);
  EXPECT_EQ(kTestETag, http_request_.headers["If-Match"]);

  EXPECT_EQ(net::test_server::METHOD_PUT, http_request_.method);
  EXPECT_EQ(
      "/upload/drive/v2/files/resource_id?uploadType=resumable"
      "&supportsTeamDrives=true",
      http_request_.relative_url);
  EXPECT_TRUE(http_request_.has_content);
  EXPECT_TRUE(http_request_.content.empty());

  // Upload the content to the upload URL.
  UploadRangeResponse response;
  std::unique_ptr<FileResource> new_entry;

  {
    base::RunLoop run_loop;
    std::unique_ptr<drive::ResumeUploadRequest> request =
        std::make_unique<drive::ResumeUploadRequest>(
            request_sender_.get(), upload_url,
            0,                    // start_position
            kTestContent.size(),  // end_position (exclusive)
            kTestContent.size(),  // content_length,
            kTestContentType, kTestFilePath,
            test_util::CreateQuitCallback(
                &run_loop,
                test_util::CreateCopyResultCallback(&response, &new_entry)),
            ProgressCallback());
    request_sender_->StartRequestWithAuthRetry(std::move(request));
    run_loop.Run();
  }

  // METHOD_PUT should be used to upload data.
  EXPECT_EQ(net::test_server::METHOD_PUT, http_request_.method);
  // Request should go to the upload URL.
  EXPECT_EQ(upload_url.path(), http_request_.relative_url);
  // Content-Range header should be added.
  EXPECT_EQ("bytes 0-" + base::NumberToString(kTestContent.size() - 1) + "/" +
                base::NumberToString(kTestContent.size()),
            http_request_.headers["Content-Range"]);
  // The upload content should be set in the HTTP request.
  EXPECT_TRUE(http_request_.has_content);
  EXPECT_EQ(kTestContent, http_request_.content);

  // Check the response.
  EXPECT_EQ(HTTP_SUCCESS, response.code);  // Because it's an existing file
  // The start and end positions should be set to -1, if an upload is complete.
  EXPECT_EQ(-1, response.start_position_received);
  EXPECT_EQ(-1, response.end_position_received);
}

TEST_F(DriveApiRequestsTest, UploadExistingFileRequestWithETagConflicting) {
  // Set an expected url for uploading.
  expected_upload_path_ = kTestUploadExistingFilePath;

  // If it turned out that the etag is conflicting, PRECONDITION_FAILED should
  // be returned.
  expected_precondition_failed_file_path_ =
      test_util::GetTestFilePath("drive/error.json");

  const char kTestContentType[] = "text/plain";
  const std::string kTestContent(100, 'a');

  DriveApiErrorCode error = DRIVE_OTHER_ERROR;
  GURL upload_url;

  // Initiate uploading a new file to the directory with "parent_resource_id".
  {
    base::RunLoop run_loop;
    std::unique_ptr<drive::InitiateUploadExistingFileRequest> request =
        std::make_unique<drive::InitiateUploadExistingFileRequest>(
            request_sender_.get(), *url_generator_, kTestContentType,
            kTestContent.size(),
            "resource_id",  // The resource id of the file to be overwritten.
            "Conflicting-etag",
            test_util::CreateQuitCallback(
                &run_loop,
                test_util::CreateCopyResultCallback(&error, &upload_url)));
    request_sender_->StartRequestWithAuthRetry(std::move(request));
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_PRECONDITION, error);
  EXPECT_EQ(kTestContentType, http_request_.headers["X-Upload-Content-Type"]);
  EXPECT_EQ(base::NumberToString(kTestContent.size()),
            http_request_.headers["X-Upload-Content-Length"]);
  EXPECT_EQ("Conflicting-etag", http_request_.headers["If-Match"]);

  EXPECT_EQ(net::test_server::METHOD_PUT, http_request_.method);
  EXPECT_EQ(
      "/upload/drive/v2/files/resource_id?uploadType=resumable"
      "&supportsTeamDrives=true",
      http_request_.relative_url);
  EXPECT_TRUE(http_request_.has_content);
  EXPECT_TRUE(http_request_.content.empty());
}

TEST_F(DriveApiRequestsTest,
       UploadExistingFileRequestWithETagConflictOnResumeUpload) {
  // Set an expected url for uploading.
  expected_upload_path_ = kTestUploadExistingFilePath;

  const char kTestContentType[] = "text/plain";
  const std::string kTestContent(100, 'a');
  const base::FilePath kTestFilePath =
      temp_dir_.GetPath().AppendASCII("upload_file.txt");
  ASSERT_TRUE(test_util::WriteStringToFile(kTestFilePath, kTestContent));

  DriveApiErrorCode error = DRIVE_OTHER_ERROR;
  GURL upload_url;

  // Initiate uploading a new file to the directory with "parent_resource_id".
  {
    base::RunLoop run_loop;
    std::unique_ptr<drive::InitiateUploadExistingFileRequest> request =
        std::make_unique<drive::InitiateUploadExistingFileRequest>(
            request_sender_.get(), *url_generator_, kTestContentType,
            kTestContent.size(),
            "resource_id",  // The resource id of the file to be overwritten.
            kTestETag,
            test_util::CreateQuitCallback(
                &run_loop,
                test_util::CreateCopyResultCallback(&error, &upload_url)));
    request_sender_->StartRequestWithAuthRetry(std::move(request));
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(kTestUploadExistingFilePath, upload_url.path());
  EXPECT_EQ(kTestContentType, http_request_.headers["X-Upload-Content-Type"]);
  EXPECT_EQ(base::NumberToString(kTestContent.size()),
            http_request_.headers["X-Upload-Content-Length"]);
  EXPECT_EQ(kTestETag, http_request_.headers["If-Match"]);

  EXPECT_EQ(net::test_server::METHOD_PUT, http_request_.method);
  EXPECT_EQ(
      "/upload/drive/v2/files/resource_id?uploadType=resumable"
      "&supportsTeamDrives=true",
      http_request_.relative_url);
  EXPECT_TRUE(http_request_.has_content);
  EXPECT_TRUE(http_request_.content.empty());

  // Set PRECONDITION_FAILED to the server. This is the emulation of the
  // confliction during uploading.
  expected_precondition_failed_file_path_ =
      test_util::GetTestFilePath("drive/error.json");

  // Upload the content to the upload URL.
  UploadRangeResponse response;
  std::unique_ptr<FileResource> new_entry;

  {
    base::RunLoop run_loop;
    std::unique_ptr<drive::ResumeUploadRequest> resume_request =
        std::make_unique<drive::ResumeUploadRequest>(
            request_sender_.get(), upload_url,
            0,                    // start_position
            kTestContent.size(),  // end_position (exclusive)
            kTestContent.size(),  // content_length,
            kTestContentType, kTestFilePath,
            test_util::CreateQuitCallback(
                &run_loop,
                test_util::CreateCopyResultCallback(&response, &new_entry)),
            ProgressCallback());
    request_sender_->StartRequestWithAuthRetry(std::move(resume_request));
    run_loop.Run();
  }

  // METHOD_PUT should be used to upload data.
  EXPECT_EQ(net::test_server::METHOD_PUT, http_request_.method);
  // Request should go to the upload URL.
  EXPECT_EQ(upload_url.path(), http_request_.relative_url);
  // Content-Range header should be added.
  EXPECT_EQ("bytes 0-" + base::NumberToString(kTestContent.size() - 1) + "/" +
                base::NumberToString(kTestContent.size()),
            http_request_.headers["Content-Range"]);
  // The upload content should be set in the HTTP request.
  EXPECT_TRUE(http_request_.has_content);
  EXPECT_EQ(kTestContent, http_request_.content);

  // Check the response.
  EXPECT_EQ(HTTP_PRECONDITION, response.code);
  // The start and end positions should be set to -1 for error.
  EXPECT_EQ(-1, response.start_position_received);
  EXPECT_EQ(-1, response.end_position_received);

  // New entry should be NULL.
  EXPECT_FALSE(new_entry.get());
}

TEST_F(DriveApiRequestsTest, UploadExistingFileWithMetadataRequest) {
  const base::Time::Exploded kModifiedDate = {2012, 7, 0, 19, 15, 59, 13, 123};
  const base::Time::Exploded kLastViewedByMeDate =
      {2013, 7, 0, 19, 15, 59, 13, 123};

  // Set an expected url for uploading.
  expected_upload_path_ = kTestUploadExistingFilePath;

  const char kTestContentType[] = "text/plain";
  const std::string kTestContent(100, 'a');

  DriveApiErrorCode error = DRIVE_OTHER_ERROR;
  GURL upload_url;

  // Initiate uploading a new file to the directory with "parent_resource_id".
  {
    base::RunLoop run_loop;
    std::unique_ptr<drive::InitiateUploadExistingFileRequest> request =
        std::make_unique<drive::InitiateUploadExistingFileRequest>(
            request_sender_.get(), *url_generator_, kTestContentType,
            kTestContent.size(),
            "resource_id",  // The resource id of the file to be overwritten.
            kTestETag,
            test_util::CreateQuitCallback(
                &run_loop,
                test_util::CreateCopyResultCallback(&error, &upload_url)));
    request->set_parent_resource_id("new_parent_resource_id");
    request->set_title("new file title");
    base::Time modified_date_utc;
    ASSERT_TRUE(base::Time::FromUTCExploded(kModifiedDate, &modified_date_utc));

    request->set_modified_date(modified_date_utc);

    base::Time last_viewed_by_me_date_utc;
    ASSERT_TRUE(base::Time::FromUTCExploded(kLastViewedByMeDate,
                                            &last_viewed_by_me_date_utc));
    request->set_last_viewed_by_me_date(last_viewed_by_me_date_utc);
    request_sender_->StartRequestWithAuthRetry(std::move(request));
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(kTestUploadExistingFilePath, upload_url.path());
  EXPECT_EQ(kTestContentType, http_request_.headers["X-Upload-Content-Type"]);
  EXPECT_EQ(base::NumberToString(kTestContent.size()),
            http_request_.headers["X-Upload-Content-Length"]);
  EXPECT_EQ(kTestETag, http_request_.headers["If-Match"]);

  EXPECT_EQ(net::test_server::METHOD_PUT, http_request_.method);
  EXPECT_EQ(
      "/upload/drive/v2/files/resource_id?"
      "uploadType=resumable&supportsTeamDrives=true&setModifiedDate=true",
      http_request_.relative_url);
  EXPECT_EQ("application/json", http_request_.headers["Content-Type"]);
  EXPECT_TRUE(http_request_.has_content);
  EXPECT_EQ("{\"lastViewedByMeDate\":\"2013-07-19T15:59:13.123Z\","
            "\"modifiedDate\":\"2012-07-19T15:59:13.123Z\","
            "\"parents\":[{\"id\":\"new_parent_resource_id\","
            "\"kind\":\"drive#fileLink\"}],"
            "\"title\":\"new file title\"}",
            http_request_.content);
}

TEST_F(DriveApiRequestsTest, DownloadFileRequest) {
  const base::FilePath kDownloadedFilePath =
      temp_dir_.GetPath().AppendASCII("cache_file");
  const std::string kTestId("dummyId");

  DriveApiErrorCode result_code = DRIVE_OTHER_ERROR;
  base::FilePath temp_file;
  {
    base::RunLoop run_loop;
    std::unique_ptr<drive::DownloadFileRequest> request =
        std::make_unique<drive::DownloadFileRequest>(
            request_sender_.get(), *url_generator_, kTestId,
            kDownloadedFilePath,
            test_util::CreateQuitCallback(
                &run_loop,
                test_util::CreateCopyResultCallback(&result_code, &temp_file)),
            GetContentCallback(), ProgressCallback());
    request_sender_->StartRequestWithAuthRetry(std::move(request));
    run_loop.Run();
  }

  std::string contents;
  base::ReadFileToString(temp_file, &contents);
  base::DeleteFile(temp_file, false);

  EXPECT_EQ(HTTP_SUCCESS, result_code);
  EXPECT_EQ(net::test_server::METHOD_GET, http_request_.method);
  EXPECT_EQ(kTestDownloadPathPrefix + kTestId + "?" + kTestDownloadFileQuery,
            http_request_.relative_url);
  EXPECT_EQ(kDownloadedFilePath, temp_file);

  const std::string expected_contents = kTestId + kTestId + kTestId;
  EXPECT_EQ(expected_contents, contents);
}

TEST_F(DriveApiRequestsTest, DownloadFileRequest_GetContentCallback) {
  const base::FilePath kDownloadedFilePath =
      temp_dir_.GetPath().AppendASCII("cache_file");
  const std::string kTestId("dummyId");

  DriveApiErrorCode result_code = DRIVE_OTHER_ERROR;
  base::FilePath temp_file;
  std::string contents;
  {
    base::RunLoop run_loop;
    std::unique_ptr<drive::DownloadFileRequest> request =
        std::make_unique<drive::DownloadFileRequest>(
            request_sender_.get(), *url_generator_, kTestId,
            kDownloadedFilePath,
            test_util::CreateQuitCallback(
                &run_loop,
                test_util::CreateCopyResultCallback(&result_code, &temp_file)),
            base::Bind(&AppendContent, &contents), ProgressCallback());
    request_sender_->StartRequestWithAuthRetry(std::move(request));
    run_loop.Run();
  }

  base::DeleteFile(temp_file, false);

  EXPECT_EQ(HTTP_SUCCESS, result_code);
  EXPECT_EQ(net::test_server::METHOD_GET, http_request_.method);
  EXPECT_EQ(kTestDownloadPathPrefix + kTestId + "?" + kTestDownloadFileQuery,
            http_request_.relative_url);
  EXPECT_EQ(kDownloadedFilePath, temp_file);

  const std::string expected_contents = kTestId + kTestId + kTestId;
  EXPECT_EQ(expected_contents, contents);
}

TEST_F(DriveApiRequestsTest, PermissionsInsertRequest) {
  expected_content_type_ = "application/json";
  expected_content_ = kTestPermissionResponse;

  DriveApiErrorCode error = DRIVE_OTHER_ERROR;

  // Add comment permission to the user "user@example.com".
  {
    base::RunLoop run_loop;
    std::unique_ptr<drive::PermissionsInsertRequest> request =
        std::make_unique<drive::PermissionsInsertRequest>(
            request_sender_.get(), *url_generator_,
            test_util::CreateQuitCallback(
                &run_loop, test_util::CreateCopyResultCallback(&error)));
    request->set_id("resource_id");
    request->set_role(drive::PERMISSION_ROLE_COMMENTER);
    request->set_type(drive::PERMISSION_TYPE_USER);
    request->set_value("user@example.com");
    request_sender_->StartRequestWithAuthRetry(std::move(request));
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(net::test_server::METHOD_POST, http_request_.method);
  EXPECT_EQ("/drive/v2/files/resource_id/permissions?supportsTeamDrives=true",
            http_request_.relative_url);
  EXPECT_EQ("application/json", http_request_.headers["Content-Type"]);

  std::unique_ptr<base::Value> expected = base::JSONReader::ReadDeprecated(
      "{\"additionalRoles\":[\"commenter\"], \"role\":\"reader\", "
      "\"type\":\"user\",\"value\":\"user@example.com\"}");
  ASSERT_TRUE(expected);

  std::unique_ptr<base::Value> result =
      base::JSONReader::ReadDeprecated(http_request_.content);
  EXPECT_TRUE(http_request_.has_content);
  EXPECT_EQ(*expected, *result);

  // Add "can edit" permission to users in "example.com".
  error = DRIVE_OTHER_ERROR;
  {
    base::RunLoop run_loop;
    std::unique_ptr<drive::PermissionsInsertRequest> request =
        std::make_unique<drive::PermissionsInsertRequest>(
            request_sender_.get(), *url_generator_,
            test_util::CreateQuitCallback(
                &run_loop, test_util::CreateCopyResultCallback(&error)));
    request->set_id("resource_id2");
    request->set_role(drive::PERMISSION_ROLE_WRITER);
    request->set_type(drive::PERMISSION_TYPE_DOMAIN);
    request->set_value("example.com");
    request_sender_->StartRequestWithAuthRetry(std::move(request));
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(net::test_server::METHOD_POST, http_request_.method);
  EXPECT_EQ("/drive/v2/files/resource_id2/permissions?supportsTeamDrives=true",
            http_request_.relative_url);
  EXPECT_EQ("application/json", http_request_.headers["Content-Type"]);

  expected = base::JSONReader::ReadDeprecated(
      "{\"role\":\"writer\", \"type\":\"domain\",\"value\":\"example.com\"}");
  ASSERT_TRUE(expected);

  result = base::JSONReader::ReadDeprecated(http_request_.content);
  EXPECT_TRUE(http_request_.has_content);
  EXPECT_EQ(*expected, *result);
}

TEST_F(DriveApiRequestsTest, BatchUploadRequest) {
  // Preapre constants.
  const char kTestContentType[] = "text/plain";
  const std::string kTestContent(10, 'a');
  const base::FilePath kTestFilePath =
      temp_dir_.GetPath().AppendASCII("upload_file.txt");
  ASSERT_TRUE(test_util::WriteStringToFile(kTestFilePath, kTestContent));

  // Create batch request.
  std::unique_ptr<drive::BatchUploadRequest> request =
      std::make_unique<drive::BatchUploadRequest>(request_sender_.get(),
                                                  *url_generator_);
  drive::BatchUploadRequest* request_ptr = request.get();
  request_ptr->SetBoundaryForTesting("OUTERBOUNDARY");
  request_sender_->StartRequestWithAuthRetry(std::move(request));

  // Create child request.
  DriveApiErrorCode errors[] = {DRIVE_OTHER_ERROR, DRIVE_OTHER_ERROR};
  std::unique_ptr<FileResource> file_resources[2];
  base::RunLoop run_loop[2];
  for (int i = 0; i < 2; ++i) {
    const FileResourceCallback callback = test_util::CreateQuitCallback(
        &run_loop[i],
        test_util::CreateCopyResultCallback(&errors[i], &file_resources[i]));
    drive::MultipartUploadNewFileDelegate* const child_request =
        new drive::MultipartUploadNewFileDelegate(
            request_sender_->blocking_task_runner(),
            base::StringPrintf("new file title %d", i),
            "parent_resource_id", kTestContentType, kTestContent.size(),
            base::Time(), base::Time(), kTestFilePath, drive::Properties(),
            *url_generator_, callback, ProgressCallback());
    child_request->SetBoundaryForTesting("INNERBOUNDARY");
    request_ptr->AddRequest(child_request);
  }
  request_ptr->Commit();
  run_loop[0].Run();
  run_loop[1].Run();

  EXPECT_EQ(net::test_server::METHOD_PUT, http_request_.method);
  EXPECT_EQ("batch", http_request_.headers["X-Goog-Upload-Protocol"]);
  EXPECT_EQ("multipart/mixed; boundary=OUTERBOUNDARY",
            http_request_.headers["Content-Type"]);
  EXPECT_EQ(
      "--OUTERBOUNDARY\n"
      "Content-Type: application/http\n"
      "\n"
      "POST /upload/drive/v2/files HTTP/1.1\n"
      "Host: 127.0.0.1\n"
      "X-Goog-Upload-Protocol: multipart\n"
      "Content-Type: multipart/related; boundary=INNERBOUNDARY\n"
      "\n"
      "--INNERBOUNDARY\n"
      "Content-Type: application/json\n"
      "\n"
      "{\"parents\":[{\"id\":\"parent_resource_id\","
      "\"kind\":\"drive#fileLink\"}],\"title\":\"new file title 0\"}\n"
      "--INNERBOUNDARY\n"
      "Content-Type: text/plain\n"
      "\n"
      "aaaaaaaaaa\n"
      "--INNERBOUNDARY--\n"
      "--OUTERBOUNDARY\n"
      "Content-Type: application/http\n"
      "\n"
      "POST /upload/drive/v2/files HTTP/1.1\n"
      "Host: 127.0.0.1\n"
      "X-Goog-Upload-Protocol: multipart\n"
      "Content-Type: multipart/related; boundary=INNERBOUNDARY\n"
      "\n"
      "--INNERBOUNDARY\n"
      "Content-Type: application/json\n"
      "\n"
      "{\"parents\":[{\"id\":\"parent_resource_id\","
      "\"kind\":\"drive#fileLink\"}],\"title\":\"new file title 1\"}\n"
      "--INNERBOUNDARY\n"
      "Content-Type: text/plain\n"
      "\n"
      "aaaaaaaaaa\n"
      "--INNERBOUNDARY--\n"
      "--OUTERBOUNDARY--",
      http_request_.content);
  EXPECT_EQ(HTTP_SUCCESS, errors[0]);
  ASSERT_TRUE(file_resources[0]);
  EXPECT_EQ("file_id_1", file_resources[0]->file_id());
  ASSERT_FALSE(file_resources[1]);
  EXPECT_EQ(HTTP_SERVICE_UNAVAILABLE, errors[1]);
}

TEST_F(DriveApiRequestsTest, BatchUploadRequestWithBodyIncludingZero) {
  // Create batch request.
  std::unique_ptr<drive::BatchUploadRequest> request =
      std::make_unique<drive::BatchUploadRequest>(request_sender_.get(),
                                                  *url_generator_);
  drive::BatchUploadRequest* request_ptr = request.get();
  request_ptr->SetBoundaryForTesting("OUTERBOUNDARY");
  request_sender_->StartRequestWithAuthRetry(std::move(request));

  // Create child request.
  {
    base::RunLoop loop;
    TestBatchableDelegate* const child_request = new TestBatchableDelegate(
        GURL("http://example.com/test"), "application/binary",
        std::string("Apple\0Orange\0", 13), loop.QuitClosure());
    request_ptr->AddRequest(child_request);
    request_ptr->Commit();
    loop.Run();
  }

  EXPECT_EQ(net::test_server::METHOD_PUT, http_request_.method);
  EXPECT_EQ("batch", http_request_.headers["X-Goog-Upload-Protocol"]);
  EXPECT_EQ("multipart/mixed; boundary=OUTERBOUNDARY",
            http_request_.headers["Content-Type"]);
  EXPECT_EQ(
      "--OUTERBOUNDARY\n"
      "Content-Type: application/http\n"
      "\n"
      "PUT /test HTTP/1.1\n"
      "Host: 127.0.0.1\n"
      "X-Goog-Upload-Protocol: multipart\n"
      "Content-Type: application/binary\n"
      "\n" +
          std::string("Apple\0Orange\0", 13) +
          "\n"
          "--OUTERBOUNDARY--",
      http_request_.content);
}

TEST_F(DriveApiRequestsTest, BatchUploadRequestProgress) {
  // Create batch request.
  std::unique_ptr<drive::BatchUploadRequest> request =
      std::make_unique<drive::BatchUploadRequest>(request_sender_.get(),
                                                  *url_generator_);
  TestBatchableDelegate* requests[] = {
      new TestBatchableDelegate(GURL("http://example.com/test"),
                                "application/binary", std::string(100, 'a'),
                                base::DoNothing()),
      new TestBatchableDelegate(GURL("http://example.com/test"),
                                "application/binary", std::string(50, 'b'),
                                base::DoNothing()),
      new TestBatchableDelegate(GURL("http://example.com/test"),
                                "application/binary", std::string(0, 'c'),
                                base::DoNothing())};
  const size_t kExpectedUploadDataPosition[] = {207, 515, 773};
  const size_t kExpectedUploadDataSize = 851;
  request->AddRequest(requests[0]);
  request->AddRequest(requests[1]);
  request->AddRequest(requests[2]);
  request->Commit();
  request->Prepare(base::DoNothing());

  request->OnUploadProgress(0, kExpectedUploadDataSize);
  request->OnUploadProgress(150, kExpectedUploadDataSize);
  EXPECT_EQ(0u, requests[0]->progress_values().size());
  EXPECT_EQ(0u, requests[1]->progress_values().size());
  EXPECT_EQ(0u, requests[2]->progress_values().size());
  request->OnUploadProgress(kExpectedUploadDataPosition[0],
                            kExpectedUploadDataSize);
  EXPECT_EQ(1u, requests[0]->progress_values().size());
  EXPECT_EQ(0u, requests[1]->progress_values().size());
  EXPECT_EQ(0u, requests[2]->progress_values().size());
  request->OnUploadProgress(kExpectedUploadDataPosition[0] + 50,
                            kExpectedUploadDataSize);
  EXPECT_EQ(2u, requests[0]->progress_values().size());
  EXPECT_EQ(0u, requests[1]->progress_values().size());
  EXPECT_EQ(0u, requests[2]->progress_values().size());
  request->OnUploadProgress(kExpectedUploadDataPosition[1] + 20,
                            kExpectedUploadDataSize);
  EXPECT_EQ(3u, requests[0]->progress_values().size());
  EXPECT_EQ(1u, requests[1]->progress_values().size());
  EXPECT_EQ(0u, requests[2]->progress_values().size());
  request->OnUploadProgress(kExpectedUploadDataPosition[2],
                            kExpectedUploadDataSize);
  EXPECT_EQ(3u, requests[0]->progress_values().size());
  EXPECT_EQ(2u, requests[1]->progress_values().size());
  EXPECT_EQ(1u, requests[2]->progress_values().size());
  request->OnUploadProgress(kExpectedUploadDataSize, kExpectedUploadDataSize);
  ASSERT_EQ(3u, requests[0]->progress_values().size());
  EXPECT_EQ(0, requests[0]->progress_values()[0]);
  EXPECT_EQ(50, requests[0]->progress_values()[1]);
  EXPECT_EQ(100, requests[0]->progress_values()[2]);
  ASSERT_EQ(2u, requests[1]->progress_values().size());
  EXPECT_EQ(20, requests[1]->progress_values()[0]);
  EXPECT_EQ(50, requests[1]->progress_values()[1]);
  ASSERT_EQ(1u, requests[2]->progress_values().size());
  EXPECT_EQ(0, requests[2]->progress_values()[0]);

  request->Cancel();
}

TEST(ParseMultipartResponseTest, Empty) {
  std::vector<drive::MultipartHttpResponse> parts;
  EXPECT_FALSE(drive::ParseMultipartResponse(
      "multipart/mixed; boundary=BOUNDARY", "", &parts));
  EXPECT_FALSE(drive::ParseMultipartResponse("multipart/mixed; boundary=",
                                             "CONTENT", &parts));
}

TEST(ParseMultipartResponseTest, Basic) {
  std::vector<drive::MultipartHttpResponse> parts;
  ASSERT_TRUE(
      drive::ParseMultipartResponse("multipart/mixed; boundary=BOUNDARY",
                                    "--BOUNDARY\r\n"
                                    "Content-Type: application/http\r\n"
                                    "\r\n"
                                    "HTTP/1.1 200 OK\r\n"
                                    "Header: value\r\n"
                                    "\r\n"
                                    "First line\r\n"
                                    "Second line\r\n"
                                    "--BOUNDARY\r\n"
                                    "Content-Type: application/http\r\n"
                                    "\r\n"
                                    "HTTP/1.1 404 Not Found\r\n"
                                    "Header: value\r\n"
                                    "--BOUNDARY--",
                                    &parts));
  ASSERT_EQ(2u, parts.size());
  EXPECT_EQ(HTTP_SUCCESS, parts[0].code);
  EXPECT_EQ("First line\r\nSecond line", parts[0].body);
  EXPECT_EQ(HTTP_NOT_FOUND, parts[1].code);
  EXPECT_EQ("", parts[1].body);
}

TEST(ParseMultipartResponseTest, InvalidStatusLine) {
  std::vector<drive::MultipartHttpResponse> parts;
  ASSERT_TRUE(
      drive::ParseMultipartResponse("multipart/mixed; boundary=BOUNDARY",
                                    "--BOUNDARY\r\n"
                                    "Content-Type: application/http\r\n"
                                    "\r\n"
                                    "InvalidStatusLine 200 \r\n"
                                    "Header: value\r\n"
                                    "\r\n"
                                    "{}\r\n"
                                    "--BOUNDARY--",
                                    &parts));
  ASSERT_EQ(1u, parts.size());
  EXPECT_EQ(DRIVE_PARSE_ERROR, parts[0].code);
  EXPECT_EQ("{}", parts[0].body);
}

TEST(ParseMultipartResponseTest, BoundaryInTheBodyAndPreamble) {
  std::vector<drive::MultipartHttpResponse> parts;
  ASSERT_TRUE(
      drive::ParseMultipartResponse("multipart/mixed; boundary=BOUNDARY",
                                    "BOUNDARY\r\n"
                                    "PREUMBLE\r\n"
                                    "--BOUNDARY\r\n"
                                    "Content-Type: application/http\r\n"
                                    "\r\n"
                                    "HTTP/1.1 200 OK\r\n"
                                    "Header: value\r\n"
                                    "\r\n"
                                    "{--BOUNDARY}\r\n"
                                    "--BOUNDARY--",
                                    &parts));
  ASSERT_EQ(1u, parts.size());
  EXPECT_EQ(HTTP_SUCCESS, parts[0].code);
  EXPECT_EQ("{--BOUNDARY}", parts[0].body);
}

TEST(ParseMultipartResponseTest, QuatedBoundary) {
  std::vector<drive::MultipartHttpResponse> parts;
  ASSERT_TRUE(
      drive::ParseMultipartResponse("multipart/mixed; boundary=\"BOUNDARY\"",
                                    "--BOUNDARY\r\n"
                                    "Content-Type: application/http\r\n"
                                    "\r\n"
                                    "HTTP/1.1 200 OK\r\n"
                                    "Header: value\r\n"
                                    "\r\n"
                                    "BODY\r\n"
                                    "--BOUNDARY--",
                                    &parts));
  ASSERT_EQ(1u, parts.size());
  EXPECT_EQ(HTTP_SUCCESS, parts[0].code);
  EXPECT_EQ("BODY", parts[0].body);
}

TEST(ParseMultipartResponseTest, BoundaryWithTransportPadding) {
  std::vector<drive::MultipartHttpResponse> parts;
  ASSERT_TRUE(
      drive::ParseMultipartResponse("multipart/mixed; boundary=BOUNDARY",
                                    "--BOUNDARY \t\r\n"
                                    "Content-Type: application/http\r\n"
                                    "\r\n"
                                    "HTTP/1.1 200 OK\r\n"
                                    "Header: value\r\n"
                                    "\r\n"
                                    "BODY\r\n"
                                    "--BOUNDARY-- \t",
                                    &parts));
  ASSERT_EQ(1u, parts.size());
  EXPECT_EQ(HTTP_SUCCESS, parts[0].code);
  EXPECT_EQ("BODY", parts[0].body);
}
}  // namespace google_apis
