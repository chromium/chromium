// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/url_loader/worker_main_script_loader.h"

#include "base/containers/span.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "net/http/http_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/loader/resource_load_info_notifier.mojom.h"
#include "third_party/blink/public/mojom/navigation/renderer_eviction_reason.mojom-blink.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_observer.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/worker_main_script_loader_client.h"
#include "third_party/blink/renderer/platform/loader/testing/fake_resource_load_info_notifier.h"
#include "third_party/blink/renderer/platform/loader/testing/mock_fetch_context.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"

namespace blink {

namespace {

using ::testing::_;

const char kTopLevelScriptURL[] = "https://example.com/worker.js";
const char kHeader[] =
    "HTTP/1.1 200 OK\n"
    "Content-Type: text/javascript\n\n";
const char kFailHeader[] = "HTTP/1.1 404 Not Found\n\n";
const std::string kTopLevelScript = "fetch(\"empty.html\");";

class WorkerMainScriptLoaderTest : public testing::Test {
 public:
  WorkerMainScriptLoaderTest()
      : fake_loader_(pending_remote_loader_.InitWithNewPipeAndPassReceiver()),
        client_(MakeGarbageCollected<TestClient>()) {
    scoped_feature_list_.InitWithFeatureState(
        blink::features::kPlzDedicatedWorker, true);
  }
  ~WorkerMainScriptLoaderTest() override {
    // Forced GC in order to finalize objects depending on MockResourceObserver,
    // see details https://crbug.com/1132634.
    ThreadState::Current()->CollectAllGarbageForTesting();
  }

 protected:
  class TestClient final : public GarbageCollected<TestClient>,
                           public WorkerMainScriptLoaderClient {

   public:
    // Implements WorkerMainScriptLoaderClient.
    void DidReceiveDataWorkerMainScript(base::span<const char> data) override {
      if (!data_)
        data_ = SharedBuffer::Create(data.data(), data.size());
      else
        data_->Append(data.data(), data.size());
    }
    void OnFinishedLoadingWorkerMainScript() override { finished_ = true; }
    void OnFailedLoadingWorkerMainScript() override { failed_ = true; }

    bool LoadingIsFinished() const { return finished_; }
    bool LoadingIsFailed() const { return failed_; }

    SharedBuffer* Data() const { return data_.get(); }

    void Trace(Visitor* visitor) const override {
      visitor->Trace(worker_main_script_loader_);
    }

   private:
    Member<WorkerMainScriptLoader> worker_main_script_loader_;
    scoped_refptr<SharedBuffer> data_;
    bool finished_ = false;
    bool failed_ = false;
  };

  class FakeURLLoader final : public network::mojom::URLLoader {
   public:
    explicit FakeURLLoader(
        mojo::PendingReceiver<network::mojom::URLLoader> url_loader_receiver)
        : receiver_(this, std::move(url_loader_receiver)) {}
    ~FakeURLLoader() override = default;

    FakeURLLoader(const FakeURLLoader&) = delete;
    FakeURLLoader& operator=(const FakeURLLoader&) = delete;

    // network::mojom::URLLoader overrides.
    void FollowRedirect(const std::vector<std::string>&,
                        const net::HttpRequestHeaders&,
                        const net::HttpRequestHeaders&,
                        const std::optional<GURL>&) override {}
    void SetPriority(net::RequestPriority priority,
                     int32_t intra_priority_value) override {}
    void PauseReadingBodyFromNet() override {}
    void ResumeReadingBodyFromNet() override {}

   private:
    mojo::Receiver<network::mojom::URLLoader> receiver_;
  };

  class MockResourceLoadObserver : public ResourceLoadObserver {
   public:
    MOCK_METHOD2(DidStartRequest, void(const FetchParameters&, ResourceType));
    MOCK_METHOD6(WillSendRequest,
                 void(const ResourceRequest&,
                      const ResourceResponse& redirect_response,
                      ResourceType,
                      const ResourceLoaderOptions&,
                      RenderBlockingBehavior,
                      const Resource*));
    MOCK_METHOD3(DidChangePriority,
                 void(uint64_t identifier,
                      ResourceLoadPriority,
                      int intra_priority_value));
    MOCK_METHOD5(DidReceiveResponse,
                 void(uint64_t identifier,
                      const ResourceRequest& request,
                      const ResourceResponse& response,
                      const Resource* resource,
                      ResponseSource));
    MOCK_METHOD2(DidReceiveData,
                 void(uint64_t identifier, base::SpanOrSize<const char> chunk));
    MOCK_METHOD2(DidReceiveTransferSizeUpdate,
                 void(uint64_t identifier, int transfer_size_diff));
    MOCK_METHOD2(DidDownloadToBlob, void(uint64_t identifier, BlobDataHandle*));
    MOCK_METHOD4(DidFinishLoading,
                 void(uint64_t identifier,
                      base::TimeTicks finish_time,
                      int64_t encoded_data_length,
                      int64_t decoded_body_length));
    MOCK_METHOD5(DidFailLoading,
                 void(const KURL&,
                      uint64_t identifier,
                      const ResourceError&,
                      int64_t encoded_data_length,
                      IsInternalRequest));
    MOCK_METHOD2(DidChangeRenderBlockingBehavior,
                 void(Resource* resource, const FetchParameters& params));
    MOCK_METHOD0(InterestedInAllRequests, bool());
    MOCK_METHOD1(EvictFromBackForwardCache,
                 void(mojom::blink::RendererEvictionReason));
  };

