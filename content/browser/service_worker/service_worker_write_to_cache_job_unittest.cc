// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_request_handler.h"
#include "content/browser/service_worker/service_worker_disk_cache.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/test/mock_resource_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/test/url_request/url_request_failed_job.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "net/url_request/url_request_test_job.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/mojom/request_context_frame_type.mojom.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/service_worker/service_worker_utils.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"

// Note for S13nServiceWorker: All tests are skipped as we don't use
// ServiceWorkerWriteToCacheJob when S13nServiceWorker is enabled.

namespace content {
namespace service_worker_write_to_cache_job_unittest {

const char kHeaders[] =
    "HTTP/1.1 200 OK\n"
    "Content-Type: text/javascript\n"
    "Expires: Thu, 1 Jan 2100 20:00:00 GMT\n"
    "\n";
const char kScriptCode[] = "// no script code\n";

// The blocksize that ServiceWorkerWriteToCacheJob reads/writes at a time.
const int kBlockSize = 16 * 1024;
const int kNumBlocks = 8;
const int kMiddleBlock = 5;

std::string GenerateLongResponse() {
  return std::string(kNumBlocks * kBlockSize, 'a');
}

net::URLRequestJob* CreateNormalURLRequestJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) {
  return new net::URLRequestTestJob(request,
                                    network_delegate,
                                    std::string(kHeaders, arraysize(kHeaders)),
                                    kScriptCode,
                                    true);
}

net::URLRequestJob* CreateResponseJob(const std::string& response_data,
                                      net::URLRequest* request,
                                      net::NetworkDelegate* network_delegate) {
  return new net::URLRequestTestJob(request, network_delegate,
                                    std::string(kHeaders, arraysize(kHeaders)),
                                    response_data, true);
}

net::URLRequestJob* CreateFailedURLRequestJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) {
  return new net::URLRequestFailedJob(request, network_delegate,
                                      net::URLRequestFailedJob::START,
                                      net::ERR_FAILED);
}

net::URLRequestJob* CreateInvalidMimeTypeJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) {
  const char kPlainTextHeaders[] =
      "HTTP/1.1 200 OK\n"
      "Content-Type: text/plain\n"
      "Expires: Thu, 1 Jan 2100 20:00:00 GMT\n"
      "\n";
  return new net::URLRequestTestJob(
      request,
      network_delegate,
      std::string(kPlainTextHeaders, arraysize(kPlainTextHeaders)),
      kScriptCode,
      true);
}

class SSLCertificateErrorJob : public net::URLRequestTestJob {
 public:
  SSLCertificateErrorJob(net::URLRequest* request,
                         net::NetworkDelegate* network_delegate,
                         const std::string& response_headers,
                         const std::string& response_data,
                         bool auto_advance)
      : net::URLRequestTestJob(request,
                               network_delegate,
                               response_headers,
                               response_data,
                               auto_advance),
        weak_factory_(this) {}
  void Start() override {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&SSLCertificateErrorJob::NotifyError,
                                  weak_factory_.GetWeakPtr()));
  }
  void NotifyError() {
    net::SSLInfo info;
    info.cert_status = net::CERT_STATUS_DATE_INVALID;
    NotifySSLCertificateError(info, true);
  }
  void ContinueDespiteLastError() override {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&SSLCertificateErrorJob::StartAsync,
                                  weak_factory_.GetWeakPtr()));
  }

 protected:
  ~SSLCertificateErrorJob() override {}
  base::WeakPtrFactory<SSLCertificateErrorJob> weak_factory_;
};

net::URLRequestJob* CreateSSLCertificateErrorJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) {
  return new SSLCertificateErrorJob(request,
                                    network_delegate,
                                    std::string(kHeaders, arraysize(kHeaders)),
                                    kScriptCode,
                                    true);
}

class CertStatusErrorJob : public net::URLRequestTestJob {
 public:
  CertStatusErrorJob(net::URLRequest* request,
                     net::NetworkDelegate* network_delegate,
                     const std::string& response_headers,
                     const std::string& response_data,
                     bool auto_advance)
      : net::URLRequestTestJob(request,
                               network_delegate,
                               response_headers,
                               response_data,
                               auto_advance) {}
  void GetResponseInfo(net::HttpResponseInfo* info) override {
    URLRequestTestJob::GetResponseInfo(info);
    info->ssl_info.cert_status = net::CERT_STATUS_DATE_INVALID;
  }

