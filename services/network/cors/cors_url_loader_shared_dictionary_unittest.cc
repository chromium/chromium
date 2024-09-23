// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "net/cookies/site_for_cookies.h"
#include "services/network/cors/cors_url_loader.h"
#include "services/network/cors/cors_url_loader_test_util.h"
#include "services/network/network_context.h"
#include "services/network/public/cpp/features.h"
#include "services/network/shared_dictionary/shared_dictionary_constants.h"
#include "services/network/shared_dictionary/shared_dictionary_in_memory.h"
#include "services/network/shared_dictionary/shared_dictionary_manager.h"
#include "services/network/shared_dictionary/shared_dictionary_storage.h"
#include "services/network/shared_dictionary/shared_dictionary_storage_in_memory.h"
#include "services/network/test/client_security_state_builder.h"
#include "services/network/test/test_url_loader_client.h"
#include "url/scheme_host_port.h"

namespace network::cors {
namespace {
const std::string kTestData = "hello world";
const std::string kTestOriginString = "https://origin.test/";
const std::string kTestInsecureOriginString = "http://origin.test/";

}  // namespace

class CorsURLLoaderSharedDictionaryTest : public CorsURLLoaderTestBase {
 public:
  CorsURLLoaderSharedDictionaryTest()
      : CorsURLLoaderTestBase(/*shared_dictionary_enabled*/ true) {
    const url::Origin kOrigin = url::Origin::Create(GURL(kTestOriginString));
    isolation_info_ = net::IsolationInfo::Create(
        net::IsolationInfo::RequestType::kOther, kOrigin, kOrigin,
        net::SiteForCookies::FromOrigin(kOrigin));
  }
  ~CorsURLLoaderSharedDictionaryTest() override = default;

  CorsURLLoaderSharedDictionaryTest(const CorsURLLoaderSharedDictionaryTest&) =
      delete;
  CorsURLLoaderSharedDictionaryTest& operator=(
      const CorsURLLoaderSharedDictionaryTest&) = delete;

 protected:
  void ResetFactory(bool is_web_secure_context = true) {
    ResetFactoryParams factory_params;
    factory_params.isolation_info = isolation_info_;
    factory_params.client_security_state =
        ClientSecurityStateBuilder()
            .WithIsSecureContext(is_web_secure_context)
            .Build();
    CorsURLLoaderTestBase::ResetFactory(isolation_info_.frame_origin(),
                                        kRendererProcessId, factory_params);
  }
  void ResetTrustedFactory() {
    ResetFactoryParams factory_params;
    factory_params.is_trusted = true;
    CorsURLLoaderTestBase::ResetFactory(isolation_info_.frame_origin(),
                                        kRendererProcessId, factory_params);
  }

  void ResetInsecureIsolationInfo() {
    const url::Origin kInsecureOrigin =
        url::Origin::Create(GURL(kTestInsecureOriginString));
    isolation_info_ = net::IsolationInfo::Create(
        net::IsolationInfo::RequestType::kOther, kInsecureOrigin,
        kInsecureOrigin, net::SiteForCookies::FromOrigin(kInsecureOrigin));
  }

  ResourceRequest CreateResourceRequest(
      bool shared_dictionary_writer_enabled = true) {
    ResourceRequest request;
    request.method = "GET";
    request.mode = mojom::RequestMode::kCors;
    request.url = isolation_info_.frame_origin()->GetURL().Resolve("/test");
    request.request_initiator = isolation_info_.frame_origin();
    request.shared_dictionary_writer_enabled = shared_dictionary_writer_enabled;
    return request;
  }

  void CreateDataPipe() {
    MojoCreateDataPipeOptions options;
    options.struct_size = sizeof(MojoCreateDataPipeOptions);
    options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
    options.element_num_bytes = 1;
    options.capacity_num_bytes =
        network::features::GetDataPipeDefaultAllocationSize(
            features::DataPipeAllocationSize::kLargerSizeIfPossible);
    ASSERT_EQ(MOJO_RESULT_OK, mojo::CreateDataPipe(&options, producer_handle_,
                                                   consumer_handle_));
  }
  void CreateDataPipeAndWriteTestData() {
    CreateDataPipe();
    ASSERT_TRUE(mojo::BlockingCopyFromString(kTestData, producer_handle_));
  }

  void NotifyLoaderClientOnReceiveResponseWithUseAsDictionaryHeader(
      std::vector<std::pair<std::string, std::string>> extra_headers = {}) {
    extra_headers.emplace_back(
        network::shared_dictionary::kUseAsDictionaryHeaderName,
        "match=\"/path*\"");
    extra_headers.emplace_back("cache-control", "max-age=2592000");
    NotifyLoaderClientOnReceiveResponse(extra_headers,
                                        std::move(consumer_handle_));
  }

