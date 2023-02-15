// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/worklet_module_responses_map.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_script_creation_params.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_script_loader.h"
#include "third_party/blink/renderer/core/loader/modulescript/worklet_module_script_fetcher.h"
#include "third_party/blink/renderer/core/script/modulator.h"
#include "third_party/blink/renderer/core/testing/dummy_modulator.h"
#include "third_party/blink/renderer/platform/loader/testing/fetch_testing_platform_support.h"
#include "third_party/blink/renderer/platform/loader/testing/mock_fetch_context.h"
#include "third_party/blink/renderer/platform/loader/testing/test_loader_factory.h"
#include "third_party/blink/renderer/platform/loader/testing/test_resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/testing/mock_context_lifecycle_notifier.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_loader_mock_factory.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

class WorkletModuleResponsesMapTest : public testing::Test {
 public:
  WorkletModuleResponsesMapTest() {
    platform_->AdvanceClockSeconds(1.);  // For non-zero DocumentParserTimings
    auto* properties = MakeGarbageCollected<TestResourceFetcherProperties>();
    auto* context = MakeGarbageCollected<MockFetchContext>();
    fetcher_ = MakeGarbageCollected<ResourceFetcher>(ResourceFetcherInit(
        properties->MakeDetachable(), context,
        base::MakeRefCounted<scheduler::FakeTaskRunner>(),
        base::MakeRefCounted<scheduler::FakeTaskRunner>(),
        MakeGarbageCollected<TestLoaderFactory>(
            platform_->GetURLLoaderMockFactory()),
        MakeGarbageCollected<MockContextLifecycleNotifier>(),
        nullptr /* back_forward_cache_loader_helper */));
    map_ = MakeGarbageCollected<WorkletModuleResponsesMap>();
  }

  class ClientImpl final : public GarbageCollected<ClientImpl>,
                           public ModuleScriptFetcher::Client {
   public:
    enum class Result { kInitial, kOK, kFailed };

    void NotifyFetchFinishedError(
        const HeapVector<Member<ConsoleMessage>>&) override {
      ASSERT_EQ(Result::kInitial, result_);
      result_ = Result::kFailed;
    }

    void NotifyFetchFinishedSuccess(
        const ModuleScriptCreationParams& params) override {
      ASSERT_EQ(Result::kInitial, result_);
      result_ = Result::kOK;
      params_.emplace(std::move(params));
    }

    Result GetResult() const { return result_; }
    bool HasParams() const { return params_.has_value(); }

   private:
    Result result_ = Result::kInitial;
    absl::optional<ModuleScriptCreationParams> params_;
  };

  void Fetch(const KURL& url, ClientImpl* client) {
    ResourceRequest resource_request(url);
    // TODO(nhiroki): Specify worklet-specific request context (e.g.,
    // "paintworklet").
    resource_request.SetRequestContext(
        mojom::blink::RequestContextType::SCRIPT);
    FetchParameters fetch_params =
        FetchParameters::CreateForTest(std::move(resource_request));
    fetch_params.SetModuleScript();
    WorkletModuleScriptFetcher* module_fetcher =
        MakeGarbageCollected<WorkletModuleScriptFetcher>(
            map_.Get(), ModuleScriptLoader::CreatePassKeyForTests());
    module_fetcher->Fetch(fetch_params, ModuleType::kJavaScript, fetcher_.Get(),
                          ModuleGraphLevel::kTopLevelModuleFetch, client);
  }

  void RunUntilIdle() {
    static_cast<scheduler::FakeTaskRunner*>(fetcher_->GetTaskRunner().get())
        ->RunUntilIdle();
  }

 protected:
  ScopedTestingPlatformSupport<FetchTestingPlatformSupport> platform_;
  Persistent<ResourceFetcher> fetcher_;
  Persistent<WorkletModuleResponsesMap> map_;
  const scoped_refptr<scheduler::FakeTaskRunner> task_runner_;
};

