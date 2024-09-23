// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/worker_host/shared_worker_service_impl.h"

#include <memory>
#include <set>
#include <string>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/queue.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/worker_host/mock_shared_worker.h"
#include "content/browser/worker_host/shared_worker_connector_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_utils.h"
#include "content/test/fake_network_url_loader_factory.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/messaging/message_port_channel.h"
#include "third_party/blink/public/mojom/worker/shared_worker_info.mojom.h"
#include "url/origin.h"

using blink::MessagePortChannel;

namespace content {

namespace {

using ::testing::ElementsAre;

const ukm::SourceId kClientUkmSourceId = 1;

void ConnectToSharedWorker(
    mojo::Remote<blink::mojom::SharedWorkerConnector> connector,
    const GURL& url,
    const std::string& name,
    MockSharedWorkerClient* client,
    MessagePortChannel* local_port) {
  auto options = blink::mojom::WorkerOptions::New();
  options->name = name;
  blink::mojom::SharedWorkerInfoPtr info(blink::mojom::SharedWorkerInfo::New(
      url, std::move(options),
      std::vector<network::mojom::ContentSecurityPolicyPtr>(),
      blink::mojom::FetchClientSettingsObject::New(
          network::mojom::ReferrerPolicy::kDefault, GURL(),
          blink::mojom::InsecureRequestsPolicy::kDoNotUpgrade),
      blink::mojom::SharedWorkerSameSiteCookies::kAll));

  blink::MessagePortDescriptorPair pipe;
  *local_port = MessagePortChannel(pipe.TakePort0());

  mojo::PendingRemote<blink::mojom::SharedWorkerClient> client_proxy;
  client->Bind(client_proxy.InitWithNewPipeAndPassReceiver());

  connector->Connect(std::move(info), std::move(client_proxy),
                     blink::mojom::SharedWorkerCreationContextType::kSecure,
                     pipe.TakePort1(), mojo::NullRemote(), kClientUkmSourceId);
}

}  // namespace

class SharedWorkerServiceImplTest : public RenderViewHostImplTestHarness {
 public:
  class MockRenderProcessHostFactoryForSharedWorker;

  class MockRenderProcessHostForSharedWorker : public MockRenderProcessHost {
   public:
    MockRenderProcessHostForSharedWorker(BrowserContext* browser_context,
                                         SharedWorkerServiceImplTest* test)
        : MockRenderProcessHost(browser_context), test_(test) {}

    ~MockRenderProcessHostForSharedWorker() override {}

    void BindReceiver(mojo::GenericPendingReceiver receiver) override {
      if (*receiver.interface_name() !=
          blink::mojom::SharedWorkerFactory::Name_)
        return;
      test_->BindSharedWorkerFactory(GetID(), receiver.PassPipe());
    }

    const raw_ptr<SharedWorkerServiceImplTest> test_;
  };

  class MockRenderProcessHostFactoryForSharedWorker
      : public MockRenderProcessHostFactory {
   public:
    explicit MockRenderProcessHostFactoryForSharedWorker(
        SharedWorkerServiceImplTest* test)
        : test_(test) {}

    RenderProcessHost* CreateRenderProcessHost(
        BrowserContext* browser_context,
        SiteInstance* site_instance) override {
      auto host = std::make_unique<MockRenderProcessHostForSharedWorker>(
          browser_context, test_);
      processes_.push_back(std::move(host));
      return processes_.back().get();
    }

    void Remove(MockRenderProcessHostForSharedWorker* host) {
      for (auto it = processes_.begin(); it != processes_.end(); ++it) {
        if (it->get() == host) {
          processes_.erase(it);
          break;
        }
      }
    }

   private:
    const raw_ptr<SharedWorkerServiceImplTest> test_;
    std::vector<std::unique_ptr<MockRenderProcessHostForSharedWorker>>
        processes_;
  };

  SharedWorkerServiceImplTest(const SharedWorkerServiceImplTest&) = delete;
  SharedWorkerServiceImplTest& operator=(const SharedWorkerServiceImplTest&) =
      delete;

  mojo::Remote<blink::mojom::SharedWorkerConnector> MakeSharedWorkerConnector(
      GlobalRenderFrameHostId render_frame_host_id) {
    mojo::Remote<blink::mojom::SharedWorkerConnector> connector;
    SharedWorkerConnectorImpl::Create(render_frame_host_id,
                                      connector.BindNewPipeAndPassReceiver());
    return connector;
  }

  // Waits until a mojo::PendingReceiver<blink::mojom::SharedWorkerFactory>
  // is received. Return a pair of the receiver and the process id.
  std::pair<mojo::PendingReceiver<blink::mojom::SharedWorkerFactory>, int>
  WaitForFactoryReceiver() {
    if (factorys_receivers_.empty()) {
      base::RunLoop run_loop;
      factory_receiver_callback_ = run_loop.QuitClosure();
      run_loop.Run();
    }

    DCHECK(!factorys_receivers_.empty());
    auto rv = std::move(factorys_receivers_.front());
    factorys_receivers_.pop();
    return rv;
  }

  // Receives a PendingReceiver<blink::mojom::SharedWorkerFactory>.
  void BindSharedWorkerFactory(int process_id,
                               mojo::ScopedMessagePipeHandle handle) {
    if (factory_receiver_callback_)
      std::move(factory_receiver_callback_).Run();

    factorys_receivers_.emplace(std::move(handle), process_id);
  }

  std::unique_ptr<TestWebContents> CreateWebContents(const GURL& url) {
    std::unique_ptr<TestWebContents> web_contents(TestWebContents::Create(
        browser_context_.get(),
        SiteInstanceImpl::Create(browser_context_.get())));
    web_contents->NavigateAndCommit(url);
    return web_contents;
  }

 protected:
  SharedWorkerServiceImplTest() : browser_context_(new TestBrowserContext()) {}

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    render_process_host_factory_ =
        std::make_unique<MockRenderProcessHostFactoryForSharedWorker>(this);
    RenderProcessHostImpl::set_render_process_host_factory_for_testing(
        render_process_host_factory_.get());

    fake_url_loader_factory_ = std::make_unique<FakeNetworkURLLoaderFactory>();
    url_loader_factory_wrapper_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            fake_url_loader_factory_.get());
    static_cast<SharedWorkerServiceImpl*>(
        browser_context_->GetDefaultStoragePartition()
            ->GetSharedWorkerService())
        ->SetURLLoaderFactoryForTesting(url_loader_factory_wrapper_);
  }

  void TearDown() override {
    if (url_loader_factory_wrapper_)
      url_loader_factory_wrapper_->Detach();
    render_process_host_factory_.reset();

    browser_context_.reset();
    RenderProcessHostImpl::set_render_process_host_factory_for_testing(nullptr);
    RenderViewHostImplTestHarness::TearDown();
  }

  std::unique_ptr<TestBrowserContext> browser_context_;

  // Holds pending receivers of SharedWorkerFactory.
  base::queue<
      std::pair<mojo::PendingReceiver<blink::mojom::SharedWorkerFactory>,
                int /* process_id */>>
      factorys_receivers_;

  // The callback is called when a pending receiver of SharedWorkerFactory is
  // received.
  base::OnceClosure factory_receiver_callback_;

  std::unique_ptr<MockRenderProcessHostFactoryForSharedWorker>
      render_process_host_factory_;

  std::unique_ptr<FakeNetworkURLLoaderFactory> fake_url_loader_factory_;
  scoped_refptr<network::WeakWrapperSharedURLLoaderFactory>
      url_loader_factory_wrapper_;
};