 protected:
  ~CertStatusErrorJob() override {}
};

net::URLRequestJob* CreateCertStatusErrorJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) {
  return new CertStatusErrorJob(request,
                                network_delegate,
                                std::string(kHeaders, arraysize(kHeaders)),
                                kScriptCode,
                                true);
}

class MockHttpProtocolHandler
    : public net::URLRequestJobFactory::ProtocolHandler {
 public:
  using JobCallback =
      base::RepeatingCallback<net::URLRequestJob*(net::URLRequest*,
                                                  net::NetworkDelegate*)>;

  explicit MockHttpProtocolHandler(ResourceContext* resource_context)
      : resource_context_(resource_context) {}
  ~MockHttpProtocolHandler() override {}

  net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    ServiceWorkerRequestHandler* handler =
        ServiceWorkerRequestHandler::GetHandler(request);
    if (handler) {
      return handler->MaybeCreateJob(
          request, network_delegate, resource_context_);
    }
    return create_job_callback_.Run(request, network_delegate);
  }
  void SetCreateJobCallback(JobCallback callback) {
    create_job_callback_ = std::move(callback);
  }

 private:
  ResourceContext* resource_context_;
  JobCallback create_job_callback_;
};

class ResponseVerifier : public base::RefCounted<ResponseVerifier> {
 public:
  ResponseVerifier(std::unique_ptr<ServiceWorkerResponseReader> reader,
                   const std::string& expected,
                   base::OnceCallback<void(bool)> callback)
      : reader_(reader.release()),
        expected_(expected),
        callback_(std::move(callback)) {}

  void Start() {
    info_buffer_ = new HttpResponseInfoIOBuffer();
    io_buffer_ = base::MakeRefCounted<net::IOBuffer>(kBlockSize);
    reader_->ReadInfo(
        info_buffer_.get(),
        base::BindOnce(&ResponseVerifier::OnReadInfoComplete, this));
    bytes_read_ = 0;
  }

  void OnReadInfoComplete(int result) {
    if (result < 0) {
      std::move(callback_).Run(false);
      return;
    }
    if (info_buffer_->response_data_size !=
        static_cast<int>(expected_.size())) {
      std::move(callback_).Run(false);
      return;
    }
    ReadSomeData();
  }

  void ReadSomeData() {
    reader_->ReadData(
        io_buffer_.get(), kBlockSize,
        base::BindOnce(&ResponseVerifier::OnReadDataComplete, this));
  }

  void OnReadDataComplete(int result) {
    if (result < 0) {
      std::move(callback_).Run(false);
      return;
    }
    if (result == 0) {
      std::move(callback_).Run(true);
      return;
    }
    std::string str(io_buffer_->data(), result);
    std::string expect = expected_.substr(bytes_read_, result);
    if (str != expect) {
      std::move(callback_).Run(false);
      return;
    }
    bytes_read_ += result;
    ReadSomeData();
  }

 private:
  friend class base::RefCounted<ResponseVerifier>;
  ~ResponseVerifier() {}

  std::unique_ptr<ServiceWorkerResponseReader> reader_;
  const std::string expected_;
  base::OnceCallback<void(bool)> callback_;
  scoped_refptr<HttpResponseInfoIOBuffer> info_buffer_;
  scoped_refptr<net::IOBuffer> io_buffer_;
  size_t bytes_read_;
};