  void CallOnReceiveResponseAndOnCompleteAndFinishBody(
      const std::vector<std::pair<std::string, std::string>>& extra_headers =
          {}) {
    NotifyLoaderClientOnReceiveResponseWithUseAsDictionaryHeader(extra_headers);
    NotifyLoaderClientOnComplete(net::OK);
    producer_handle_.reset();
  }

  void CheckDictionaryInStorage(
      bool expect_exists,
      const GURL& dictionary_url = GURL("https://origin.test/test")) {
    ASSERT_TRUE(isolation_info_.frame_origin());
    std::optional<net::SharedDictionaryIsolationKey> isolation_key =
        net::SharedDictionaryIsolationKey::MaybeCreate(isolation_info_);
    ASSERT_TRUE(isolation_key);
    scoped_refptr<SharedDictionaryStorage> storage =
        network_context()->GetSharedDictionaryManager()->GetStorage(
            *isolation_key);
    const auto& dictionary_map = GetInMemoryDictionaryMap(storage.get());
    if (!expect_exists) {
      EXPECT_TRUE(dictionary_map.empty());
      return;
    }

    ASSERT_EQ(1u, dictionary_map.size());
    EXPECT_EQ(url::SchemeHostPort(dictionary_url),
              dictionary_map.begin()->first);

    ASSERT_EQ(1u, dictionary_map.begin()->second.size());
    EXPECT_EQ(std::make_tuple("/path*", std::set<mojom::RequestDestination>()),
              dictionary_map.begin()->second.begin()->first);
    const auto& dictionary_info =
        dictionary_map.begin()->second.begin()->second;
    EXPECT_EQ(dictionary_url, dictionary_info.url());
    EXPECT_EQ(base::FeatureList::IsEnabled(
                  network::features::kCompressionDictionaryTransport)
                  ? base::Seconds(2592000)
                  : shared_dictionary::kMaxExpirationForOriginTrial,
              dictionary_info.expiration());
    EXPECT_EQ("/path*", dictionary_info.match());
    EXPECT_EQ(kTestData.size(), dictionary_info.size());
    EXPECT_EQ(net::OK, dictionary_info.dictionary()->ReadAll(
                           base::BindOnce([](int) { NOTREACHED(); })));
    EXPECT_EQ(kTestData,
              std::string(dictionary_info.dictionary()->data()->data(),
                          dictionary_info.size()));
  }

  const std::map<
      url::SchemeHostPort,
      std::map<std::tuple<std::string, std::set<mojom::RequestDestination>>,
               SharedDictionaryStorageInMemory::DictionaryInfo>>&
  GetInMemoryDictionaryMap(SharedDictionaryStorage* storage) {
    return static_cast<SharedDictionaryStorageInMemory*>(storage)
        ->GetDictionaryMap();
  }

  size_t GetStorageCount() {
    return network_context()
        ->GetSharedDictionaryManager()
        ->GetStorageCountForTesting();
  }