TEST_F(SharedWorkerServiceImplTest, BasicTest) {
  std::unique_ptr<TestWebContents> web_contents =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host = web_contents->GetPrimaryMainFrame();
  MockRenderProcessHost* renderer_host = render_frame_host->GetProcess();
  const int process_id = renderer_host->GetID();
  renderer_host->OverrideBinderForTesting(
      blink::mojom::SharedWorkerFactory::Name_,
      base::BindRepeating(&SharedWorkerServiceImplTest::BindSharedWorkerFactory,
                          base::Unretained(this), process_id));

  MockSharedWorkerClient client;
  MessagePortChannel local_port;
  const GURL kUrl("http://example.com/w.js");
  ConnectToSharedWorker(
      MakeSharedWorkerConnector(render_frame_host->GetGlobalId()), kUrl, "name",
      &client, &local_port);

  mojo::PendingReceiver<blink::mojom::SharedWorkerFactory> factory_receiver;
  std::tie(factory_receiver, std::ignore) = WaitForFactoryReceiver();
  MockSharedWorkerFactory factory(std::move(factory_receiver));
  base::RunLoop().RunUntilIdle();

  mojo::Remote<blink::mojom::SharedWorkerHost> worker_host;
  mojo::PendingReceiver<blink::mojom::SharedWorker> worker_receiver;
  EXPECT_TRUE(factory.CheckReceivedCreateSharedWorker(
      kUrl, "name", std::vector<network::mojom::ContentSecurityPolicyPtr>(),
      &worker_host, &worker_receiver));
  MockSharedWorker worker(std::move(worker_receiver));
  base::RunLoop().RunUntilIdle();

  int connection_request_id;
  MessagePortChannel port;
  EXPECT_TRUE(worker.CheckReceivedConnect(&connection_request_id, &port));

  EXPECT_TRUE(client.CheckReceivedOnCreated());

  // Simulate events the shared worker would send.

  // Create message pipes. We may need to keep |devtools_agent_receiver| and
  // |devtools_agent_host_remote| if we want not to invoke connection error
  // handlers.
  mojo::PendingRemote<blink::mojom::DevToolsAgent> devtools_agent_remote;
  mojo::PendingReceiver<blink::mojom::DevToolsAgent> devtools_agent_receiver =
      devtools_agent_remote.InitWithNewPipeAndPassReceiver();
  mojo::PendingRemote<blink::mojom::DevToolsAgentHost>
      devtools_agent_host_remote;
  worker_host->OnReadyForInspection(
      std::move(devtools_agent_remote),
      devtools_agent_host_remote.InitWithNewPipeAndPassReceiver());

  worker_host->OnConnected(connection_request_id);

  base::RunLoop().RunUntilIdle();

  auto feature3 = blink::mojom::WebFeature::kCoepNoneSharedWorker;
  worker_host->OnFeatureUsed(feature3);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(client.CheckReceivedOnFeatureUsed(feature3));

  // Verify that |port| corresponds to |connector->local_port()|.
  std::string expected_message("test1");
  EXPECT_TRUE(mojo::test::WriteTextMessage(
      local_port.GetHandle().handle().get(), expected_message));
  std::string received_message;
  EXPECT_TRUE(mojo::test::ReadTextMessage(port.GetHandle().handle().get(),
                                          &received_message));
  EXPECT_EQ(expected_message, received_message);

  // Send feature from shared worker to host.
  auto feature1 = static_cast<blink::mojom::WebFeature>(124);
  worker_host->OnFeatureUsed(feature1);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(client.CheckReceivedOnFeatureUsed(feature1));

  // A message should be sent only one time per feature.
  worker_host->OnFeatureUsed(feature1);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(client.CheckNotReceivedOnFeatureUsed());

  // Send another feature.
  auto feature2 = static_cast<blink::mojom::WebFeature>(901);
  worker_host->OnFeatureUsed(feature2);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(client.CheckReceivedOnFeatureUsed(feature2));

  // Tear down the worker host.
  worker_host->OnContextClosed();
  base::RunLoop().RunUntilIdle();
}

// Tests that the shared worker will not be started if the hosting web contents
// is destroyed while the script is being fetched.
TEST_F(SharedWorkerServiceImplTest, WebContentsDestroyed) {
  std::unique_ptr<TestWebContents> web_contents =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host = web_contents->GetPrimaryMainFrame();
  MockRenderProcessHost* renderer_host = render_frame_host->GetProcess();
  const int process_id = renderer_host->GetID();
  renderer_host->OverrideBinderForTesting(
      blink::mojom::SharedWorkerFactory::Name_,
      base::BindRepeating(&SharedWorkerServiceImplTest::BindSharedWorkerFactory,
                          base::Unretained(this), process_id));

  MockSharedWorkerClient client;
  MessagePortChannel local_port;
  const GURL kUrl("http://example.com/w.js");
  ConnectToSharedWorker(
      MakeSharedWorkerConnector(render_frame_host->GetGlobalId()), kUrl, "name",
      &client, &local_port);

  // Now asynchronously destroy |web_contents| so that the startup sequence at
  // least reaches SharedWorkerServiceImpl::StartWorker().
  base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(
      FROM_HERE, std::move(web_contents));

  base::RunLoop().RunUntilIdle();

  // The shared worker creation failed, which means the client never connects
  // and receives OnScriptLoadFailed().
  EXPECT_TRUE(client.CheckReceivedOnCreated());
  EXPECT_FALSE(client.CheckReceivedOnConnected({}));
  EXPECT_TRUE(client.CheckReceivedOnScriptLoadFailed());
}