TEST_F(WorkletModuleResponsesMapTest, Basic) {
  const KURL kUrl("https://example.com/module.js");
  url_test_helpers::RegisterMockedURLLoad(
      kUrl, test::CoreTestDataPath("module.js"), "text/javascript",
      platform_->GetURLLoaderMockFactory());
  HeapVector<Member<ClientImpl>> clients;

  // An initial read call initiates a fetch request.
  clients.push_back(MakeGarbageCollected<ClientImpl>());
  Fetch(kUrl, clients[0]);
  EXPECT_EQ(ClientImpl::Result::kInitial, clients[0]->GetResult());
  EXPECT_FALSE(clients[0]->HasParams());

  // The entry is now being fetched. Following read calls should wait for the
  // completion.
  clients.push_back(MakeGarbageCollected<ClientImpl>());
  Fetch(kUrl, clients[1]);
  EXPECT_EQ(ClientImpl::Result::kInitial, clients[1]->GetResult());

  clients.push_back(MakeGarbageCollected<ClientImpl>());
  Fetch(kUrl, clients[2]);
  EXPECT_EQ(ClientImpl::Result::kInitial, clients[2]->GetResult());

  // Serve the fetch request. This should notify the waiting clients.
  platform_->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  RunUntilIdle();
  for (auto client : clients) {
    EXPECT_EQ(ClientImpl::Result::kOK, client->GetResult());
    EXPECT_TRUE(client->HasParams());
  }
}

TEST_F(WorkletModuleResponsesMapTest, Failure) {
  const KURL kUrl("https://example.com/module.js");
  url_test_helpers::RegisterMockedErrorURLLoad(
      kUrl, platform_->GetURLLoaderMockFactory());
  HeapVector<Member<ClientImpl>> clients;

  // An initial read call initiates a fetch request.
  clients.push_back(MakeGarbageCollected<ClientImpl>());
  Fetch(kUrl, clients[0]);
  EXPECT_EQ(ClientImpl::Result::kInitial, clients[0]->GetResult());
  EXPECT_FALSE(clients[0]->HasParams());

  // The entry is now being fetched. Following read calls should wait for the
  // completion.
  clients.push_back(MakeGarbageCollected<ClientImpl>());
  Fetch(kUrl, clients[1]);
  EXPECT_EQ(ClientImpl::Result::kInitial, clients[1]->GetResult());

  clients.push_back(MakeGarbageCollected<ClientImpl>());
  Fetch(kUrl, clients[2]);
  EXPECT_EQ(ClientImpl::Result::kInitial, clients[2]->GetResult());

  // Serve the fetch request with 404. This should fail the waiting clients.
  platform_->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  RunUntilIdle();
  for (auto client : clients) {
    EXPECT_EQ(ClientImpl::Result::kFailed, client->GetResult());
    EXPECT_FALSE(client->HasParams());
  }
}

TEST_F(WorkletModuleResponsesMapTest, Isolation) {
  const KURL kUrl1("https://example.com/module?1.js");
  const KURL kUrl2("https://example.com/module?2.js");
  url_test_helpers::RegisterMockedErrorURLLoad(
      kUrl1, platform_->GetURLLoaderMockFactory());
  url_test_helpers::RegisterMockedURLLoad(
      kUrl2, test::CoreTestDataPath("module.js"), "text/javascript",
      platform_->GetURLLoaderMockFactory());
  HeapVector<Member<ClientImpl>> clients;

  // An initial read call for |kUrl1| initiates a fetch request.
  clients.push_back(MakeGarbageCollected<ClientImpl>());
  Fetch(kUrl1, clients[0]);
  EXPECT_EQ(ClientImpl::Result::kInitial, clients[0]->GetResult());
  EXPECT_FALSE(clients[0]->HasParams());

  // The entry is now being fetched. Following read calls for |kUrl1| should
  // wait for the completion.
  clients.push_back(MakeGarbageCollected<ClientImpl>());
  Fetch(kUrl1, clients[1]);
  EXPECT_EQ(ClientImpl::Result::kInitial, clients[1]->GetResult());

  // An initial read call for |kUrl2| initiates a fetch request.
  clients.push_back(MakeGarbageCollected<ClientImpl>());
  Fetch(kUrl2, clients[2]);
  EXPECT_EQ(ClientImpl::Result::kInitial, clients[2]->GetResult());
  EXPECT_FALSE(clients[2]->HasParams());

  // The entry is now being fetched. Following read calls for |kUrl2| should
  // wait for the completion.
  clients.push_back(MakeGarbageCollected<ClientImpl>());
  Fetch(kUrl2, clients[3]);
  EXPECT_EQ(ClientImpl::Result::kInitial, clients[3]->GetResult());

  // The read call for |kUrl2| should not affect the other entry for |kUrl1|.
  EXPECT_EQ(ClientImpl::Result::kInitial, clients[0]->GetResult());

  // Serve the fetch requests.
  platform_->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  RunUntilIdle();
  EXPECT_EQ(ClientImpl::Result::kFailed, clients[0]->GetResult());
  EXPECT_FALSE(clients[0]->HasParams());
  EXPECT_EQ(ClientImpl::Result::kFailed, clients[1]->GetResult());
  EXPECT_FALSE(clients[1]->HasParams());
  EXPECT_EQ(ClientImpl::Result::kOK, clients[2]->GetResult());
  EXPECT_TRUE(clients[2]->HasParams());
  EXPECT_EQ(ClientImpl::Result::kOK, clients[3]->GetResult());
  EXPECT_TRUE(clients[3]->HasParams());
}