  MojoCreateDataPipeOptions CreateDataPipeOptions() {
    MojoCreateDataPipeOptions options;
    options.struct_size = sizeof(MojoCreateDataPipeOptions);
    options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
    options.element_num_bytes = 1;
    options.capacity_num_bytes = 1024;
    return options;
  }

  std::unique_ptr<WorkerMainScriptLoadParameters> CreateMainScriptLoaderParams(
      const char* header,
      mojo::ScopedDataPipeProducerHandle* body_producer) {
    auto head = network::mojom::URLResponseHead::New();
    head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
        net::HttpUtil::AssembleRawHeaders(header));
    head->headers->GetMimeType(&head->mime_type);
    network::mojom::URLLoaderClientEndpointsPtr endpoints =
        network::mojom::URLLoaderClientEndpoints::New(
            std::move(pending_remote_loader_),
            loader_client_.BindNewPipeAndPassReceiver());

    std::unique_ptr<WorkerMainScriptLoadParameters>
        worker_main_script_load_params =
            std::make_unique<WorkerMainScriptLoadParameters>();
    worker_main_script_load_params->response_head = std::move(head);
    worker_main_script_load_params->url_loader_client_endpoints =
        std::move(endpoints);
    mojo::ScopedDataPipeConsumerHandle body_consumer;
    MojoCreateDataPipeOptions options = CreateDataPipeOptions();
    EXPECT_EQ(MOJO_RESULT_OK,
              mojo::CreateDataPipe(&options, *body_producer, body_consumer));
    worker_main_script_load_params->response_body = std::move(body_consumer);

    return worker_main_script_load_params;
  }

  WorkerMainScriptLoader* CreateWorkerMainScriptLoaderAndStartLoading(
      std::unique_ptr<WorkerMainScriptLoadParameters>
          worker_main_script_load_params,
      ResourceLoadObserver* observer,
      mojom::ResourceLoadInfoNotifier* resource_load_info_notifier) {
    ResourceRequest request(kTopLevelScriptURL);
    request.SetRequestContext(mojom::blink::RequestContextType::SHARED_WORKER);
    request.SetRequestDestination(
        network::mojom::RequestDestination::kSharedWorker);
    FetchParameters fetch_params(std::move(request),
                                 ResourceLoaderOptions(nullptr /* world */));
    WorkerMainScriptLoader* worker_main_script_loader =
        MakeGarbageCollected<WorkerMainScriptLoader>();
    MockFetchContext* fetch_context = MakeGarbageCollected<MockFetchContext>();
    fetch_context->SetResourceLoadInfoNotifier(resource_load_info_notifier);
    worker_main_script_loader->Start(fetch_params,
                                     std::move(worker_main_script_load_params),
                                     fetch_context, observer, client_);
    return worker_main_script_loader;
  }

  void Complete(int net_error) {
    loader_client_->OnComplete(network::URLLoaderCompletionStatus(net_error));
    base::RunLoop().RunUntilIdle();
  }

 protected:
  base::test::TaskEnvironment task_environment_;

  mojo::PendingRemote<network::mojom::URLLoader> pending_remote_loader_;
  mojo::Remote<network::mojom::URLLoaderClient> loader_client_;
  FakeURLLoader fake_loader_;