TEST_F(SharedWorkerServiceImplTest, TwoRendererTest) {
  // The first renderer host.
  std::unique_ptr<TestWebContents> web_contents0 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host0 =
      web_contents0->GetPrimaryMainFrame();
  MockRenderProcessHost* renderer_host0 = render_frame_host0->GetProcess();

  MockSharedWorkerClient client0;
  MessagePortChannel local_port0;
  const GURL kUrl("http://example.com/w.js");
  ConnectToSharedWorker(
      MakeSharedWorkerConnector(render_frame_host0->GetGlobalId()), kUrl,
      "name", &client0, &local_port0);

  base::RunLoop().RunUntilIdle();

  auto [factory_receiver, process_id] = WaitForFactoryReceiver();
  // Currently shared worker is created in the same process with the creator's
  // process by default.
  EXPECT_EQ(renderer_host0->GetID(), process_id);

  MockSharedWorkerFactory factory(std::move(factory_receiver));
  base::RunLoop().RunUntilIdle();

  mojo::Remote<blink::mojom::SharedWorkerHost> worker_host;
  mojo::PendingReceiver<blink::mojom::SharedWorker> worker_receiver;
  EXPECT_TRUE(factory.CheckReceivedCreateSharedWorker(
      kUrl, "name", std::vector<network::mojom::ContentSecurityPolicyPtr>(),
      &worker_host, &worker_receiver));
  MockSharedWorker worker(std::move(worker_receiver));
  base::RunLoop().RunUntilIdle();

  int connection_request_id0;
  MessagePortChannel port0;
  EXPECT_TRUE(worker.CheckReceivedConnect(&connection_request_id0, &port0));

  EXPECT_TRUE(client0.CheckReceivedOnCreated());

  // Simulate events the shared worker would send.

  // Create message pipes. We may need to keep |devtools_agent_receiver| and
  // |devtools_agent_host_remote| if we want not to invoke connection error
  // handlers.
  mojo::PendingRemote<blink::mojom::DevToolsAgent> devtools_agent_remote;
  mojo::PendingReceiver<blink::mojom::DevToolsAgent> devtools_agent_receiver =
      devtools_agent_remote.InitWithNewPipeAndPassReceiver();
  mojo::PendingRemote<blink::mojom::DevToolsAgentHost>
      devtools_agent_host_remote;
  worker_host->OnReadyForInspection(
      std::move(devtools_agent_remote),
      devtools_agent_host_remote.InitWithNewPipeAndPassReceiver());
  worker_host->OnConnected(connection_request_id0);

  base::RunLoop().RunUntilIdle();

  auto feature4 = blink::mojom::WebFeature::kCoepNoneSharedWorker;
  worker_host->OnFeatureUsed(feature4);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(client0.CheckReceivedOnFeatureUsed(feature4));

  // Verify that |port0| corresponds to |connector0->local_port()|.
  std::string expected_message0("test1");
  EXPECT_TRUE(mojo::test::WriteTextMessage(
      local_port0.GetHandle().handle().get(), expected_message0));
  std::string received_message0;
  EXPECT_TRUE(mojo::test::ReadTextMessage(port0.GetHandle().handle().get(),
                                          &received_message0));
  EXPECT_EQ(expected_message0, received_message0);

  auto feature1 = static_cast<blink::mojom::WebFeature>(124);
  worker_host->OnFeatureUsed(feature1);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(client0.CheckReceivedOnFeatureUsed(feature1));
  auto feature2 = static_cast<blink::mojom::WebFeature>(901);
  worker_host->OnFeatureUsed(feature2);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(client0.CheckReceivedOnFeatureUsed(feature2));

  // Only a single worker instance in process 0.
  EXPECT_EQ(1u, renderer_host0->GetWorkerRefCount());

  // The second renderer host.
  std::unique_ptr<TestWebContents> web_contents1 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host1 =
      web_contents1->GetPrimaryMainFrame();
  MockRenderProcessHost* renderer_host1 = render_frame_host1->GetProcess();

  MockSharedWorkerClient client1;
  MessagePortChannel local_port1;
  ConnectToSharedWorker(
      MakeSharedWorkerConnector(render_frame_host1->GetGlobalId()), kUrl,
      "name", &client1, &local_port1);

  base::RunLoop().RunUntilIdle();

  // Should not have tried to create a new shared worker.
  EXPECT_TRUE(factorys_receivers_.empty());

  int connection_request_id1;
  MessagePortChannel port1;
  EXPECT_TRUE(worker.CheckReceivedConnect(&connection_request_id1, &port1));

  EXPECT_TRUE(client1.CheckReceivedOnCreated());

  // Only a single worker instance in process 0.
  EXPECT_EQ(1u, renderer_host0->GetWorkerRefCount());
  EXPECT_EQ(0u, renderer_host1->GetWorkerRefCount());

  worker_host->OnConnected(connection_request_id1);

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(client1.CheckReceivedOnConnected({feature1, feature2, feature4}));

  // Verify that |worker_msg_port2| corresponds to |connector1->local_port()|.
  std::string expected_message1("test2");
  EXPECT_TRUE(mojo::test::WriteTextMessage(
      local_port1.GetHandle().handle().get(), expected_message1));
  std::string received_message1;
  EXPECT_TRUE(mojo::test::ReadTextMessage(port1.GetHandle().handle().get(),
                                          &received_message1));
  EXPECT_EQ(expected_message1, received_message1);

  worker_host->OnFeatureUsed(feature1);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(client0.CheckNotReceivedOnFeatureUsed());
  EXPECT_TRUE(client1.CheckNotReceivedOnFeatureUsed());

  auto feature3 = static_cast<blink::mojom::WebFeature>(1019);
  worker_host->OnFeatureUsed(feature3);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(client0.CheckReceivedOnFeatureUsed(feature3));
  EXPECT_TRUE(client1.CheckReceivedOnFeatureUsed(feature3));

  // Tear down the worker host.
  worker_host->OnContextClosed();
  base::RunLoop().RunUntilIdle();
}

TEST_F(SharedWorkerServiceImplTest, CreateWorkerTest_NormalCase) {
  const GURL kUrl("http://example.com/w.js");
  const char kName[] = "name";

  // The first renderer host.
  std::unique_ptr<TestWebContents> web_contents0 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host0 =
      web_contents0->GetPrimaryMainFrame();
  MockRenderProcessHost* renderer_host0 = render_frame_host0->GetProcess();
  const int process_id0 = renderer_host0->GetID();
  renderer_host0->OverrideBinderForTesting(
      blink::mojom::SharedWorkerFactory::Name_,
      base::BindRepeating(&SharedWorkerServiceImplTest::BindSharedWorkerFactory,
                          base::Unretained(this), process_id0));

  // The second renderer host.
  std::unique_ptr<TestWebContents> web_contents1 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host1 =
      web_contents1->GetPrimaryMainFrame();

  // First client, creates worker.

  MockSharedWorkerClient client0;
  MessagePortChannel local_port0;
  ConnectToSharedWorker(
      MakeSharedWorkerConnector(render_frame_host0->GetGlobalId()), kUrl, kName,
      &client0, &local_port0);
  base::RunLoop().RunUntilIdle();

  mojo::PendingReceiver<blink::mojom::SharedWorkerFactory> factory_receiver;
  std::tie(factory_receiver, std::ignore) = WaitForFactoryReceiver();
  MockSharedWorkerFactory factory(std::move(factory_receiver));
  base::RunLoop().RunUntilIdle();

  mojo::Remote<blink::mojom::SharedWorkerHost> worker_host;
  mojo::PendingReceiver<blink::mojom::SharedWorker> worker_receiver;
  EXPECT_TRUE(factory.CheckReceivedCreateSharedWorker(
      kUrl, kName, std::vector<network::mojom::ContentSecurityPolicyPtr>(),
      &worker_host, &worker_receiver));
  MockSharedWorker worker(std::move(worker_receiver));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(worker.CheckReceivedConnect(nullptr, nullptr));
  EXPECT_TRUE(client0.CheckReceivedOnCreated());

  // Second client, same worker.

  MockSharedWorkerClient client1;
  MessagePortChannel local_port1;
  ConnectToSharedWorker(
      MakeSharedWorkerConnector(render_frame_host1->GetGlobalId()), kUrl, kName,
      &client1, &local_port1);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(factorys_receivers_.empty());

  EXPECT_TRUE(worker.CheckReceivedConnect(nullptr, nullptr));
  EXPECT_TRUE(client1.CheckReceivedOnCreated());

  // Cleanup

  client0.Close();
  client1.Close();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(worker.CheckReceivedTerminate());
}