TEST_F(WorkletModuleResponsesMapTest, InvalidURL) {
  const KURL kEmptyURL;
  ASSERT_TRUE(kEmptyURL.IsEmpty());
  ClientImpl* client1 = MakeGarbageCollected<ClientImpl>();
  Fetch(kEmptyURL, client1);
  RunUntilIdle();
  EXPECT_EQ(ClientImpl::Result::kFailed, client1->GetResult());
  EXPECT_FALSE(client1->HasParams());

  const KURL kNullURL = NullURL();
  ASSERT_TRUE(kNullURL.IsNull());
  ClientImpl* client2 = MakeGarbageCollected<ClientImpl>();
  Fetch(kNullURL, client2);
  RunUntilIdle();
  EXPECT_EQ(ClientImpl::Result::kFailed, client2->GetResult());
  EXPECT_FALSE(client2->HasParams());

  const KURL kInvalidURL;
  ASSERT_FALSE(kInvalidURL.IsValid());
  ClientImpl* client3 = MakeGarbageCollected<ClientImpl>();
  Fetch(kInvalidURL, client3);
  RunUntilIdle();
  EXPECT_EQ(ClientImpl::Result::kFailed, client3->GetResult());
  EXPECT_FALSE(client3->HasParams());
}

TEST_F(WorkletModuleResponsesMapTest, Dispose) {
  const KURL kUrl1("https://example.com/module?1.js");
  const KURL kUrl2("https://example.com/module?2.js");
  url_test_helpers::RegisterMockedURLLoad(
      kUrl1, test::CoreTestDataPath("module.js"), "text/javascript",
      platform_->GetURLLoaderMockFactory());
  url_test_helpers::RegisterMockedURLLoad(
      kUrl2, test::CoreTestDataPath("module.js"), "text/javascript",
      platform_->GetURLLoaderMockFactory());
  HeapVector<Member<ClientImpl>> clients;

  // An initial read call for |kUrl1| creates a placeholder entry and asks the
  // client to fetch a module script.
  clients.push_back(MakeGarbageCollected<ClientImpl>());
  Fetch(kUrl1, clients[0]);
  EXPECT_EQ(ClientImpl::Result::kInitial, clients[0]->GetResult());
  EXPECT_FALSE(clients[0]->HasParams());

  // The entry is now being fetched. Following read calls for |kUrl1| should
  // wait for the completion.
  clients.push_back(MakeGarbageCollected<ClientImpl>());
  Fetch(kUrl1, clients[1]);
  EXPECT_EQ(ClientImpl::Result::kInitial, clients[1]->GetResult());

  // An initial read call for |kUrl2| also creates a placeholder entry and asks
  // the client to fetch a module script.
  clients.push_back(MakeGarbageCollected<ClientImpl>());
  Fetch(kUrl2, clients[2]);
  EXPECT_EQ(ClientImpl::Result::kInitial, clients[2]->GetResult());
  EXPECT_FALSE(clients[2]->HasParams());

  // The entry is now being fetched. Following read calls for |kUrl2| should
  // wait for the completion.
  clients.push_back(MakeGarbageCollected<ClientImpl>());
  Fetch(kUrl2, clients[3]);
  EXPECT_EQ(ClientImpl::Result::kInitial, clients[3]->GetResult());

  // Dispose() should notify to all waiting clients.
  map_->Dispose();
  RunUntilIdle();
  for (auto client : clients) {
    EXPECT_EQ(ClientImpl::Result::kFailed, client->GetResult());
    EXPECT_FALSE(client->HasParams());
  }
}

}  // namespace blink