class ServiceWorkerWriteToCacheJobTest : public testing::Test {
 public:
  ServiceWorkerWriteToCacheJobTest()
      : ServiceWorkerWriteToCacheJobTest("https://host/scope/",
                                         "https://host/script.js") {}
  ServiceWorkerWriteToCacheJobTest(const std::string& scope,
                                   const std::string& script_url)
      : scope_(scope),
        script_url_(script_url),
        browser_thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP),
        mock_protocol_handler_(nullptr) {}
  ~ServiceWorkerWriteToCacheJobTest() override {}

  base::WeakPtr<ServiceWorkerProviderHost> CreateHostForVersion(
      int process_id,
      const scoped_refptr<ServiceWorkerVersion>& version) {
    return CreateProviderHostForServiceWorkerContext(
        process_id, true /* is_parent_frame_secure */, version.get(),
        context()->AsWeakPtr(), &remote_endpoint_);
  }

  void SetUpScriptRequest(int process_id, int provider_id) {
    request_.reset();
    url_request_context_.reset();
    url_request_job_factory_.reset();
    mock_protocol_handler_ = nullptr;
    // URLRequestJobs may post clean-up tasks on destruction.
    base::RunLoop().RunUntilIdle();

    url_request_context_.reset(new net::URLRequestContext);
    mock_protocol_handler_ = new MockHttpProtocolHandler(&resource_context_);
    url_request_job_factory_.reset(new net::URLRequestJobFactoryImpl);
    url_request_job_factory_->SetProtocolHandler(
        "https", base::WrapUnique(mock_protocol_handler_));
    url_request_context_->set_job_factory(url_request_job_factory_.get());

    request_ = url_request_context_->CreateRequest(
        script_url_, net::DEFAULT_PRIORITY, &url_request_delegate_,
        TRAFFIC_ANNOTATION_FOR_TESTS);
    ServiceWorkerRequestHandler::InitializeHandler(
        request_.get(), context_wrapper(), &blob_storage_context_, process_id,
        provider_id, false, network::mojom::FetchRequestMode::kNoCORS,
        network::mojom::FetchCredentialsMode::kOmit,
        network::mojom::FetchRedirectMode::kFollow,
        std::string() /* integrity */, false /* keepalive */,
        RESOURCE_TYPE_SERVICE_WORKER,
        blink::mojom::RequestContextType::SERVICE_WORKER,
        network::mojom::RequestContextFrameType::kNone,
        scoped_refptr<network::ResourceRequestBody>());
  }

  int NextVersionId() { return next_version_id_++; }

  void SetUp() override {
    helper_.reset(new EmbeddedWorkerTestHelper(base::FilePath()));

    // A new unstored registration/version.
    blink::mojom::ServiceWorkerRegistrationOptions options;
    options.scope = scope_;
    registration_ =
        new ServiceWorkerRegistration(options, 1L, context()->AsWeakPtr());
    version_ = new ServiceWorkerVersion(
        registration_.get(), script_url_, blink::mojom::ScriptType::kClassic,
        NextVersionId(), context()->AsWeakPtr());
    base::WeakPtr<ServiceWorkerProviderHost> host =
        CreateHostForVersion(helper_->mock_render_process_id(), version_);
    ASSERT_TRUE(host);
    SetUpScriptRequest(helper_->mock_render_process_id(), host->provider_id());

    context()->storage()->LazyInitializeForTest(base::DoNothing());
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    request_.reset();
    url_request_context_.reset();
    url_request_job_factory_.reset();
    mock_protocol_handler_ = nullptr;
    version_ = nullptr;
    registration_ = nullptr;
    helper_.reset();
    // URLRequestJobs may post clean-up tasks on destruction.
    base::RunLoop().RunUntilIdle();
  }

  int CreateIncumbent(const std::string& response) {
    mock_protocol_handler_->SetCreateJobCallback(
        base::Bind(&CreateResponseJob, response));
    request_->Start();
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(net::URLRequestStatus::SUCCESS, request_->status().status());
    int64_t incumbent_resource_id =
        version_->script_cache_map()->LookupResourceId(script_url_);
    EXPECT_NE(kInvalidServiceWorkerResourceId, incumbent_resource_id);

    registration_->SetActiveVersion(version_);

    // Teardown the request.
    request_.reset();
    url_request_context_.reset();
    url_request_job_factory_.reset();
    mock_protocol_handler_ = nullptr;
    base::RunLoop().RunUntilIdle();

    return incumbent_resource_id;
  }

  int64_t GetResourceId(ServiceWorkerVersion* version) {
    return version->script_cache_map()->LookupResourceId(script_url_);
  }

  // Performs the net request for an update of |registration_|'s incumbent
  // to the script |response|. Returns the new version.
  scoped_refptr<ServiceWorkerVersion> UpdateScript(
      const std::string& response) {
    scoped_refptr<ServiceWorkerVersion> new_version = new ServiceWorkerVersion(
        registration_.get(), script_url_, blink::mojom::ScriptType::kClassic,
        NextVersionId(), context()->AsWeakPtr());
    new_version->SetToPauseAfterDownload(base::DoNothing());
    base::WeakPtr<ServiceWorkerProviderHost> host =
        CreateHostForVersion(helper_->mock_render_process_id(), new_version);
    EXPECT_TRUE(host);
    SetUpScriptRequest(helper_->mock_render_process_id(), host->provider_id());
    mock_protocol_handler_->SetCreateJobCallback(
        base::Bind(&CreateResponseJob, response));
    request_->Start();
    base::RunLoop().RunUntilIdle();
    return new_version;
  }

  void VerifyResource(int64_t id, const std::string& expected) {
    ASSERT_NE(kInvalidServiceWorkerResourceId, id);
    base::Optional<bool> is_equal;
    std::unique_ptr<ServiceWorkerResponseReader> reader =
        context()->storage()->CreateResponseReader(id);
    scoped_refptr<ResponseVerifier> verifier = new ResponseVerifier(
        std::move(reader), expected, CreateReceiverOnCurrentThread(&is_equal));
    verifier->Start();
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(is_equal.value());
  }

  ServiceWorkerContextCore* context() const { return helper_->context(); }
  ServiceWorkerContextWrapper* context_wrapper() const {
    return helper_->context_wrapper();
  }

  // Disables the cache to simulate cache errors.
  void DisableCache() { context()->storage()->disk_cache()->Disable(); }

 protected:
  const GURL scope_;
  const GURL script_url_;

  TestBrowserThreadBundle browser_thread_bundle_;
  std::unique_ptr<EmbeddedWorkerTestHelper> helper_;
  scoped_refptr<ServiceWorkerRegistration> registration_;
  scoped_refptr<ServiceWorkerVersion> version_;
  std::unique_ptr<net::URLRequestContext> url_request_context_;
  std::unique_ptr<net::URLRequestJobFactoryImpl> url_request_job_factory_;
  std::unique_ptr<net::URLRequest> request_;
  MockHttpProtocolHandler* mock_protocol_handler_;

  storage::BlobStorageContext blob_storage_context_;
  content::MockResourceContext resource_context_;
  ServiceWorkerRemoteProviderEndpoint remote_endpoint_;

  net::TestDelegate url_request_delegate_;
  int next_provider_id_ = 1;
  int64_t next_version_id_ = 1L;
};