TEST_F(SharedWorkerServiceImplTest, CreateWorkerTest_NormalCase_URLMismatch) {
  const GURL kUrl0("http://example.com/w0.js");
  const GURL kUrl1("http://example.com/w1.js");
  const char kName[] = "name";

  // The first renderer host.
  std::unique_ptr<TestWebContents> web_contents0 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host0 =
      web_contents0->GetPrimaryMainFrame();

  // The second renderer host.
  std::unique_ptr<TestWebContents> web_contents1 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host1 =
      web_contents1->GetPrimaryMainFrame();

  // First client, creates worker.

  MockSharedWorkerClient client0;
  MessagePortChannel local_port0;
  ConnectToSharedWorker(
      MakeSharedWorkerConnector(render_frame_host0->GetGlobalId()), kUrl0,
      kName, &client0, &local_port0);
  base::RunLoop().RunUntilIdle();

  mojo::PendingReceiver<blink::mojom::SharedWorkerFactory> factory_receiver0;
  std::tie(factory_receiver0, std::ignore) = WaitForFactoryReceiver();
  MockSharedWorkerFactory factory0(std::move(factory_receiver0));
  base::RunLoop().RunUntilIdle();

  mojo::Remote<blink::mojom::SharedWorkerHost> worker_host0;
  mojo::PendingReceiver<blink::mojom::SharedWorker> worker_receiver0;
  EXPECT_TRUE(factory0.CheckReceivedCreateSharedWorker(
      kUrl0, kName, std::vector<network::mojom::ContentSecurityPolicyPtr>(),
      &worker_host0, &worker_receiver0));
  MockSharedWorker worker0(std::move(worker_receiver0));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(worker0.CheckReceivedConnect(nullptr, nullptr));
  EXPECT_TRUE(client0.CheckReceivedOnCreated());

  // Second client, creates worker.

  MockSharedWorkerClient client1;
  MessagePortChannel local_port1;
  ConnectToSharedWorker(
      MakeSharedWorkerConnector(render_frame_host1->GetGlobalId()), kUrl1,
      kName, &client1, &local_port1);
  base::RunLoop().RunUntilIdle();

  mojo::PendingReceiver<blink::mojom::SharedWorkerFactory> factory_receiver1;
  std::tie(factory_receiver1, std::ignore) = WaitForFactoryReceiver();
  MockSharedWorkerFactory factory1(std::move(factory_receiver1));
  base::RunLoop().RunUntilIdle();

  mojo::Remote<blink::mojom::SharedWorkerHost> worker_host1;
  mojo::PendingReceiver<blink::mojom::SharedWorker> worker_receiver1;
  EXPECT_TRUE(factory1.CheckReceivedCreateSharedWorker(
      kUrl1, kName, std::vector<network::mojom::ContentSecurityPolicyPtr>(),
      &worker_host1, &worker_receiver1));
  MockSharedWorker worker1(std::move(worker_receiver1));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(worker1.CheckReceivedConnect(nullptr, nullptr));
  EXPECT_TRUE(client1.CheckReceivedOnCreated());

  // Cleanup

  client0.Close();
  client1.Close();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(worker0.CheckReceivedTerminate());
  EXPECT_TRUE(worker1.CheckReceivedTerminate());
}

TEST_F(SharedWorkerServiceImplTest, CreateWorkerTest_NormalCase_NameMismatch) {
  const GURL kUrl("http://example.com/w.js");
  const char kName0[] = "name0";
  const char kName1[] = "name1";

  // The first renderer host.
  std::unique_ptr<TestWebContents> web_contents0 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host0 =
      web_contents0->GetPrimaryMainFrame();
  MockRenderProcessHost* renderer_host0 = render_frame_host0->GetProcess();
  const int process_id0 = renderer_host0->GetID();
  renderer_host0->OverrideBinderForTesting(
      blink::mojom::SharedWorkerFactory::Name_,
      base::BindRepeating(&SharedWorkerServiceImplTest::BindSharedWorkerFactory,
                          base::Unretained(this), process_id0));

  // The second renderer host.
  std::unique_ptr<TestWebContents> web_contents1 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host1 =
      web_contents1->GetPrimaryMainFrame();
  MockRenderProcessHost* renderer_host1 = render_frame_host1->GetProcess();
  const int process_id1 = renderer_host1->GetID();
  renderer_host1->OverrideBinderForTesting(
      blink::mojom::SharedWorkerFactory::Name_,
      base::BindRepeating(&SharedWorkerServiceImplTest::BindSharedWorkerFactory,
                          base::Unretained(this), process_id1));

  // First client, creates worker.

  MockSharedWorkerClient client0;
  MessagePortChannel local_port0;
  ConnectToSharedWorker(
      MakeSharedWorkerConnector(render_frame_host0->GetGlobalId()), kUrl,
      kName0, &client0, &local_port0);
  base::RunLoop().RunUntilIdle();

  mojo::PendingReceiver<blink::mojom::SharedWorkerFactory> factory_receiver0;
  std::tie(factory_receiver0, std::ignore) = WaitForFactoryReceiver();
  MockSharedWorkerFactory factory0(std::move(factory_receiver0));
  base::RunLoop().RunUntilIdle();

  mojo::Remote<blink::mojom::SharedWorkerHost> worker_host0;
  mojo::PendingReceiver<blink::mojom::SharedWorker> worker_receiver0;
  EXPECT_TRUE(factory0.CheckReceivedCreateSharedWorker(
      kUrl, kName0, std::vector<network::mojom::ContentSecurityPolicyPtr>(),
      &worker_host0, &worker_receiver0));
  MockSharedWorker worker0(std::move(worker_receiver0));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(worker0.CheckReceivedConnect(nullptr, nullptr));
  EXPECT_TRUE(client0.CheckReceivedOnCreated());

  // Second client, creates worker.

  MockSharedWorkerClient client1;
  MessagePortChannel local_port1;
  ConnectToSharedWorker(
      MakeSharedWorkerConnector(render_frame_host1->GetGlobalId()), kUrl,
      kName1, &client1, &local_port1);
  base::RunLoop().RunUntilIdle();

  mojo::PendingReceiver<blink::mojom::SharedWorkerFactory> factory_receiver1;
  std::tie(factory_receiver1, std::ignore) = WaitForFactoryReceiver();
  MockSharedWorkerFactory factory1(std::move(factory_receiver1));
  base::RunLoop().RunUntilIdle();

  mojo::Remote<blink::mojom::SharedWorkerHost> worker_host1;
  mojo::PendingReceiver<blink::mojom::SharedWorker> worker_receiver1;
  EXPECT_TRUE(factory1.CheckReceivedCreateSharedWorker(
      kUrl, kName1, std::vector<network::mojom::ContentSecurityPolicyPtr>(),
      &worker_host1, &worker_receiver1));
  MockSharedWorker worker1(std::move(worker_receiver1));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(worker1.CheckReceivedConnect(nullptr, nullptr));
  EXPECT_TRUE(client1.CheckReceivedOnCreated());

  // Cleanup

  client0.Close();
  client1.Close();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(worker0.CheckReceivedTerminate());
  EXPECT_TRUE(worker1.CheckReceivedTerminate());
}

TEST_F(SharedWorkerServiceImplTest, CreateWorkerTest_PendingCase) {
  const GURL kUrl("http://example.com/w.js");
  const char kName[] = "name";

  // The first renderer host.
  std::unique_ptr<TestWebContents> web_contents0 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host0 =
      web_contents0->GetPrimaryMainFrame();
  MockRenderProcessHost* renderer_host0 = render_frame_host0->GetProcess();
  const int process_id0 = renderer_host0->GetID();
  renderer_host0->OverrideBinderForTesting(
      blink::mojom::SharedWorkerFactory::Name_,
      base::BindRepeating(&SharedWorkerServiceImplTest::BindSharedWorkerFactory,
                          base::Unretained(this), process_id0));

  // The second renderer host.
  std::unique_ptr<TestWebContents> web_contents1 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host1 =
      web_contents1->GetPrimaryMainFrame();
  MockRenderProcessHost* renderer_host1 = render_frame_host1->GetProcess();
  const int process_id1 = renderer_host1->GetID();
  renderer_host1->OverrideBinderForTesting(
      blink::mojom::SharedWorkerFactory::Name_,
      base::BindRepeating(&SharedWorkerServiceImplTest::BindSharedWorkerFactory,
                          base::Unretained(this), process_id1));

  // First client and second client are created before the worker starts.

  MockSharedWorkerClient client0;
  MessagePortChannel local_port0;
  ConnectToSharedWorker(
      MakeSharedWorkerConnector(render_frame_host0->GetGlobalId()), kUrl, kName,
      &client0, &local_port0);

  MockSharedWorkerClient client1;
  MessagePortChannel local_port1;
  ConnectToSharedWorker(
      MakeSharedWorkerConnector(render_frame_host1->GetGlobalId()), kUrl, kName,
      &client1, &local_port1);

  base::RunLoop().RunUntilIdle();

  // Check that the worker was created. The receiver could come from either
  // process.

  mojo::PendingReceiver<blink::mojom::SharedWorkerFactory> factory_receiver;
  std::tie(factory_receiver, std::ignore) = WaitForFactoryReceiver();
  MockSharedWorkerFactory factory(std::move(factory_receiver));

  base::RunLoop().RunUntilIdle();

  mojo::Remote<blink::mojom::SharedWorkerHost> worker_host;
  mojo::PendingReceiver<blink::mojom::SharedWorker> worker_receiver;
  EXPECT_TRUE(factory.CheckReceivedCreateSharedWorker(
      kUrl, kName, std::vector<network::mojom::ContentSecurityPolicyPtr>(),
      &worker_host, &worker_receiver));
  MockSharedWorker worker(std::move(worker_receiver));

  base::RunLoop().RunUntilIdle();

  // Check that the worker received two connections.

  EXPECT_TRUE(worker.CheckReceivedConnect(nullptr, nullptr));
  EXPECT_TRUE(client0.CheckReceivedOnCreated());

  EXPECT_TRUE(worker.CheckReceivedConnect(nullptr, nullptr));
  EXPECT_TRUE(client1.CheckReceivedOnCreated());

  // Cleanup

  client0.Close();
  client1.Close();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(worker.CheckReceivedTerminate());
}