  net::IsolationInfo isolation_info_;
  mojo::ScopedDataPipeProducerHandle producer_handle_;
  mojo::ScopedDataPipeConsumerHandle consumer_handle_;
};

TEST_F(CorsURLLoaderSharedDictionaryTest, SameOriginUrlSameOriginModeRequest) {
  ResetFactory();
  ResourceRequest request = CreateResourceRequest();
  request.mode = mojom::RequestMode::kSameOrigin;
  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();

  CreateDataPipeAndWriteTestData();
  CallOnReceiveResponseAndOnCompleteAndFinishBody();

  RunUntilComplete();
  EXPECT_EQ(net::OK, client().completion_status().error_code);

  // The response of SameOrigin request should be stored to the
  // dictionary storage.
  CheckDictionaryInStorage(/*expect_exists=*/true);
}

TEST_F(CorsURLLoaderSharedDictionaryTest, SameOriginUrlNoCorsModeRequest) {
  ResetFactory();

  ResourceRequest request = CreateResourceRequest();
  request.mode = mojom::RequestMode::kNoCors;
  CreateLoaderAndStart(request);

  RunUntilCreateLoaderAndStartCalled();

  CreateDataPipeAndWriteTestData();
  CallOnReceiveResponseAndOnCompleteAndFinishBody();

  RunUntilComplete();
  EXPECT_EQ(net::OK, client().completion_status().error_code);

  // The response of NoCors mode same origin request should be stored to
  // the dictionary storage.
  CheckDictionaryInStorage(/*expect_exists=*/true);
}

TEST_F(CorsURLLoaderSharedDictionaryTest, CrossOriginUrlNoCorsModeRequest) {
  ResetFactory();

  ResourceRequest request = CreateResourceRequest();
  request.url = GURL("https://crossorigin.test/test");
  request.mode = mojom::RequestMode::kNoCors;
  CreateLoaderAndStart(request);

  RunUntilCreateLoaderAndStartCalled();

  CreateDataPipeAndWriteTestData();
  CallOnReceiveResponseAndOnCompleteAndFinishBody();

  RunUntilComplete();
  EXPECT_EQ(net::OK, client().completion_status().error_code);

  // The response of NoCors mode cross origin request should not be
  // stored to the dictionary storage.
  CheckDictionaryInStorage(/*expect_exists=*/false);
}

TEST_F(CorsURLLoaderSharedDictionaryTest, SameOriginUrlCorsModeRequest) {
  ResetFactory();

  ResourceRequest request = CreateResourceRequest();
  EXPECT_EQ(mojom::RequestMode::kCors, request.mode);
  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();

  CreateDataPipeAndWriteTestData();
  CallOnReceiveResponseAndOnCompleteAndFinishBody();

  RunUntilComplete();
  EXPECT_EQ(net::OK, client().completion_status().error_code);

  // The response of Cors request should be stored to the dictionary storage.
  CheckDictionaryInStorage(/*expect_exists=*/true);
}

TEST_F(CorsURLLoaderSharedDictionaryTest,
       SameOriginUrlCorsWithForcedPreflightModeRequest) {
  ResetFactory();

  ResourceRequest request = CreateResourceRequest();
  request.mode = mojom::RequestMode::kCorsWithForcedPreflight;
  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();

  CreateDataPipeAndWriteTestData();
  CallOnReceiveResponseAndOnCompleteAndFinishBody();

  RunUntilComplete();
  EXPECT_EQ(net::OK, client().completion_status().error_code);

  // The response of CorsWithForcedPreflight mode request should be stored to
  // to the dictionary storage.
  CheckDictionaryInStorage(/*expect_exists=*/true);
}

TEST_F(CorsURLLoaderSharedDictionaryTest, SameOriginUrlNavigateModeRequest) {
  ResetTrustedFactory();

  ResourceRequest request = CreateResourceRequest();
  request.mode = mojom::RequestMode::kNavigate;
  request.redirect_mode = mojom::RedirectMode::kManual;
  request.navigation_redirect_chain.push_back(request.url);
  request.trusted_params = network::ResourceRequest::TrustedParams();
  request.trusted_params->isolation_info = isolation_info_;
  request.site_for_cookies = isolation_info_.site_for_cookies();
  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();

  CreateDataPipeAndWriteTestData();
  CallOnReceiveResponseAndOnCompleteAndFinishBody();

  RunUntilComplete();
  EXPECT_EQ(net::OK, client().completion_status().error_code);

  // The response of Navigation mode request should not be stored to the
  // dictionary storage.
  CheckDictionaryInStorage(/*expect_exists=*/false);
}

TEST_F(CorsURLLoaderSharedDictionaryTest,
       CrossOriginUrlCorsModeOmitCredentialRequest) {
  ResetFactory();

  ResourceRequest request = CreateResourceRequest();
  request.url = GURL("https://crossorigin.test/test");
  EXPECT_EQ(mojom::RequestMode::kCors, request.mode);
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();

  CreateDataPipeAndWriteTestData();
  CallOnReceiveResponseAndOnCompleteAndFinishBody(
      {{"Access-Control-Allow-Origin", "https://origin.test"}});

  RunUntilComplete();
  EXPECT_EQ(net::OK, client().completion_status().error_code);

  // The successful response of Cors request should be stored to the dictionary
  // storage.
  CheckDictionaryInStorage(/*expect_exists=*/true, request.url);
}

TEST_F(CorsURLLoaderSharedDictionaryTest,
       CrossOriginUrlCorsModeOmitCredentialRequestAsteriskACAO) {
  ResetFactory();

  ResourceRequest request = CreateResourceRequest();
  request.url = GURL("https://crossorigin.test/test");
  EXPECT_EQ(mojom::RequestMode::kCors, request.mode);
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();

  CreateDataPipeAndWriteTestData();
  CallOnReceiveResponseAndOnCompleteAndFinishBody(
      {{"Access-Control-Allow-Origin", "*"}});

  RunUntilComplete();
  EXPECT_EQ(net::OK, client().completion_status().error_code);

  // The successful response of Cors request should be stored to the dictionary
  // storage.
  CheckDictionaryInStorage(/*expect_exists=*/true, request.url);
}

TEST_F(CorsURLLoaderSharedDictionaryTest,
       CrossOriginUrlCorsModeIncludeCredentialRequest) {
  ResetFactory();

  ResourceRequest request = CreateResourceRequest();
  request.url = GURL("https://crossorigin.test/test");
  EXPECT_EQ(mojom::RequestMode::kCors, request.mode);
  request.credentials_mode = mojom::CredentialsMode::kInclude;
  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();

  CreateDataPipeAndWriteTestData();
  CallOnReceiveResponseAndOnCompleteAndFinishBody({
      {"Access-Control-Allow-Origin", "https://origin.test"},
      {"Access-Control-Allow-Credentials", "true"},
  });

  RunUntilComplete();
  EXPECT_EQ(net::OK, client().completion_status().error_code);

  // The successful response of Cors request should be stored to the dictionary
  // storage.
  CheckDictionaryInStorage(/*expect_exists=*/true, request.url);
}

TEST_F(CorsURLLoaderSharedDictionaryTest,
       OnCompleteAfterClosingBodyDataHandle) {
  ResetFactory();

  CreateLoaderAndStart(CreateResourceRequest());
  RunUntilCreateLoaderAndStartCalled();

  CreateDataPipeAndWriteTestData();

  NotifyLoaderClientOnReceiveResponseWithUseAsDictionaryHeader();

  producer_handle_.reset();

  base::RunLoop().RunUntilIdle();

  // Call OnComplete after closing the body data handle.
  NotifyLoaderClientOnComplete(net::OK);

  RunUntilComplete();

  EXPECT_EQ(net::OK, client().completion_status().error_code);
  CheckDictionaryInStorage(/*expect_exists=*/true);
}

TEST_F(CorsURLLoaderSharedDictionaryTest,
       ResetClientRemoteFromNetworkWithoutOnCompleteCalled) {
  ResetFactory();

  CreateLoaderAndStart(CreateResourceRequest());
  RunUntilCreateLoaderAndStartCalled();

  CreateDataPipeAndWriteTestData();

  NotifyLoaderClientOnReceiveResponseWithUseAsDictionaryHeader();

  base::RunLoop().RunUntilIdle();

  // Simulate an abort from the network side.
  ResetClientRemote();

  RunUntilComplete();

  EXPECT_EQ(net::ERR_ABORTED, client().completion_status().error_code);

  // When the requesta was aborted from the network side, the response should
  // not be stored to the dictionary storage.
  CheckDictionaryInStorage(/*expect_exists=*/false);
}

TEST_F(CorsURLLoaderSharedDictionaryTest,
       ResetClientRemoteFromNetworkAfterOnCompleteCalled) {
  ResetFactory();

  CreateLoaderAndStart(CreateResourceRequest());
  RunUntilCreateLoaderAndStartCalled();

  CreateDataPipeAndWriteTestData();

  NotifyLoaderClientOnReceiveResponseWithUseAsDictionaryHeader();
  NotifyLoaderClientOnComplete(net::OK);

  base::RunLoop().RunUntilIdle();

  // Simulate an abort from the network side.
  ResetClientRemote();

  producer_handle_.reset();

  RunUntilComplete();

  EXPECT_EQ(net::OK, client().completion_status().error_code);

  // When the request was aborted from the network side after OnComplete() is
  // called, the response should be stored to the dictionary storage.
  CheckDictionaryInStorage(/*expect_exists=*/true);
}

TEST_F(CorsURLLoaderSharedDictionaryTest, InsecureContext) {
  ResetFactory(/*is_web_secure_context=*/false);

  ResourceRequest request = CreateResourceRequest();
  EXPECT_EQ(mojom::RequestMode::kCors, request.mode);
  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();

  CreateDataPipeAndWriteTestData();
  CallOnReceiveResponseAndOnCompleteAndFinishBody();

  RunUntilComplete();
  EXPECT_EQ(net::OK, client().completion_status().error_code);

  // The response of should be stored to the dictionary storage because the web
  // context is not secure.
  CheckDictionaryInStorage(/*expect_exists=*/false);
}

TEST_F(CorsURLLoaderSharedDictionaryTest, SharedDictionaryWriterDisabled) {
  ResetFactory();

  ResourceRequest request =
      CreateResourceRequest(/*shared_dictionary_writer_enabled=*/false);
  EXPECT_EQ(mojom::RequestMode::kCors, request.mode);
  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();

  CreateDataPipeAndWriteTestData();
  CallOnReceiveResponseAndOnCompleteAndFinishBody();

  RunUntilComplete();
  EXPECT_EQ(net::OK, client().completion_status().error_code);

  // The response of should be stored to the dictionary storage because shared
  // dictionary writer is disabled.
  CheckDictionaryInStorage(/*expect_exists=*/false);
}

TEST_F(CorsURLLoaderSharedDictionaryTest, StorageCountForSecureContext) {
  ResetFactory(/*is_web_secure_context=*/true);
  // SharedDictionaryStorage should have been created for secure context
  // factory.
  EXPECT_EQ(1u, GetStorageCount());
}

TEST_F(CorsURLLoaderSharedDictionaryTest, StorageCountForUnsecureContext) {
  ResetFactory(/*is_web_secure_context=*/false);
  // SharedDictionaryStorage should not have been created for non-secure
  // context factory.
  EXPECT_EQ(0u, GetStorageCount());
}

TEST_F(CorsURLLoaderSharedDictionaryTest, StorageCountForTrustedFactory) {
  ResetTrustedFactory();
  // SharedDictionaryStorage should not have been created for trusted factory.
  EXPECT_EQ(0u, GetStorageCount());
}

TEST_F(CorsURLLoaderSharedDictionaryTest,
       StorageCountTopFrameNavigationSecureRequest) {
  ResetTrustedFactory();

  ResourceRequest request = CreateResourceRequest();
  request.mode = mojom::RequestMode::kNavigate;
  request.redirect_mode = mojom::RedirectMode::kManual;
  request.navigation_redirect_chain.push_back(request.url);
  request.trusted_params = network::ResourceRequest::TrustedParams();
  request.trusted_params->isolation_info = isolation_info_;
  request.site_for_cookies = isolation_info_.site_for_cookies();
  CreateLoaderAndStart(request);

  // Starting a secure navigation request should create a
  // SharedDictionaryStorage.
  EXPECT_EQ(1u, GetStorageCount());
}

TEST_F(CorsURLLoaderSharedDictionaryTest,
       StorageCountTopFrameNavigationInsecureRequest) {
  ResetInsecureIsolationInfo();
  ResetTrustedFactory();

  ResourceRequest request = CreateResourceRequest();
  request.mode = mojom::RequestMode::kNavigate;
  request.redirect_mode = mojom::RedirectMode::kManual;
  request.navigation_redirect_chain.push_back(request.url);
  request.trusted_params = network::ResourceRequest::TrustedParams();
  request.trusted_params->isolation_info = isolation_info_;
  request.site_for_cookies = isolation_info_.site_for_cookies();
  CreateLoaderAndStart(request);

  // Starting an insecure navigation request should not create a
  // SharedDictionaryStorage.
  EXPECT_EQ(0u, GetStorageCount());
}

TEST_F(CorsURLLoaderSharedDictionaryTest,
       StorageCountSubFrameNavigationRequest) {
  ResetTrustedFactory();

  ResourceRequest request = CreateResourceRequest();
  request.mode = mojom::RequestMode::kNavigate;
  request.redirect_mode = mojom::RedirectMode::kManual;
  request.navigation_redirect_chain.push_back(request.url);
  request.trusted_params = network::ResourceRequest::TrustedParams();
  request.trusted_params->isolation_info = isolation_info_;
  request.trusted_params->client_security_state =
      ClientSecurityStateBuilder().WithIsSecureContext(true).Build();
  request.site_for_cookies = isolation_info_.site_for_cookies();
  CreateLoaderAndStart(request);

  // Starting a navigation request for secure context should create a
  // SharedDictionaryStorage.
  EXPECT_EQ(1u, GetStorageCount());
}

TEST_F(CorsURLLoaderSharedDictionaryTest,
       StorageCountSubFrameNavigationRequestInsecureContext) {
  ResetTrustedFactory();

  ResourceRequest request = CreateResourceRequest();
  request.mode = mojom::RequestMode::kNavigate;
  request.redirect_mode = mojom::RedirectMode::kManual;
  request.navigation_redirect_chain.push_back(request.url);
  request.trusted_params = network::ResourceRequest::TrustedParams();
  request.trusted_params->isolation_info = isolation_info_;
  request.trusted_params->client_security_state =
      ClientSecurityStateBuilder().WithIsSecureContext(false).Build();
  request.site_for_cookies = isolation_info_.site_for_cookies();
  CreateLoaderAndStart(request);

  // Starting a navigation request for insecure context should not create a
  // SharedDictionaryStorage.
  EXPECT_EQ(0u, GetStorageCount());
}

}  // namespace network::cors