TEST_F(ServiceWorkerWriteToCacheJobTest, Normal) {
  if (blink::ServiceWorkerUtils::IsServicificationEnabled())
    return;

  mock_protocol_handler_->SetCreateJobCallback(
      base::Bind(&CreateNormalURLRequestJob));
  request_->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(net::URLRequestStatus::SUCCESS, request_->status().status());
  EXPECT_NE(kInvalidServiceWorkerResourceId,
            version_->script_cache_map()->LookupResourceId(script_url_));
}

TEST_F(ServiceWorkerWriteToCacheJobTest, InvalidMimeType) {
  if (blink::ServiceWorkerUtils::IsServicificationEnabled())
    return;

  mock_protocol_handler_->SetCreateJobCallback(
      base::Bind(&CreateInvalidMimeTypeJob));
  request_->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(net::URLRequestStatus::FAILED, request_->status().status());
  EXPECT_EQ(net::ERR_INSECURE_RESPONSE, request_->status().error());
  EXPECT_EQ(kInvalidServiceWorkerResourceId,
            version_->script_cache_map()->LookupResourceId(script_url_));
}

TEST_F(ServiceWorkerWriteToCacheJobTest, SSLCertificateError) {
  if (blink::ServiceWorkerUtils::IsServicificationEnabled())
    return;

  mock_protocol_handler_->SetCreateJobCallback(
      base::Bind(&CreateSSLCertificateErrorJob));
  request_->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(net::URLRequestStatus::FAILED, request_->status().status());
  EXPECT_EQ(net::ERR_CERT_DATE_INVALID, request_->status().error());
  EXPECT_EQ(kInvalidServiceWorkerResourceId,
            version_->script_cache_map()->LookupResourceId(script_url_));
}

class ServiceWorkerWriteToCacheLocalhostTest
    : public ServiceWorkerWriteToCacheJobTest {
 public:
  ServiceWorkerWriteToCacheLocalhostTest()
      : ServiceWorkerWriteToCacheJobTest("https://localhost/scope/",
                                         "https://localhost/script.js") {}
  ~ServiceWorkerWriteToCacheLocalhostTest() override {}
};

TEST_F(ServiceWorkerWriteToCacheLocalhostTest,
       SSLCertificateError_AllowInsecureLocalhost) {
  if (blink::ServiceWorkerUtils::IsServicificationEnabled())
    return;

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kAllowInsecureLocalhost);

  mock_protocol_handler_->SetCreateJobCallback(
      base::Bind(&CreateSSLCertificateErrorJob));
  request_->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(net::URLRequestStatus::SUCCESS, request_->status().status());
  EXPECT_EQ(net::OK, request_->status().error());
  EXPECT_NE(kInvalidServiceWorkerResourceId,
            version_->script_cache_map()->LookupResourceId(script_url_));
}