TEST_F(SharedWorkerServiceImplTest, CreateWorkerTest_PendingCase_URLMismatch) {
  const GURL kUrl0("http://example.com/w0.js");
  const GURL kUrl1("http://example.com/w1.js");
  const char kName[] = "name";

  // The first renderer host.
  std::unique_ptr<TestWebContents> web_contents0 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host0 =
      web_contents0->GetPrimaryMainFrame();

  // The second renderer host.
  std::unique_ptr<TestWebContents> web_contents1 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host1 =
      web_contents1->GetPrimaryMainFrame();

  // First client and second client are created before the workers start.

  MockSharedWorkerClient client0;
  MessagePortChannel local_port0;
  ConnectToSharedWorker(
      MakeSharedWorkerConnector(render_frame_host0->GetGlobalId()), kUrl0,
      kName, &client0, &local_port0);

  MockSharedWorkerClient client1;
  MessagePortChannel local_port1;
  ConnectToSharedWorker(
      MakeSharedWorkerConnector(render_frame_host1->GetGlobalId()), kUrl1,
      kName, &client1, &local_port1);

  base::RunLoop().RunUntilIdle();

  // Check that both workers were created.

  mojo::PendingReceiver<blink::mojom::SharedWorkerFactory> factory_receiver0;
  std::tie(factory_receiver0, std::ignore) = WaitForFactoryReceiver();
  MockSharedWorkerFactory factory0(std::move(factory_receiver0));

  mojo::PendingReceiver<blink::mojom::SharedWorkerFactory> factory_receiver1;
  std::tie(factory_receiver1, std::ignore) = WaitForFactoryReceiver();
  MockSharedWorkerFactory factory1(std::move(factory_receiver1));

  base::RunLoop().RunUntilIdle();

  mojo::Remote<blink::mojom::SharedWorkerHost> worker_host0;
  mojo::PendingReceiver<blink::mojom::SharedWorker> worker_receiver0;
  EXPECT_TRUE(factory0.CheckReceivedCreateSharedWorker(
      kUrl0, kName, std::vector<network::mojom::ContentSecurityPolicyPtr>(),
      &worker_host0, &worker_receiver0));
  MockSharedWorker worker0(std::move(worker_receiver0));

  mojo::Remote<blink::mojom::SharedWorkerHost> worker_host1;
  mojo::PendingReceiver<blink::mojom::SharedWorker> worker_receiver1;
  EXPECT_TRUE(factory1.CheckReceivedCreateSharedWorker(
      kUrl1, kName, std::vector<network::mojom::ContentSecurityPolicyPtr>(),
      &worker_host1, &worker_receiver1));
  MockSharedWorker worker1(std::move(worker_receiver1));

  base::RunLoop().RunUntilIdle();

  // Check that the workers each received a connection.

  EXPECT_TRUE(worker0.CheckReceivedConnect(nullptr, nullptr));
  EXPECT_TRUE(worker0.CheckNotReceivedConnect());
  EXPECT_TRUE(client0.CheckReceivedOnCreated());

  EXPECT_TRUE(worker1.CheckReceivedConnect(nullptr, nullptr));
  EXPECT_TRUE(worker1.CheckNotReceivedConnect());
  EXPECT_TRUE(client1.CheckReceivedOnCreated());

  // Cleanup

  client0.Close();
  client1.Close();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(worker0.CheckReceivedTerminate());
  EXPECT_TRUE(worker1.CheckReceivedTerminate());
}

TEST_F(SharedWorkerServiceImplTest, CreateWorkerTest_PendingCase_NameMismatch) {
  const GURL kUrl("http://example.com/w.js");
  const char kName0[] = "name0";
  const char kName1[] = "name1";

  // The first renderer host.
  std::unique_ptr<TestWebContents> web_contents0 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host0 =
      web_contents0->GetPrimaryMainFrame();
  MockRenderProcessHost* renderer_host0 = render_frame_host0->GetProcess();
  const int process_id0 = renderer_host0->GetID();
  renderer_host0->OverrideBinderForTesting(
      blink::mojom::SharedWorkerFactory::Name_,
      base::BindRepeating(&SharedWorkerServiceImplTest::BindSharedWorkerFactory,
                          base::Unretained(this), process_id0));

  // The second renderer host.
  std::unique_ptr<TestWebContents> web_contents1 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host1 =
      web_contents1->GetPrimaryMainFrame();

  // First client and second client are created before the workers start.

  MockSharedWorkerClient client0;
  MessagePortChannel local_port0;
  ConnectToSharedWorker(
      MakeSharedWorkerConnector(render_frame_host0->GetGlobalId()), kUrl,
      kName0, &client0, &local_port0);

  MockSharedWorkerClient client1;
  MessagePortChannel local_port1;
  ConnectToSharedWorker(
      MakeSharedWorkerConnector(render_frame_host1->GetGlobalId()), kUrl,
      kName1, &client1, &local_port1);

  base::RunLoop().RunUntilIdle();

  // Check that both workers were created.

  mojo::PendingReceiver<blink::mojom::SharedWorkerFactory> factory_receiver0;
  std::tie(factory_receiver0, std::ignore) = WaitForFactoryReceiver();
  MockSharedWorkerFactory factory0(std::move(factory_receiver0));

  mojo::PendingReceiver<blink::mojom::SharedWorkerFactory> factory_receiver1;
  std::tie(factory_receiver1, std::ignore) = WaitForFactoryReceiver();
  MockSharedWorkerFactory factory1(std::move(factory_receiver1));

  base::RunLoop().RunUntilIdle();

  mojo::Remote<blink::mojom::SharedWorkerHost> worker_host0;
  mojo::PendingReceiver<blink::mojom::SharedWorker> worker_receiver0;
  EXPECT_TRUE(factory0.CheckReceivedCreateSharedWorker(
      kUrl, kName0, std::vector<network::mojom::ContentSecurityPolicyPtr>(),
      &worker_host0, &worker_receiver0));
  MockSharedWorker worker0(std::move(worker_receiver0));

  mojo::Remote<blink::mojom::SharedWorkerHost> worker_host1;
  mojo::PendingReceiver<blink::mojom::SharedWorker> worker_receiver1;
  EXPECT_TRUE(factory1.CheckReceivedCreateSharedWorker(
      kUrl, kName1, std::vector<network::mojom::ContentSecurityPolicyPtr>(),
      &worker_host1, &worker_receiver1));
  MockSharedWorker worker1(std::move(worker_receiver1));

  base::RunLoop().RunUntilIdle();

  // Check that the workers each received a connection.

  EXPECT_TRUE(worker0.CheckReceivedConnect(nullptr, nullptr));
  EXPECT_TRUE(worker0.CheckNotReceivedConnect());
  EXPECT_TRUE(client0.CheckReceivedOnCreated());

  EXPECT_TRUE(worker1.CheckReceivedConnect(nullptr, nullptr));
  EXPECT_TRUE(worker1.CheckNotReceivedConnect());
  EXPECT_TRUE(client1.CheckReceivedOnCreated());

  // Cleanup

  client0.Close();
  client1.Close();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(worker0.CheckReceivedTerminate());
  EXPECT_TRUE(worker1.CheckReceivedTerminate());
}

