// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cors/cors_url_loader.h"

#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "net/cookies/site_for_cookies.h"
#include "services/network/cors/cors_url_loader_test_util.h"
#include "services/network/network_context.h"
#include "services/network/public/cpp/features.h"
#include "services/network/shared_dictionary/shared_dictionary_constants.h"
#include "services/network/shared_dictionary/shared_dictionary_manager.h"
#include "services/network/shared_dictionary/shared_dictionary_storage.h"
#include "services/network/shared_dictionary/shared_dictionary_storage_in_memory.h"
#include "services/network/test/test_url_loader_client.h"

namespace network::cors {
namespace {
const std::string kTestData = "hello world";
const std::string kTestOriginString = "https://origin.test/";

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
  void ResetFactory() {
    ResetFactoryParams factory_params;
    factory_params.isolation_info = isolation_info_;
    CorsURLLoaderTestBase::ResetFactory(isolation_info_.frame_origin(),
                                        kRendererProcessId, factory_params);
  }

  ResourceRequest CreateResourceRequest() {
    ResourceRequest request;
    request.method = "GET";
    request.mode = mojom::RequestMode::kCors;
    request.url = GURL("https://origin.test/test");
    request.request_initiator = isolation_info_.frame_origin();
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
    absl::optional<SharedDictionaryStorageIsolationKey> isolation_key =
        SharedDictionaryStorageIsolationKey::MaybeCreate(isolation_info_);
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
    EXPECT_EQ(url::Origin::Create(dictionary_url),
              dictionary_map.begin()->first);

    ASSERT_EQ(1u, dictionary_map.begin()->second.size());
    EXPECT_EQ("/path*", dictionary_map.begin()->second.begin()->first);
    const auto& dictionary_info =
        dictionary_map.begin()->second.begin()->second;
    EXPECT_EQ(dictionary_url, dictionary_info.url());
    EXPECT_EQ(shared_dictionary::kDefaultExpiration,
              dictionary_info.expiration());
    EXPECT_EQ("/path*", dictionary_info.path_pattern());
    EXPECT_EQ(kTestData.size(), dictionary_info.size());
    EXPECT_EQ(kTestData, std::string(dictionary_info.data()->data(),
                                     dictionary_info.size()));
  }

  const std::map<
      url::Origin,
      std::map<std::string, SharedDictionaryStorageInMemory::DictionaryInfo>>&
  GetInMemoryDictionaryMap(SharedDictionaryStorage* storage) {
    return static_cast<SharedDictionaryStorageInMemory*>(storage)
        ->GetDictionaryMapForTesting();
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

  // The response of SameOrigin mode request should not be stored to the
  // dictionary storage.
  CheckDictionaryInStorage(/*expect_exists=*/false);
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

  // The response of NoCors mode request should not be stored to the dictionary
  // storage.
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
  ResetFactory();

  ResourceRequest request = CreateResourceRequest();
  request.mode = mojom::RequestMode::kNavigate;
  request.redirect_mode = mojom::RedirectMode::kManual;
  request.navigation_redirect_chain.push_back(request.url);
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

}  // namespace network::cors