TEST_F(ServiceWorkerWriteToCacheLocalhostTest, SSLCertificateError) {
  if (blink::ServiceWorkerUtils::IsServicificationEnabled())
    return;

  mock_protocol_handler_->SetCreateJobCallback(
      base::Bind(&CreateSSLCertificateErrorJob));
  request_->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(net::URLRequestStatus::FAILED, request_->status().status());
  EXPECT_EQ(net::ERR_CERT_DATE_INVALID, request_->status().error());
  EXPECT_EQ(kInvalidServiceWorkerResourceId,
            version_->script_cache_map()->LookupResourceId(script_url_));
}

TEST_F(ServiceWorkerWriteToCacheLocalhostTest,
       CertStatusError_AllowInsecureLocalhost) {
  if (blink::ServiceWorkerUtils::IsServicificationEnabled())
    return;

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kAllowInsecureLocalhost);

  mock_protocol_handler_->SetCreateJobCallback(
      base::Bind(&CreateCertStatusErrorJob));
  request_->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(net::URLRequestStatus::SUCCESS, request_->status().status());
  EXPECT_EQ(net::OK, request_->status().error());
  EXPECT_NE(kInvalidServiceWorkerResourceId,
            version_->script_cache_map()->LookupResourceId(script_url_));
}

TEST_F(ServiceWorkerWriteToCacheLocalhostTest, CertStatusError) {
  if (blink::ServiceWorkerUtils::IsServicificationEnabled())
    return;

  mock_protocol_handler_->SetCreateJobCallback(
      base::Bind(&CreateCertStatusErrorJob));
  request_->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(net::URLRequestStatus::FAILED, request_->status().status());
  EXPECT_EQ(net::ERR_CERT_DATE_INVALID, request_->status().error());
  EXPECT_EQ(kInvalidServiceWorkerResourceId,
            version_->script_cache_map()->LookupResourceId(script_url_));
}

TEST_F(ServiceWorkerWriteToCacheJobTest, CertStatusError) {
  if (blink::ServiceWorkerUtils::IsServicificationEnabled())
    return;

  mock_protocol_handler_->SetCreateJobCallback(
      base::Bind(&CreateCertStatusErrorJob));
  request_->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(net::URLRequestStatus::FAILED, request_->status().status());
  EXPECT_EQ(net::ERR_CERT_DATE_INVALID, request_->status().error());
  EXPECT_EQ(kInvalidServiceWorkerResourceId,
            version_->script_cache_map()->LookupResourceId(script_url_));
}

TEST_F(ServiceWorkerWriteToCacheJobTest, Update_SameScript) {
  if (blink::ServiceWorkerUtils::IsServicificationEnabled())
    return;

  std::string response = GenerateLongResponse();
  CreateIncumbent(response);
  scoped_refptr<ServiceWorkerVersion> version = UpdateScript(response);
  EXPECT_EQ(kInvalidServiceWorkerResourceId, GetResourceId(version.get()));
}

TEST_F(ServiceWorkerWriteToCacheJobTest, Update_SameSizeScript) {
  if (blink::ServiceWorkerUtils::IsServicificationEnabled())
    return;

  std::string response = GenerateLongResponse();
  CreateIncumbent(response);

  // Change the first byte.
  response[0] = 'x';
  scoped_refptr<ServiceWorkerVersion> version = UpdateScript(response);
  VerifyResource(GetResourceId(version.get()), response);
  registration_->SetWaitingVersion(version);

  // Change something within the first block.
  response[5555] = 'x';
  version = UpdateScript(response);
  VerifyResource(GetResourceId(version.get()), response);
  registration_->SetWaitingVersion(version);

  // Change something in a middle block.
  response[kMiddleBlock * kBlockSize + 111] = 'x';
  version = UpdateScript(response);
  VerifyResource(GetResourceId(version.get()), response);
  registration_->SetWaitingVersion(version);

  // Change something within the last block.
  response[(kNumBlocks - 1) * kBlockSize] = 'x';
  version = UpdateScript(response);
  VerifyResource(GetResourceId(version.get()), response);
  registration_->SetWaitingVersion(version);

  // Change the last byte.
  response[(kNumBlocks * kBlockSize) - 1] = 'x';
  version = UpdateScript(response);
  VerifyResource(GetResourceId(version.get()), response);
  registration_->SetWaitingVersion(version);
}