TEST_F(SharedWorkerServiceImplTest, CreateWorkerRaceTest) {
  const GURL kUrl("http://example.com/w.js");
  const char kName[] = "name";

  // Create three renderer hosts.

  std::unique_ptr<TestWebContents> web_contents0 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host0 =
      web_contents0->GetPrimaryMainFrame();

  std::unique_ptr<TestWebContents> web_contents1 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host1 =
      web_contents1->GetPrimaryMainFrame();

  std::unique_ptr<TestWebContents> web_contents2 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host2 =
      web_contents2->GetPrimaryMainFrame();

  MockSharedWorkerClient client0;
  MessagePortChannel local_port0;
  ConnectToSharedWorker(
      MakeSharedWorkerConnector(render_frame_host0->GetGlobalId()), kUrl, kName,
      &client0, &local_port0);

  base::RunLoop().RunUntilIdle();

  // Starts a worker.

  auto [factory_receiver0, worker_process_id0] = WaitForFactoryReceiver();
  MockSharedWorkerFactory factory0(std::move(factory_receiver0));

  base::RunLoop().RunUntilIdle();

  mojo::Remote<blink::mojom::SharedWorkerHost> worker_host0;
  mojo::PendingReceiver<blink::mojom::SharedWorker> worker_receiver0;
  EXPECT_TRUE(factory0.CheckReceivedCreateSharedWorker(
      kUrl, kName, std::vector<network::mojom::ContentSecurityPolicyPtr>(),
      &worker_host0, &worker_receiver0));
  MockSharedWorker worker0(std::move(worker_receiver0));

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(worker0.CheckReceivedConnect(nullptr, nullptr));
  EXPECT_TRUE(client0.CheckReceivedOnCreated());

  // Simulate unexpected disconnection that results in discarding worker0.
  worker0.Disconnect();
  base::RunLoop().RunUntilIdle();

  // Start a new client, attemping to connect to the same worker.
  MockSharedWorkerClient client1;
  MessagePortChannel local_port1;
  ConnectToSharedWorker(
      MakeSharedWorkerConnector(render_frame_host1->GetGlobalId()), kUrl, kName,
      &client1, &local_port1);

  base::RunLoop().RunUntilIdle();

  // The previous worker is unavailable, so a new worker is created.

  mojo::PendingReceiver<blink::mojom::SharedWorkerFactory> factory_receiver1;
  std::tie(factory_receiver1, std::ignore) = WaitForFactoryReceiver();
  MockSharedWorkerFactory factory1(std::move(factory_receiver1));

  base::RunLoop().RunUntilIdle();

  mojo::Remote<blink::mojom::SharedWorkerHost> worker_host1;
  mojo::PendingReceiver<blink::mojom::SharedWorker> worker_receiver1;
  EXPECT_TRUE(factory1.CheckReceivedCreateSharedWorker(
      kUrl, kName, std::vector<network::mojom::ContentSecurityPolicyPtr>(),
      &worker_host1, &worker_receiver1));
  MockSharedWorker worker1(std::move(worker_receiver1));

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(worker0.CheckNotReceivedConnect());
  EXPECT_TRUE(worker1.CheckReceivedConnect(nullptr, nullptr));
  EXPECT_TRUE(client1.CheckReceivedOnCreated());

  // Start another client to confirm that it can connect to the same worker.
  MockSharedWorkerClient client2;
  MessagePortChannel local_port2;
  ConnectToSharedWorker(
      MakeSharedWorkerConnector(render_frame_host2->GetGlobalId()), kUrl, kName,
      &client2, &local_port2);

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(factorys_receivers_.empty());

  EXPECT_TRUE(worker1.CheckReceivedConnect(nullptr, nullptr));
  EXPECT_TRUE(client2.CheckReceivedOnCreated());

  // Tear down the worker host.
  worker_host1->OnContextClosed();
  base::RunLoop().RunUntilIdle();
}

TEST_F(SharedWorkerServiceImplTest, CreateWorkerRaceTest2) {
  const GURL kUrl("http://example.com/w.js");
  const char kName[] = "name";

  // Create three renderer hosts.

  std::unique_ptr<TestWebContents> web_contents0 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host0 =
      web_contents0->GetPrimaryMainFrame();

  std::unique_ptr<TestWebContents> web_contents1 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host1 =
      web_contents1->GetPrimaryMainFrame();

  std::unique_ptr<TestWebContents> web_contents2 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host2 =
      web_contents2->GetPrimaryMainFrame();

  MockSharedWorkerClient client0;
  MessagePortChannel local_port0;
  ConnectToSharedWorker(
      MakeSharedWorkerConnector(render_frame_host0->GetGlobalId()), kUrl, kName,
      &client0, &local_port0);

  auto [factory_receiver0, worker_process_id0] = WaitForFactoryReceiver();
  MockSharedWorkerFactory factory0(std::move(factory_receiver0));

  // Simulate unexpected disconnection like a process crash.
  factory0.Disconnect();
  base::RunLoop().RunUntilIdle();

  // Start a new client, attempting to connect to the same worker.
  MockSharedWorkerClient client1;
  MessagePortChannel local_port1;
  ConnectToSharedWorker(
      MakeSharedWorkerConnector(render_frame_host1->GetGlobalId()), kUrl, kName,
      &client1, &local_port1);

  base::RunLoop().RunUntilIdle();

  // The previous worker is unavailable, so a new worker is created.

  mojo::PendingReceiver<blink::mojom::SharedWorkerFactory> factory_receiver1;
  std::tie(factory_receiver1, std::ignore) = WaitForFactoryReceiver();
  MockSharedWorkerFactory factory1(std::move(factory_receiver1));

  EXPECT_TRUE(factorys_receivers_.empty());

  base::RunLoop().RunUntilIdle();

  mojo::Remote<blink::mojom::SharedWorkerHost> worker_host1;
  mojo::PendingReceiver<blink::mojom::SharedWorker> worker_receiver1;
  EXPECT_TRUE(factory1.CheckReceivedCreateSharedWorker(
      kUrl, kName, std::vector<network::mojom::ContentSecurityPolicyPtr>(),
      &worker_host1, &worker_receiver1));
  MockSharedWorker worker1(std::move(worker_receiver1));

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(worker1.CheckReceivedConnect(nullptr, nullptr));
  EXPECT_TRUE(client1.CheckReceivedOnCreated());

  // Start another client to confirm that it can connect to the same worker.
  MockSharedWorkerClient client2;
  MessagePortChannel local_port2;
  ConnectToSharedWorker(
      MakeSharedWorkerConnector(render_frame_host2->GetGlobalId()), kUrl, kName,
      &client2, &local_port2);

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(factorys_receivers_.empty());

  EXPECT_TRUE(worker1.CheckReceivedConnect(nullptr, nullptr));
  EXPECT_TRUE(client2.CheckReceivedOnCreated());

  // Tear down the worker host.
  worker_host1->OnContextClosed();
  base::RunLoop().RunUntilIdle();
}