  Persistent<TestClient> client_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(WorkerMainScriptLoaderTest, ResponseWithSucessThenOnComplete) {
  mojo::ScopedDataPipeProducerHandle body_producer;
  std::unique_ptr<WorkerMainScriptLoadParameters>
      worker_main_script_load_params =
          CreateMainScriptLoaderParams(kHeader, &body_producer);
  MockResourceLoadObserver* mock_observer =
      MakeGarbageCollected<MockResourceLoadObserver>();
  FakeResourceLoadInfoNotifier fake_resource_load_info_notifier;
  EXPECT_CALL(*mock_observer, DidReceiveResponse(_, _, _, _, _));
  EXPECT_CALL(*mock_observer, DidReceiveData(_, _));
  EXPECT_CALL(*mock_observer, DidFinishLoading(_, _, _, _));
  EXPECT_CALL(*mock_observer, DidFailLoading(_, _, _, _, _)).Times(0);
  Persistent<WorkerMainScriptLoader> worker_main_script_loader =
      CreateWorkerMainScriptLoaderAndStartLoading(
          std::move(worker_main_script_load_params), mock_observer,
          &fake_resource_load_info_notifier);
  mojo::BlockingCopyFromString(kTopLevelScript, body_producer);
  body_producer.reset();
  Complete(net::OK);

  EXPECT_TRUE(client_->LoadingIsFinished());
  EXPECT_FALSE(client_->LoadingIsFailed());
  EXPECT_EQ(KURL(kTopLevelScriptURL),
            worker_main_script_loader->GetRequestURL());
  EXPECT_EQ(UTF8Encoding(), worker_main_script_loader->GetScriptEncoding());
  auto flatten_data = client_->Data()->CopyAs<Vector<char>>();
  EXPECT_EQ(kTopLevelScript, std::string(base::as_string_view(flatten_data)));
  EXPECT_EQ("text/javascript", fake_resource_load_info_notifier.GetMimeType());
}

TEST_F(WorkerMainScriptLoaderTest, ResponseWithFailureThenOnComplete) {
  mojo::ScopedDataPipeProducerHandle body_producer;
  std::unique_ptr<WorkerMainScriptLoadParameters>
      worker_main_script_load_params =
          CreateMainScriptLoaderParams(kFailHeader, &body_producer);
  MockResourceLoadObserver* mock_observer =
      MakeGarbageCollected<MockResourceLoadObserver>();
  FakeResourceLoadInfoNotifier fake_resource_load_info_notifier;
  EXPECT_CALL(*mock_observer, DidReceiveResponse(_, _, _, _, _));
  EXPECT_CALL(*mock_observer, DidFinishLoading(_, _, _, _)).Times(0);
  EXPECT_CALL(*mock_observer, DidFailLoading(_, _, _, _, _));
  Persistent<WorkerMainScriptLoader> worker_main_script_loader =
      CreateWorkerMainScriptLoaderAndStartLoading(
          std::move(worker_main_script_load_params), mock_observer,
          &fake_resource_load_info_notifier);
  mojo::BlockingCopyFromString("PAGE NOT FOUND\n", body_producer);
  Complete(net::OK);
  body_producer.reset();

  EXPECT_FALSE(client_->LoadingIsFinished());
  EXPECT_TRUE(client_->LoadingIsFailed());
}

TEST_F(WorkerMainScriptLoaderTest, DisconnectBeforeOnComplete) {
  mojo::ScopedDataPipeProducerHandle body_producer;
  std::unique_ptr<WorkerMainScriptLoadParameters>
      worker_main_script_load_params =
          CreateMainScriptLoaderParams(kHeader, &body_producer);
  MockResourceLoadObserver* mock_observer =
      MakeGarbageCollected<MockResourceLoadObserver>();
  FakeResourceLoadInfoNotifier fake_resource_load_info_notifier;
  EXPECT_CALL(*mock_observer, DidReceiveResponse(_, _, _, _, _));
  EXPECT_CALL(*mock_observer, DidFinishLoading(_, _, _, _)).Times(0);
  EXPECT_CALL(*mock_observer, DidFailLoading(_, _, _, _, _));
  Persistent<WorkerMainScriptLoader> worker_main_script_loader =
      CreateWorkerMainScriptLoaderAndStartLoading(
          std::move(worker_main_script_load_params), mock_observer,
          &fake_resource_load_info_notifier);
  loader_client_.reset();
  body_producer.reset();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(client_->LoadingIsFinished());
  EXPECT_TRUE(client_->LoadingIsFailed());
}

TEST_F(WorkerMainScriptLoaderTest, OnCompleteWithError) {
  mojo::ScopedDataPipeProducerHandle body_producer;
  std::unique_ptr<WorkerMainScriptLoadParameters>
      worker_main_script_load_params =
          CreateMainScriptLoaderParams(kHeader, &body_producer);
  MockResourceLoadObserver* mock_observer =
      MakeGarbageCollected<MockResourceLoadObserver>();
  FakeResourceLoadInfoNotifier fake_resource_load_info_notifier;
  EXPECT_CALL(*mock_observer, DidReceiveResponse(_, _, _, _, _));
  EXPECT_CALL(*mock_observer, DidReceiveData(_, _));
  EXPECT_CALL(*mock_observer, DidFinishLoading(_, _, _, _)).Times(0);
  EXPECT_CALL(*mock_observer, DidFailLoading(_, _, _, _, _));
  Persistent<WorkerMainScriptLoader> worker_main_script_loader =
      CreateWorkerMainScriptLoaderAndStartLoading(
          std::move(worker_main_script_load_params), mock_observer,
          &fake_resource_load_info_notifier);
  mojo::BlockingCopyFromString(kTopLevelScript, body_producer);
  Complete(net::ERR_FAILED);
  body_producer.reset();

  EXPECT_FALSE(client_->LoadingIsFinished());
  EXPECT_TRUE(client_->LoadingIsFailed());
}

}  // namespace

}  // namespace blink