TEST_F(ServiceWorkerWriteToCacheJobTest, Update_TruncatedScript) {
  if (blink::ServiceWorkerUtils::IsServicificationEnabled())
    return;

  std::string response = GenerateLongResponse();
  CreateIncumbent(response);

  // Truncate a single byte.
  response.resize(response.size() - 1);
  scoped_refptr<ServiceWorkerVersion> version = UpdateScript(response);
  VerifyResource(GetResourceId(version.get()), response);
  registration_->SetWaitingVersion(version);

  // Truncate to a middle block.
  response.resize((kMiddleBlock + 1) * kBlockSize + 111);
  version = UpdateScript(response);
  VerifyResource(GetResourceId(version.get()), response);
  registration_->SetWaitingVersion(version);

  // Truncate to a block boundary.
  response.resize((kMiddleBlock - 1) * kBlockSize);
  version = UpdateScript(response);
  VerifyResource(GetResourceId(version.get()), response);
  registration_->SetWaitingVersion(version);

  // Truncate to a single byte.
  response.resize(1);
  version = UpdateScript(response);
  VerifyResource(GetResourceId(version.get()), response);
  registration_->SetWaitingVersion(version);
}

TEST_F(ServiceWorkerWriteToCacheJobTest, Update_ElongatedScript) {
  if (blink::ServiceWorkerUtils::IsServicificationEnabled())
    return;

  std::string original_response = GenerateLongResponse();
  CreateIncumbent(original_response);

  // Extend a single byte.
  std::string new_response = original_response + 'a';
  scoped_refptr<ServiceWorkerVersion> version = UpdateScript(new_response);
  VerifyResource(GetResourceId(version.get()), new_response);
  registration_->SetWaitingVersion(version);

  // Extend multiple blocks.
  new_response = original_response + std::string(3 * kBlockSize, 'a');
  version = UpdateScript(new_response);
  VerifyResource(GetResourceId(version.get()), new_response);
  registration_->SetWaitingVersion(version);

  // Extend multiple blocks and bytes.
  new_response = original_response + std::string(7 * kBlockSize + 777, 'a');
  version = UpdateScript(new_response);
  VerifyResource(GetResourceId(version.get()), new_response);
  registration_->SetWaitingVersion(version);
}

TEST_F(ServiceWorkerWriteToCacheJobTest, Update_EmptyScript) {
  if (blink::ServiceWorkerUtils::IsServicificationEnabled())
    return;

  // Create empty incumbent.
  CreateIncumbent(std::string());

  // Update from empty to non-empty.
  std::string response = GenerateLongResponse();
  scoped_refptr<ServiceWorkerVersion> version = UpdateScript(response);
  VerifyResource(GetResourceId(version.get()), response);
  registration_->SetWaitingVersion(version);

  // Update from non-empty to empty.
  version = UpdateScript(std::string());
  VerifyResource(GetResourceId(version.get()), std::string());
  registration_->SetWaitingVersion(version);

  // Update from empty to empty.
  version = UpdateScript(std::string());
  EXPECT_EQ(kInvalidServiceWorkerResourceId, GetResourceId(version.get()));
}

TEST_F(ServiceWorkerWriteToCacheJobTest, Error) {
  if (blink::ServiceWorkerUtils::IsServicificationEnabled())
    return;

  mock_protocol_handler_->SetCreateJobCallback(
      base::Bind(&CreateFailedURLRequestJob));
  request_->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(net::URLRequestStatus::FAILED, request_->status().status());
  EXPECT_EQ(net::ERR_FAILED, request_->status().error());
  EXPECT_EQ(kInvalidServiceWorkerResourceId,
            version_->script_cache_map()->LookupResourceId(script_url_));
}

TEST_F(ServiceWorkerWriteToCacheJobTest, FailedWriteHeadersToCache) {
  if (blink::ServiceWorkerUtils::IsServicificationEnabled())
    return;

  mock_protocol_handler_->SetCreateJobCallback(
      base::Bind(&CreateNormalURLRequestJob));
  DisableCache();
  request_->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(net::URLRequestStatus::FAILED, request_->status().status());
  EXPECT_EQ(net::ERR_FAILED, request_->status().error());
}

}  // namespace service_worker_write_to_cache_job_unittest
}  // namespace content