TEST_F(SharedWorkerServiceImplTest, CreateWorkerRaceTest3) {
  const GURL kURL("http://example.com/w.js");
  const char kName[] = "name";

  // The first renderer host.
  std::unique_ptr<TestWebContents> web_contents0 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host0 =
      web_contents0->GetPrimaryMainFrame();
  MockRenderProcessHost* renderer_host0 = render_frame_host0->GetProcess();
  const int process_id0 = renderer_host0->GetID();
  renderer_host0->OverrideBinderForTesting(
      blink::mojom::SharedWorkerFactory::Name_,
      base::BindRepeating(&SharedWorkerServiceImplTest::BindSharedWorkerFactory,
                          base::Unretained(this), process_id0));

  // The second renderer host.
  std::unique_ptr<TestWebContents> web_contents1 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host1 =
      web_contents1->GetPrimaryMainFrame();

  // Both clients try to connect/create a worker.

  MockSharedWorkerClient client0;
  MessagePortChannel local_port0;
  ConnectToSharedWorker(
      MakeSharedWorkerConnector(render_frame_host0->GetGlobalId()), kURL, kName,
      &client0, &local_port0);

  MockSharedWorkerClient client1;
  MessagePortChannel local_port1;
  ConnectToSharedWorker(
      MakeSharedWorkerConnector(render_frame_host1->GetGlobalId()), kURL, kName,
      &client1, &local_port1);
  base::RunLoop().RunUntilIdle();

  // Expect a factory receiver. It can come from either process.
  mojo::PendingReceiver<blink::mojom::SharedWorkerFactory> factory_receiver;
  std::tie(factory_receiver, std::ignore) = WaitForFactoryReceiver();
  MockSharedWorkerFactory factory(std::move(factory_receiver));
  base::RunLoop().RunUntilIdle();

  // Expect a create shared worker.
  mojo::Remote<blink::mojom::SharedWorkerHost> worker_host;
  mojo::PendingReceiver<blink::mojom::SharedWorker> worker_receiver;
  EXPECT_TRUE(factory.CheckReceivedCreateSharedWorker(
      kURL, kName, std::vector<network::mojom::ContentSecurityPolicyPtr>(),
      &worker_host, &worker_receiver));
  MockSharedWorker worker(std::move(worker_receiver));
  base::RunLoop().RunUntilIdle();

  // Expect one connect for the first client.
  EXPECT_TRUE(worker.CheckReceivedConnect(nullptr, nullptr));
  client0.CheckReceivedOnCreated();

  // Expect one connect for the second client.
  EXPECT_TRUE(worker.CheckReceivedConnect(nullptr, nullptr));
  client1.CheckReceivedOnCreated();

  // Cleanup

  client0.Close();
  client1.Close();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(worker.CheckReceivedTerminate());
}

class TestSharedWorkerServiceObserver : public SharedWorkerService::Observer {
 public:
  TestSharedWorkerServiceObserver() = default;

  TestSharedWorkerServiceObserver(const TestSharedWorkerServiceObserver&) =
      delete;
  TestSharedWorkerServiceObserver& operator=(
      const TestSharedWorkerServiceObserver&) = delete;

  ~TestSharedWorkerServiceObserver() override = default;

  // SharedWorkerService::Observer:
  void OnWorkerCreated(const blink::SharedWorkerToken& shared_worker_token,
                       int worker_process_id,
                       const url::Origin& security_origin,
                       const base::UnguessableToken& dev_tools_token) override {
    EXPECT_TRUE(shared_workers_.insert({shared_worker_token, {}}).second);
    shared_worker_origins_.insert(security_origin);
  }
  void OnBeforeWorkerDestroyed(
      const blink::SharedWorkerToken& shared_worker_token) override {
    auto it = shared_workers_.find(shared_worker_token);
    EXPECT_TRUE(it != shared_workers_.end());
    EXPECT_EQ(0u, it->second.size());
    shared_workers_.erase(it);
  }
  void OnFinalResponseURLDetermined(
      const blink::SharedWorkerToken& shared_worker_token,
      const GURL& url) override {}
  void OnClientAdded(
      const blink::SharedWorkerToken& shared_worker_token,
      GlobalRenderFrameHostId client_render_frame_host_id) override {
    auto it = shared_workers_.find(shared_worker_token);
    EXPECT_TRUE(it != shared_workers_.end());
    std::set<GlobalRenderFrameHostId>& clients = it->second;
    EXPECT_TRUE(clients.insert(client_render_frame_host_id).second);
  }
  void OnClientRemoved(
      const blink::SharedWorkerToken& shared_worker_token,
      GlobalRenderFrameHostId client_render_frame_host_id) override {
    auto it = shared_workers_.find(shared_worker_token);
    EXPECT_TRUE(it != shared_workers_.end());
    std::set<GlobalRenderFrameHostId>& clients = it->second;
    EXPECT_EQ(1u, clients.erase(client_render_frame_host_id));
  }

  size_t GetWorkerCount() { return shared_workers_.size(); }

  size_t GetClientCount() {
    size_t client_count = 0;
    for (const auto& worker : shared_workers_)
      client_count += worker.second.size();
    return client_count;
  }

  const base::flat_set<url::Origin>& GetWorkerOrigins() const {
    return shared_worker_origins_;
  }

 private:
  base::flat_map<blink::SharedWorkerToken, std::set<GlobalRenderFrameHostId>>
      shared_workers_;
  base::flat_set<url::Origin> shared_worker_origins_;
};

TEST_F(SharedWorkerServiceImplTest, Observer) {
  TestSharedWorkerServiceObserver observer;

  base::ScopedObservation<SharedWorkerService, SharedWorkerService::Observer>
      scoped_observation(&observer);
  scoped_observation.Observe(
      browser_context_->GetDefaultStoragePartition()->GetSharedWorkerService());

  std::unique_ptr<TestWebContents> web_contents =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host = web_contents->GetPrimaryMainFrame();
  MockRenderProcessHost* renderer_host = render_frame_host->GetProcess();
  const int process_id = renderer_host->GetID();
  renderer_host->OverrideBinderForTesting(
      blink::mojom::SharedWorkerFactory::Name_,
      base::BindRepeating(&SharedWorkerServiceImplTest::BindSharedWorkerFactory,
                          base::Unretained(this), process_id));

  MockSharedWorkerClient client;
  MessagePortChannel local_port;
  const GURL kUrl("http://example.com/w.js");
  ConnectToSharedWorker(
      MakeSharedWorkerConnector(render_frame_host->GetGlobalId()), kUrl, "name",
      &client, &local_port);

  mojo::PendingReceiver<blink::mojom::SharedWorkerFactory> factory_receiver;
  std::tie(factory_receiver, std::ignore) = WaitForFactoryReceiver();
  MockSharedWorkerFactory factory(std::move(factory_receiver));
  base::RunLoop().RunUntilIdle();

  mojo::Remote<blink::mojom::SharedWorkerHost> worker_host;
  mojo::PendingReceiver<blink::mojom::SharedWorker> worker_receiver;
  EXPECT_TRUE(factory.CheckReceivedCreateSharedWorker(
      kUrl, "name", std::vector<network::mojom::ContentSecurityPolicyPtr>(),
      &worker_host, &worker_receiver));
  MockSharedWorker worker(std::move(worker_receiver));
  base::RunLoop().RunUntilIdle();

  int connection_request_id;
  MessagePortChannel port;
  EXPECT_TRUE(worker.CheckReceivedConnect(&connection_request_id, &port));

  EXPECT_TRUE(client.CheckReceivedOnCreated());

  EXPECT_EQ(1u, observer.GetWorkerCount());
  EXPECT_EQ(1u, observer.GetClientCount());
  EXPECT_THAT(observer.GetWorkerOrigins(),
              ElementsAre(url::Origin::Create(kUrl)));

  // Tear down the worker host.
  worker_host->OnContextClosed();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0u, observer.GetWorkerCount());
  EXPECT_EQ(0u, observer.GetClientCount());
}

TEST_F(SharedWorkerServiceImplTest, EnumerateSharedWorkers) {
  TestSharedWorkerServiceObserver observer;

  std::unique_ptr<TestWebContents> web_contents =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host = web_contents->GetPrimaryMainFrame();

  MockSharedWorkerClient client;
  MessagePortChannel local_port;
  const GURL kUrl("http://example.com/w.js");
  ConnectToSharedWorker(
      MakeSharedWorkerConnector(render_frame_host->GetGlobalId()), kUrl, "name",
      &client, &local_port);

  mojo::PendingReceiver<blink::mojom::SharedWorkerFactory> factory_receiver;
  std::tie(factory_receiver, std::ignore) = WaitForFactoryReceiver();
  MockSharedWorkerFactory factory(std::move(factory_receiver));
  base::RunLoop().RunUntilIdle();

  mojo::Remote<blink::mojom::SharedWorkerHost> worker_host;
  mojo::PendingReceiver<blink::mojom::SharedWorker> worker_receiver;
  EXPECT_TRUE(factory.CheckReceivedCreateSharedWorker(
      kUrl, "name", std::vector<network::mojom::ContentSecurityPolicyPtr>(),
      &worker_host, &worker_receiver));
  MockSharedWorker worker(std::move(worker_receiver));
  base::RunLoop().RunUntilIdle();

  int connection_request_id;
  MessagePortChannel port;
  EXPECT_TRUE(worker.CheckReceivedConnect(&connection_request_id, &port));

  EXPECT_TRUE(client.CheckReceivedOnCreated());

  // The observer was never registered to the SharedWorkerService.
  EXPECT_EQ(0u, observer.GetWorkerCount());

  // Retrieve running shared workers.
  browser_context_->GetDefaultStoragePartition()
      ->GetSharedWorkerService()
      ->EnumerateSharedWorkers(&observer);

  EXPECT_EQ(1u, observer.GetWorkerCount());
  EXPECT_THAT(observer.GetWorkerOrigins(),
              ElementsAre(url::Origin::Create(kUrl)));

  // Cleanup.
  worker_host->OnContextClosed();
  base::RunLoop().RunUntilIdle();
}

TEST_F(SharedWorkerServiceImplTest, CollapseDuplicateNotifications) {
  TestSharedWorkerServiceObserver observer;

  base::ScopedObservation<SharedWorkerService, SharedWorkerService::Observer>
      scoped_observation(&observer);
  scoped_observation.Observe(
      browser_context_->GetDefaultStoragePartition()->GetSharedWorkerService());

  const GURL kUrl("http://example.com/w.js");
  const char kName[] = "name";

  // The first renderer host.
  std::unique_ptr<TestWebContents> web_contents =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host = web_contents->GetPrimaryMainFrame();

  // First client, creates worker.

  MockSharedWorkerClient client0;
  MessagePortChannel local_port0;
  ConnectToSharedWorker(
      MakeSharedWorkerConnector(render_frame_host->GetGlobalId()), kUrl, kName,
      &client0, &local_port0);
  base::RunLoop().RunUntilIdle();

  mojo::PendingReceiver<blink::mojom::SharedWorkerFactory> factory_receiver;
  std::tie(factory_receiver, std::ignore) = WaitForFactoryReceiver();
  MockSharedWorkerFactory factory(std::move(factory_receiver));
  base::RunLoop().RunUntilIdle();

  mojo::Remote<blink::mojom::SharedWorkerHost> worker_host;
  mojo::PendingReceiver<blink::mojom::SharedWorker> worker_receiver;
  EXPECT_TRUE(factory.CheckReceivedCreateSharedWorker(
      kUrl, kName, std::vector<network::mojom::ContentSecurityPolicyPtr>(),
      &worker_host, &worker_receiver));
  MockSharedWorker worker(std::move(worker_receiver));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(worker.CheckReceivedConnect(nullptr, nullptr));
  EXPECT_TRUE(client0.CheckReceivedOnCreated());

  // The observer now sees one worker with one client.
  EXPECT_EQ(1u, observer.GetWorkerCount());
  EXPECT_EQ(1u, observer.GetClientCount());

  // Now the same frame connects to the same worker.
  MockSharedWorkerClient client1;
  MessagePortChannel local_port1;
  ConnectToSharedWorker(
      MakeSharedWorkerConnector(render_frame_host->GetGlobalId()), kUrl, kName,
      &client1, &local_port1);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(factorys_receivers_.empty());

  EXPECT_TRUE(worker.CheckReceivedConnect(nullptr, nullptr));
  EXPECT_TRUE(client1.CheckReceivedOnCreated());

  // Duplicate notification for the same client/worker pair are not sent.
  EXPECT_EQ(1u, observer.GetWorkerCount());
  EXPECT_EQ(1u, observer.GetClientCount());

  // Cleanup

  client0.Close();
  base::RunLoop().RunUntilIdle();

  // With the first connection closed, the observer is still aware of one
  // client.
  EXPECT_EQ(1u, observer.GetWorkerCount());
  EXPECT_EQ(1u, observer.GetClientCount());

  client1.Close();
  base::RunLoop().RunUntilIdle();

  // Both connection are closed, the worker is stopped and there's no active
  // clients.
  EXPECT_EQ(0u, observer.GetWorkerCount());
  EXPECT_EQ(0u, observer.GetClientCount());

  EXPECT_TRUE(worker.CheckReceivedTerminate());
}

// This test ensures that OnClientRemoved is still invoked if the connection
// with the client was lost.
TEST_F(SharedWorkerServiceImplTest, Observer_OnClientConnectionLost) {
  TestSharedWorkerServiceObserver observer;

  base::ScopedObservation<SharedWorkerService, SharedWorkerService::Observer>
      scoped_observation(&observer);
  scoped_observation.Observe(
      browser_context_->GetDefaultStoragePartition()->GetSharedWorkerService());

  std::unique_ptr<TestWebContents> web_contents =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host = web_contents->GetPrimaryMainFrame();

  MockSharedWorkerClient client;
  MessagePortChannel local_port;
  const GURL kUrl("http://example.com/w.js");
  ConnectToSharedWorker(
      MakeSharedWorkerConnector(render_frame_host->GetGlobalId()), kUrl, "name",
      &client, &local_port);

  mojo::PendingReceiver<blink::mojom::SharedWorkerFactory> factory_receiver;
  std::tie(factory_receiver, std::ignore) = WaitForFactoryReceiver();
  MockSharedWorkerFactory factory(std::move(factory_receiver));
  base::RunLoop().RunUntilIdle();

  mojo::Remote<blink::mojom::SharedWorkerHost> worker_host;
  mojo::PendingReceiver<blink::mojom::SharedWorker> worker_receiver;
  EXPECT_TRUE(factory.CheckReceivedCreateSharedWorker(
      kUrl, "name", std::vector<network::mojom::ContentSecurityPolicyPtr>(),
      &worker_host, &worker_receiver));
  MockSharedWorker worker(std::move(worker_receiver));
  base::RunLoop().RunUntilIdle();

  int connection_request_id;
  MessagePortChannel port;
  EXPECT_TRUE(worker.CheckReceivedConnect(&connection_request_id, &port));

  EXPECT_TRUE(client.CheckReceivedOnCreated());

  EXPECT_EQ(1u, observer.GetWorkerCount());
  EXPECT_EQ(1u, observer.GetClientCount());

  // Simulate losing the client's connection.
  client.ResetReceiver();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0u, observer.GetWorkerCount());
  EXPECT_EQ(0u, observer.GetClientCount());
}

}  // namespace content
