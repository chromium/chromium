// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/worker_host/shared_worker_service_impl.h"

#include <memory>
#include <set>
#include <string>

#include "base/bind.h"
#include "base/containers/flat_map.h"
#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/worker_host/mock_shared_worker.h"
#include "content/browser/worker_host/shared_worker_connector_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_utils.h"
#include "content/test/fake_network_url_loader_factory.h"
#include "content/test/not_implemented_network_url_loader_factory.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/messaging/message_port_channel.h"
#include "third_party/blink/public/mojom/worker/shared_worker_info.mojom.h"

using blink::MessagePortChannel;

namespace content {

namespace {

void ConnectToSharedWorker(
    mojo::Remote<blink::mojom::SharedWorkerConnector> connector,
    const GURL& url,
    const std::string& name,
    MockSharedWorkerClient* client,
    MessagePortChannel* local_port) {
  blink::mojom::SharedWorkerInfoPtr info(blink::mojom::SharedWorkerInfo::New(
      url, name, std::string(),
      network::mojom::ContentSecurityPolicyType::kReport,
      network::mojom::IPAddressSpace::kPublic));

  mojo::MessagePipe message_pipe;
  *local_port = MessagePortChannel(std::move(message_pipe.handle0));

  mojo::PendingRemote<blink::mojom::SharedWorkerClient> client_proxy;
  client->Bind(client_proxy.InitWithNewPipeAndPassReceiver());

  connector->Connect(std::move(info),
                     blink::mojom::FetchClientSettingsObject::New(),
                     std::move(client_proxy),
                     blink::mojom::SharedWorkerCreationContextType::kSecure,
                     std::move(message_pipe.handle1), mojo::NullRemote());
}

// Helper to delete the given WebContents and shut down its process. This is
// useful because if FastShutdownIfPossible() is called without deleting the
// WebContents first, shutdown does not actually start.
void KillProcess(std::unique_ptr<WebContents> web_contents) {
  RenderFrameHost* frame_host = web_contents->GetMainFrame();
  RenderProcessHost* process_host = frame_host->GetProcess();
  web_contents.reset();
  process_host->FastShutdownIfPossible(/*page_count=*/0,
                                       /*skip_unload_handlers=*/true);
  ASSERT_TRUE(process_host->FastShutdownStarted());
}

}  // namespace

class SharedWorkerServiceImplTest : public RenderViewHostImplTestHarness {
 public:
  mojo::Remote<blink::mojom::SharedWorkerConnector> MakeSharedWorkerConnector(
      RenderProcessHost* process_host,
      int frame_id) {
    mojo::Remote<blink::mojom::SharedWorkerConnector> connector;
    SharedWorkerConnectorImpl::Create(process_host->GetID(), frame_id,
                                      connector.BindNewPipeAndPassReceiver());
    return connector;
  }

  // Waits until a mojo::PendingReceiver<blink::mojom::SharedWorkerFactory>
  // from the given process is received. kInvalidUniqueID means any process.
  mojo::PendingReceiver<blink::mojom::SharedWorkerFactory>
  WaitForFactoryReceiver(int process_id) {
    if (CheckNotReceivedFactoryReceiver(process_id)) {
      base::RunLoop run_loop;
      factory_receiver_callback_ = run_loop.QuitClosure();
      factory_receiver_callback_process_id_ = process_id;
      run_loop.Run();
    }
    auto iter = (process_id == ChildProcessHost::kInvalidUniqueID)
                    ? factorys_receivers_.begin()
                    : factorys_receivers_.find(process_id);
    DCHECK(iter != factorys_receivers_.end());
    auto& queue = iter->second;
    DCHECK(!queue.empty());
    auto rv = std::move(queue.front());
    queue.pop();
    if (queue.empty())
      factorys_receivers_.erase(iter);
    return rv;
  }

  bool CheckNotReceivedFactoryReceiver(int process_id) {
    if (process_id == ChildProcessHost::kInvalidUniqueID)
      return factorys_receivers_.empty();
    return !base::Contains(factorys_receivers_, process_id);
  }

  // Receives a PendingReceiver<blink::mojom::SharedWorkerFactory>.
  void BindSharedWorkerFactory(int process_id,
                               mojo::ScopedMessagePipeHandle handle) {
    if (factory_receiver_callback_ &&
        (factory_receiver_callback_process_id_ == process_id ||
         factory_receiver_callback_process_id_ ==
             ChildProcessHost::kInvalidUniqueID)) {
      factory_receiver_callback_process_id_ =
          ChildProcessHost::kInvalidUniqueID;
      std::move(factory_receiver_callback_).Run();
    }

    if (!base::Contains(factorys_receivers_, process_id)) {
      factorys_receivers_.emplace(
          process_id,
          base::queue<
              mojo::PendingReceiver<blink::mojom::SharedWorkerFactory>>());
    }
    factorys_receivers_[process_id].emplace(std::move(handle));
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
        std::make_unique<MockRenderProcessHostFactory>();
    RenderProcessHostImpl::set_render_process_host_factory_for_testing(
        render_process_host_factory_.get());

    fake_url_loader_factory_ = std::make_unique<FakeNetworkURLLoaderFactory>();
    url_loader_factory_wrapper_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            fake_url_loader_factory_.get());
    static_cast<SharedWorkerServiceImpl*>(
        BrowserContext::GetDefaultStoragePartition(browser_context_.get())
            ->GetSharedWorkerService())
        ->SetURLLoaderFactoryForTesting(url_loader_factory_wrapper_);
  }

  void TearDown() override {
    if (url_loader_factory_wrapper_)
      url_loader_factory_wrapper_->Detach();
    browser_context_.reset();
    RenderViewHostImplTestHarness::TearDown();
  }

  std::unique_ptr<TestBrowserContext> browser_context_;

  // Holds pending receivers of SharedWorkerFactory for each process.
  base::flat_map<
      int /* process_id */,
      base::queue<mojo::PendingReceiver<blink::mojom::SharedWorkerFactory>>>
      factorys_receivers_;

  // The callback is called when a pending receiver of SharedWorkerFactory for
  // the specified process is received. kInvalidUniqueID means any process.
  base::OnceClosure factory_receiver_callback_;
  int factory_receiver_callback_process_id_ =
      ChildProcessHost::kInvalidUniqueID;

  std::unique_ptr<MockRenderProcessHostFactory> render_process_host_factory_;

  std::unique_ptr<FakeNetworkURLLoaderFactory> fake_url_loader_factory_;
  scoped_refptr<network::WeakWrapperSharedURLLoaderFactory>
      url_loader_factory_wrapper_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SharedWorkerServiceImplTest);
};

TEST_F(SharedWorkerServiceImplTest, BasicTest) {
  std::unique_ptr<TestWebContents> web_contents =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host = web_contents->GetMainFrame();
  MockRenderProcessHost* renderer_host = render_frame_host->GetProcess();
  const int process_id = renderer_host->GetID();
  renderer_host->OverrideBinderForTesting(
      blink::mojom::SharedWorkerFactory::Name_,
      base::BindRepeating(&SharedWorkerServiceImplTest::BindSharedWorkerFactory,
                          base::Unretained(this), process_id));

  MockSharedWorkerClient client;
  MessagePortChannel local_port;
  const GURL kUrl("http://example.com/w.js");
  ConnectToSharedWorker(MakeSharedWorkerConnector(
                            renderer_host, render_frame_host->GetRoutingID()),
                        kUrl, "name", &client, &local_port);

  mojo::PendingReceiver<blink::mojom::SharedWorkerFactory> factory_receiver =
      WaitForFactoryReceiver(process_id);
  MockSharedWorkerFactory factory(std::move(factory_receiver));
  base::RunLoop().RunUntilIdle();

  mojo::Remote<blink::mojom::SharedWorkerHost> worker_host;
  mojo::PendingReceiver<blink::mojom::SharedWorker> worker_receiver;
  EXPECT_TRUE(factory.CheckReceivedCreateSharedWorker(
      kUrl, "name", network::mojom::ContentSecurityPolicyType::kReport,
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

  EXPECT_TRUE(
      client.CheckReceivedOnConnected(std::set<blink::mojom::WebFeature>()));

  // Verify that |port| corresponds to |connector->local_port()|.
  std::string expected_message("test1");
  EXPECT_TRUE(mojo::test::WriteTextMessage(local_port.GetHandle().get(),
                                           expected_message));
  std::string received_message;
  EXPECT_TRUE(
      mojo::test::ReadTextMessage(port.GetHandle().get(), &received_message));
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
  TestRenderFrameHost* render_frame_host = web_contents->GetMainFrame();
  MockRenderProcessHost* renderer_host = render_frame_host->GetProcess();
  const int process_id = renderer_host->GetID();
  renderer_host->OverrideBinderForTesting(
      blink::mojom::SharedWorkerFactory::Name_,
      base::BindRepeating(&SharedWorkerServiceImplTest::BindSharedWorkerFactory,
                          base::Unretained(this), process_id));

  MockSharedWorkerClient client;
  MessagePortChannel local_port;
  const GURL kUrl("http://example.com/w.js");
  ConnectToSharedWorker(MakeSharedWorkerConnector(
                            renderer_host, render_frame_host->GetRoutingID()),
                        kUrl, "name", &client, &local_port);

  // Now asynchronously destroy |web_contents| so that the startup sequence at
  // least reaches SharedWorkerServiceImpl::DidCreateScriptLoader().
  // reaches at least the DidCreateScriptLoader()
  base::SequencedTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE,
                                                     std::move(web_contents));

  base::RunLoop().RunUntilIdle();

  // The shared worker creation request was dropped.
  EXPECT_TRUE(!client.CheckReceivedOnCreated());
}

TEST_F(SharedWorkerServiceImplTest, TwoRendererTest) {
  // The first renderer host.
  std::unique_ptr<TestWebContents> web_contents0 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host0 = web_contents0->GetMainFrame();
  MockRenderProcessHost* renderer_host0 = render_frame_host0->GetProcess();
  const int process_id0 = renderer_host0->GetID();
  renderer_host0->OverrideBinderForTesting(
      blink::mojom::SharedWorkerFactory::Name_,
      base::BindRepeating(&SharedWorkerServiceImplTest::BindSharedWorkerFactory,
                          base::Unretained(this), process_id0));

  MockSharedWorkerClient client0;
  MessagePortChannel local_port0;
  const GURL kUrl("http://example.com/w.js");
  ConnectToSharedWorker(MakeSharedWorkerConnector(
                            renderer_host0, render_frame_host0->GetRoutingID()),
                        kUrl, "name", &client0, &local_port0);

  base::RunLoop().RunUntilIdle();

  mojo::PendingReceiver<blink::mojom::SharedWorkerFactory> factory_receiver =
      WaitForFactoryReceiver(process_id0);
  MockSharedWorkerFactory factory(std::move(factory_receiver));
  base::RunLoop().RunUntilIdle();

  mojo::Remote<blink::mojom::SharedWorkerHost> worker_host;
  mojo::PendingReceiver<blink::mojom::SharedWorker> worker_receiver;
  EXPECT_TRUE(factory.CheckReceivedCreateSharedWorker(
      kUrl, "name", network::mojom::ContentSecurityPolicyType::kReport,
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

  EXPECT_TRUE(
      client0.CheckReceivedOnConnected(std::set<blink::mojom::WebFeature>()));

  // Verify that |port0| corresponds to |connector0->local_port()|.
  std::string expected_message0("test1");
  EXPECT_TRUE(mojo::test::WriteTextMessage(local_port0.GetHandle().get(),
                                           expected_message0));
  std::string received_message0;
  EXPECT_TRUE(
      mojo::test::ReadTextMessage(port0.GetHandle().get(), &received_message0));
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
  EXPECT_EQ(1u, renderer_host0->GetKeepAliveRefCount());

  // The second renderer host.
  std::unique_ptr<TestWebContents> web_contents1 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host1 = web_contents1->GetMainFrame();
  MockRenderProcessHost* renderer_host1 = render_frame_host1->GetProcess();
  const int process_id1 = renderer_host1->GetID();
  renderer_host1->OverrideBinderForTesting(
      blink::mojom::SharedWorkerFactory::Name_,
      base::BindRepeating(&SharedWorkerServiceImplTest::BindSharedWorkerFactory,
                          base::Unretained(this), process_id1));

  MockSharedWorkerClient client1;
  MessagePortChannel local_port1;
  ConnectToSharedWorker(MakeSharedWorkerConnector(
                            renderer_host1, render_frame_host1->GetRoutingID()),
                        kUrl, "name", &client1, &local_port1);

  base::RunLoop().RunUntilIdle();

  // Should not have tried to create a new shared worker.
  EXPECT_TRUE(CheckNotReceivedFactoryReceiver(process_id1));

  int connection_request_id1;
  MessagePortChannel port1;
  EXPECT_TRUE(worker.CheckReceivedConnect(&connection_request_id1, &port1));

  EXPECT_TRUE(client1.CheckReceivedOnCreated());

  // Only a single worker instance in process 0.
  EXPECT_EQ(1u, renderer_host0->GetKeepAliveRefCount());
  EXPECT_EQ(0u, renderer_host1->GetKeepAliveRefCount());

  worker_host->OnConnected(connection_request_id1);

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(client1.CheckReceivedOnConnected({feature1, feature2}));

  // Verify that |worker_msg_port2| corresponds to |connector1->local_port()|.
  std::string expected_message1("test2");
  EXPECT_TRUE(mojo::test::WriteTextMessage(local_port1.GetHandle().get(),
                                           expected_message1));
  std::string received_message1;
  EXPECT_TRUE(
      mojo::test::ReadTextMessage(port1.GetHandle().get(), &received_message1));
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
  TestRenderFrameHost* render_frame_host0 = web_contents0->GetMainFrame();
  MockRenderProcessHost* renderer_host0 = render_frame_host0->GetProcess();
  const int process_id0 = renderer_host0->GetID();
  renderer_host0->OverrideBinderForTesting(
      blink::mojom::SharedWorkerFactory::Name_,
      base::BindRepeating(&SharedWorkerServiceImplTest::BindSharedWorkerFactory,
                          base::Unretained(this), process_id0));

  // The second renderer host.
  std::unique_ptr<TestWebContents> web_contents1 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host1 = web_contents1->GetMainFrame();
  MockRenderProcessHost* renderer_host1 = render_frame_host1->GetProcess();
  const int process_id1 = renderer_host1->GetID();
  renderer_host1->OverrideBinderForTesting(
      blink::mojom::SharedWorkerFactory::Name_,
      base::BindRepeating(&SharedWorkerServiceImplTest::BindSharedWorkerFactory,
                          base::Unretained(this), process_id1));

  // First client, creates worker.

  MockSharedWorkerClient client0;
  MessagePortChannel local_port0;
  ConnectToSharedWorker(MakeSharedWorkerConnector(
                            renderer_host0, render_frame_host0->GetRoutingID()),
                        kUrl, kName, &client0, &local_port0);
  base::RunLoop().RunUntilIdle();

  mojo::PendingReceiver<blink::mojom::SharedWorkerFactory> factory_receiver =
      WaitForFactoryReceiver(process_id0);
  MockSharedWorkerFactory factory(std::move(factory_receiver));
  base::RunLoop().RunUntilIdle();

  mojo::Remote<blink::mojom::SharedWorkerHost> worker_host;
  mojo::PendingReceiver<blink::mojom::SharedWorker> worker_receiver;
  EXPECT_TRUE(factory.CheckReceivedCreateSharedWorker(
      kUrl, kName, network::mojom::ContentSecurityPolicyType::kReport,
      &worker_host, &worker_receiver));
  MockSharedWorker worker(std::move(worker_receiver));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(worker.CheckReceivedConnect(nullptr, nullptr));
  EXPECT_TRUE(client0.CheckReceivedOnCreated());

  // Second client, same worker.

  MockSharedWorkerClient client1;
  MessagePortChannel local_port1;
  ConnectToSharedWorker(MakeSharedWorkerConnector(
                            renderer_host1, render_frame_host1->GetRoutingID()),
                        kUrl, kName, &client1, &local_port1);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(CheckNotReceivedFactoryReceiver(process_id1));

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
  TestRenderFrameHost* render_frame_host0 = web_contents0->GetMainFrame();
  MockRenderProcessHost* renderer_host0 = render_frame_host0->GetProcess();
  const int process_id0 = renderer_host0->GetID();
  renderer_host0->OverrideBinderForTesting(
      blink::mojom::SharedWorkerFactory::Name_,
      base::BindRepeating(&SharedWorkerServiceImplTest::BindSharedWorkerFactory,
                          base::Unretained(this), process_id0));

  // The second renderer host.
  std::unique_ptr<TestWebContents> web_contents1 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host1 = web_contents1->GetMainFrame();
  MockRenderProcessHost* renderer_host1 = render_frame_host1->GetProcess();
  const int process_id1 = renderer_host1->GetID();
  renderer_host1->OverrideBinderForTesting(
      blink::mojom::SharedWorkerFactory::Name_,
      base::BindRepeating(&SharedWorkerServiceImplTest::BindSharedWorkerFactory,
                          base::Unretained(this), process_id1));

  // First client, creates worker.

  MockSharedWorkerClient client0;
  MessagePortChannel local_port0;
  ConnectToSharedWorker(MakeSharedWorkerConnector(
                            renderer_host0, render_frame_host0->GetRoutingID()),
                        kUrl0, kName, &client0, &local_port0);
  base::RunLoop().RunUntilIdle();

  mojo::PendingReceiver<blink::mojom::SharedWorkerFactory> factory_receiver0 =
      WaitForFactoryReceiver(process_id0);
  MockSharedWorkerFactory factory0(std::move(factory_receiver0));
  base::RunLoop().RunUntilIdle();

  mojo::Remote<blink::mojom::SharedWorkerHost> worker_host0;
  mojo::PendingReceiver<blink::mojom::SharedWorker> worker_receiver0;
  EXPECT_TRUE(factory0.CheckReceivedCreateSharedWorker(
      kUrl0, kName, network::mojom::ContentSecurityPolicyType::kReport,
      &worker_host0, &worker_receiver0));
  MockSharedWorker worker0(std::move(worker_receiver0));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(worker0.CheckReceivedConnect(nullptr, nullptr));
  EXPECT_TRUE(client0.CheckReceivedOnCreated());

  // Second client, creates worker.

  MockSharedWorkerClient client1;
  MessagePortChannel local_port1;
  ConnectToSharedWorker(MakeSharedWorkerConnector(
                            renderer_host1, render_frame_host1->GetRoutingID()),
                        kUrl1, kName, &client1, &local_port1);
  base::RunLoop().RunUntilIdle();

  mojo::PendingReceiver<blink::mojom::SharedWorkerFactory> factory_receiver1 =
      WaitForFactoryReceiver(process_id1);
  MockSharedWorkerFactory factory1(std::move(factory_receiver1));
  base::RunLoop().RunUntilIdle();

  mojo::Remote<blink::mojom::SharedWorkerHost> worker_host1;
  mojo::PendingReceiver<blink::mojom::SharedWorker> worker_receiver1;
  EXPECT_TRUE(factory1.CheckReceivedCreateSharedWorker(
      kUrl1, kName, network::mojom::ContentSecurityPolicyType::kReport,
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
  TestRenderFrameHost* render_frame_host0 = web_contents0->GetMainFrame();
  MockRenderProcessHost* renderer_host0 = render_frame_host0->GetProcess();
  const int process_id0 = renderer_host0->GetID();
  renderer_host0->OverrideBinderForTesting(
      blink::mojom::SharedWorkerFactory::Name_,
      base::BindRepeating(&SharedWorkerServiceImplTest::BindSharedWorkerFactory,
                          base::Unretained(this), process_id0));

  // The second renderer host.
  std::unique_ptr<TestWebContents> web_contents1 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host1 = web_contents1->GetMainFrame();
  MockRenderProcessHost* renderer_host1 = render_frame_host1->GetProcess();
  const int process_id1 = renderer_host1->GetID();
  renderer_host1->OverrideBinderForTesting(
      blink::mojom::SharedWorkerFactory::Name_,
      base::BindRepeating(&SharedWorkerServiceImplTest::BindSharedWorkerFactory,
                          base::Unretained(this), process_id1));

  // First client, creates worker.

  MockSharedWorkerClient client0;
  MessagePortChannel local_port0;
  ConnectToSharedWorker(MakeSharedWorkerConnector(
                            renderer_host0, render_frame_host0->GetRoutingID()),
                        kUrl, kName0, &client0, &local_port0);
  base::RunLoop().RunUntilIdle();

  mojo::PendingReceiver<blink::mojom::SharedWorkerFactory> factory_receiver0 =
      WaitForFactoryReceiver(process_id0);
  MockSharedWorkerFactory factory0(std::move(factory_receiver0));
  base::RunLoop().RunUntilIdle();

  mojo::Remote<blink::mojom::SharedWorkerHost> worker_host0;
  mojo::PendingReceiver<blink::mojom::SharedWorker> worker_receiver0;
  EXPECT_TRUE(factory0.CheckReceivedCreateSharedWorker(
      kUrl, kName0, network::mojom::ContentSecurityPolicyType::kReport,
      &worker_host0, &worker_receiver0));
  MockSharedWorker worker0(std::move(worker_receiver0));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(worker0.CheckReceivedConnect(nullptr, nullptr));
  EXPECT_TRUE(client0.CheckReceivedOnCreated());

  // Second client, creates worker.

  MockSharedWorkerClient client1;
  MessagePortChannel local_port1;
  ConnectToSharedWorker(MakeSharedWorkerConnector(
                            renderer_host1, render_frame_host1->GetRoutingID()),
                        kUrl, kName1, &client1, &local_port1);
  base::RunLoop().RunUntilIdle();

  mojo::PendingReceiver<blink::mojom::SharedWorkerFactory> factory_receiver1 =
      WaitForFactoryReceiver(process_id1);
  MockSharedWorkerFactory factory1(std::move(factory_receiver1));
  base::RunLoop().RunUntilIdle();

  mojo::Remote<blink::mojom::SharedWorkerHost> worker_host1;
  mojo::PendingReceiver<blink::mojom::SharedWorker> worker_receiver1;
  EXPECT_TRUE(factory1.CheckReceivedCreateSharedWorker(
      kUrl, kName1, network::mojom::ContentSecurityPolicyType::kReport,
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
  TestRenderFrameHost* render_frame_host0 = web_contents0->GetMainFrame();
  MockRenderProcessHost* renderer_host0 = render_frame_host0->GetProcess();
  const int process_id0 = renderer_host0->GetID();
  renderer_host0->OverrideBinderForTesting(
      blink::mojom::SharedWorkerFactory::Name_,
      base::BindRepeating(&SharedWorkerServiceImplTest::BindSharedWorkerFactory,
                          base::Unretained(this), process_id0));

  // The second renderer host.
  std::unique_ptr<TestWebContents> web_contents1 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host1 = web_contents1->GetMainFrame();
  MockRenderProcessHost* renderer_host1 = render_frame_host1->GetProcess();
  const int process_id1 = renderer_host1->GetID();
  renderer_host1->OverrideBinderForTesting(
      blink::mojom::SharedWorkerFactory::Name_,
      base::BindRepeating(&SharedWorkerServiceImplTest::BindSharedWorkerFactory,
                          base::Unretained(this), process_id1));

  // First client and second client are created before the worker starts.

  MockSharedWorkerClient client0;
  MessagePortChannel local_port0;
  ConnectToSharedWorker(MakeSharedWorkerConnector(
                            renderer_host0, render_frame_host0->GetRoutingID()),
                        kUrl, kName, &client0, &local_port0);

  MockSharedWorkerClient client1;
  MessagePortChannel local_port1;
  ConnectToSharedWorker(MakeSharedWorkerConnector(
                            renderer_host1, render_frame_host1->GetRoutingID()),
                        kUrl, kName, &client1, &local_port1);

  base::RunLoop().RunUntilIdle();

  // Check that the worker was created. The receiver could come from either
  // process.

  mojo::PendingReceiver<blink::mojom::SharedWorkerFactory> factory_receiver =
      WaitForFactoryReceiver(ChildProcessHost::kInvalidUniqueID);
  MockSharedWorkerFactory factory(std::move(factory_receiver));

  base::RunLoop().RunUntilIdle();

  mojo::Remote<blink::mojom::SharedWorkerHost> worker_host;
  mojo::PendingReceiver<blink::mojom::SharedWorker> worker_receiver;
  EXPECT_TRUE(factory.CheckReceivedCreateSharedWorker(
      kUrl, kName, network::mojom::ContentSecurityPolicyType::kReport,
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
  TestRenderFrameHost* render_frame_host0 = web_contents0->GetMainFrame();
  MockRenderProcessHost* renderer_host0 = render_frame_host0->GetProcess();
  renderer_host0->OverrideBinderForTesting(
      blink::mojom::SharedWorkerFactory::Name_,
      base::BindRepeating(&SharedWorkerServiceImplTest::BindSharedWorkerFactory,
                          base::Unretained(this), renderer_host0->GetID()));

  // The second renderer host.
  std::unique_ptr<TestWebContents> web_contents1 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host1 = web_contents1->GetMainFrame();
  MockRenderProcessHost* renderer_host1 = render_frame_host1->GetProcess();
  renderer_host1->OverrideBinderForTesting(
      blink::mojom::SharedWorkerFactory::Name_,
      base::BindRepeating(&SharedWorkerServiceImplTest::BindSharedWorkerFactory,
                          base::Unretained(this), renderer_host1->GetID()));

  // First client and second client are created before the workers start.

  MockSharedWorkerClient client0;
  MessagePortChannel local_port0;
  ConnectToSharedWorker(MakeSharedWorkerConnector(
                            renderer_host0, render_frame_host0->GetRoutingID()),
                        kUrl0, kName, &client0, &local_port0);

  MockSharedWorkerClient client1;
  MessagePortChannel local_port1;
  ConnectToSharedWorker(MakeSharedWorkerConnector(
                            renderer_host1, render_frame_host1->GetRoutingID()),
                        kUrl1, kName, &client1, &local_port1);

  base::RunLoop().RunUntilIdle();

  // Check that both workers were created.

  mojo::PendingReceiver<blink::mojom::SharedWorkerFactory> factory_receiver0 =
      WaitForFactoryReceiver(renderer_host0->GetID());
  MockSharedWorkerFactory factory0(std::move(factory_receiver0));

  mojo::PendingReceiver<blink::mojom::SharedWorkerFactory> factory_receiver1 =
      WaitForFactoryReceiver(renderer_host1->GetID());
  MockSharedWorkerFactory factory1(std::move(factory_receiver1));

  base::RunLoop().RunUntilIdle();

  mojo::Remote<blink::mojom::SharedWorkerHost> worker_host0;
  mojo::PendingReceiver<blink::mojom::SharedWorker> worker_receiver0;
  EXPECT_TRUE(factory0.CheckReceivedCreateSharedWorker(
      kUrl0, kName, network::mojom::ContentSecurityPolicyType::kReport,
      &worker_host0, &worker_receiver0));
  MockSharedWorker worker0(std::move(worker_receiver0));

  mojo::Remote<blink::mojom::SharedWorkerHost> worker_host1;
  mojo::PendingReceiver<blink::mojom::SharedWorker> worker_receiver1;
  EXPECT_TRUE(factory1.CheckReceivedCreateSharedWorker(
      kUrl1, kName, network::mojom::ContentSecurityPolicyType::kReport,
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
  TestRenderFrameHost* render_frame_host0 = web_contents0->GetMainFrame();
  MockRenderProcessHost* renderer_host0 = render_frame_host0->GetProcess();
  const int process_id0 = renderer_host0->GetID();
  renderer_host0->OverrideBinderForTesting(
      blink::mojom::SharedWorkerFactory::Name_,
      base::BindRepeating(&SharedWorkerServiceImplTest::BindSharedWorkerFactory,
                          base::Unretained(this), process_id0));

  // The second renderer host.
  std::unique_ptr<TestWebContents> web_contents1 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host1 = web_contents1->GetMainFrame();
  MockRenderProcessHost* renderer_host1 = render_frame_host1->GetProcess();
  const int process_id1 = renderer_host1->GetID();
  renderer_host1->OverrideBinderForTesting(
      blink::mojom::SharedWorkerFactory::Name_,
      base::BindRepeating(&SharedWorkerServiceImplTest::BindSharedWorkerFactory,
                          base::Unretained(this), process_id1));

  // First client and second client are created before the workers start.

  MockSharedWorkerClient client0;
  MessagePortChannel local_port0;
  ConnectToSharedWorker(MakeSharedWorkerConnector(
                            renderer_host0, render_frame_host0->GetRoutingID()),
                        kUrl, kName0, &client0, &local_port0);

  MockSharedWorkerClient client1;
  MessagePortChannel local_port1;
  ConnectToSharedWorker(MakeSharedWorkerConnector(
                            renderer_host1, render_frame_host1->GetRoutingID()),
                        kUrl, kName1, &client1, &local_port1);

  base::RunLoop().RunUntilIdle();

  // Check that both workers were created.

  mojo::PendingReceiver<blink::mojom::SharedWorkerFactory> factory_receiver0 =
      WaitForFactoryReceiver(process_id0);
  MockSharedWorkerFactory factory0(std::move(factory_receiver0));

  mojo::PendingReceiver<blink::mojom::SharedWorkerFactory> factory_receiver1 =
      WaitForFactoryReceiver(process_id1);
  MockSharedWorkerFactory factory1(std::move(factory_receiver1));

  base::RunLoop().RunUntilIdle();

  mojo::Remote<blink::mojom::SharedWorkerHost> worker_host0;
  mojo::PendingReceiver<blink::mojom::SharedWorker> worker_receiver0;
  EXPECT_TRUE(factory0.CheckReceivedCreateSharedWorker(
      kUrl, kName0, network::mojom::ContentSecurityPolicyType::kReport,
      &worker_host0, &worker_receiver0));
  MockSharedWorker worker0(std::move(worker_receiver0));

  mojo::Remote<blink::mojom::SharedWorkerHost> worker_host1;
  mojo::PendingReceiver<blink::mojom::SharedWorker> worker_receiver1;
  EXPECT_TRUE(factory1.CheckReceivedCreateSharedWorker(
      kUrl, kName1, network::mojom::ContentSecurityPolicyType::kReport,
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
  TestRenderFrameHost* render_frame_host0 = web_contents0->GetMainFrame();
  MockRenderProcessHost* renderer_host0 = render_frame_host0->GetProcess();
  const int process_id0 = renderer_host0->GetID();
  renderer_host0->OverrideBinderForTesting(
      blink::mojom::SharedWorkerFactory::Name_,
      base::BindRepeating(&SharedWorkerServiceImplTest::BindSharedWorkerFactory,
                          base::Unretained(this), process_id0));

  std::unique_ptr<TestWebContents> web_contents1 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host1 = web_contents1->GetMainFrame();
  MockRenderProcessHost* renderer_host1 = render_frame_host1->GetProcess();
  const int process_id1 = renderer_host1->GetID();
  renderer_host1->OverrideBinderForTesting(
      blink::mojom::SharedWorkerFactory::Name_,
      base::BindRepeating(&SharedWorkerServiceImplTest::BindSharedWorkerFactory,
                          base::Unretained(this), process_id1));

  std::unique_ptr<TestWebContents> web_contents2 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host2 = web_contents2->GetMainFrame();
  MockRenderProcessHost* renderer_host2 = render_frame_host2->GetProcess();
  const int process_id2 = renderer_host2->GetID();
  renderer_host2->OverrideBinderForTesting(
      blink::mojom::SharedWorkerFactory::Name_,
      base::BindRepeating(&SharedWorkerServiceImplTest::BindSharedWorkerFactory,
                          base::Unretained(this), process_id2));

  MockSharedWorkerClient client0;
  MessagePortChannel local_port0;
  ConnectToSharedWorker(MakeSharedWorkerConnector(
                            renderer_host0, render_frame_host0->GetRoutingID()),
                        kUrl, kName, &client0, &local_port0);

  base::RunLoop().RunUntilIdle();

  // Starts a worker.

  mojo::PendingReceiver<blink::mojom::SharedWorkerFactory> factory_receiver0 =
      WaitForFactoryReceiver(process_id0);
  MockSharedWorkerFactory factory0(std::move(factory_receiver0));

  base::RunLoop().RunUntilIdle();

  mojo::Remote<blink::mojom::SharedWorkerHost> worker_host0;
  mojo::PendingReceiver<blink::mojom::SharedWorker> worker_receiver0;
  EXPECT_TRUE(factory0.CheckReceivedCreateSharedWorker(
      kUrl, kName, network::mojom::ContentSecurityPolicyType::kReport,
      &worker_host0, &worker_receiver0));
  MockSharedWorker worker0(std::move(worker_receiver0));

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(worker0.CheckReceivedConnect(nullptr, nullptr));
  EXPECT_TRUE(client0.CheckReceivedOnCreated());

  // Kill this process, which should make worker0 unavailable.
  KillProcess(std::move(web_contents0));

  // Start a new client, attemping to connect to the same worker.
  MockSharedWorkerClient client1;
  MessagePortChannel local_port1;
  ConnectToSharedWorker(MakeSharedWorkerConnector(
                            renderer_host1, render_frame_host1->GetRoutingID()),
                        kUrl, kName, &client1, &local_port1);

  base::RunLoop().RunUntilIdle();

  // The previous worker is unavailable, so a new worker is created.

  mojo::PendingReceiver<blink::mojom::SharedWorkerFactory> factory_receiver1 =
      WaitForFactoryReceiver(process_id1);
  MockSharedWorkerFactory factory1(std::move(factory_receiver1));

  base::RunLoop().RunUntilIdle();

  mojo::Remote<blink::mojom::SharedWorkerHost> worker_host1;
  mojo::PendingReceiver<blink::mojom::SharedWorker> worker_receiver1;
  EXPECT_TRUE(factory1.CheckReceivedCreateSharedWorker(
      kUrl, kName, network::mojom::ContentSecurityPolicyType::kReport,
      &worker_host1, &worker_receiver1));
  MockSharedWorker worker1(std::move(worker_receiver1));

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(worker0.CheckNotReceivedConnect());
  EXPECT_TRUE(worker1.CheckReceivedConnect(nullptr, nullptr));
  EXPECT_TRUE(client1.CheckReceivedOnCreated());

  // Start another client to confirm that it can connect to the same worker.
  MockSharedWorkerClient client2;
  MessagePortChannel local_port2;
  ConnectToSharedWorker(MakeSharedWorkerConnector(
                            renderer_host2, render_frame_host2->GetRoutingID()),
                        kUrl, kName, &client2, &local_port2);

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(CheckNotReceivedFactoryReceiver(process_id2));

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
  TestRenderFrameHost* render_frame_host0 = web_contents0->GetMainFrame();
  MockRenderProcessHost* renderer_host0 = render_frame_host0->GetProcess();
  const int process_id0 = renderer_host0->GetID();
  renderer_host0->OverrideBinderForTesting(
      blink::mojom::SharedWorkerFactory::Name_,
      base::BindRepeating(&SharedWorkerServiceImplTest::BindSharedWorkerFactory,
                          base::Unretained(this), process_id0));

  std::unique_ptr<TestWebContents> web_contents1 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host1 = web_contents1->GetMainFrame();
  MockRenderProcessHost* renderer_host1 = render_frame_host1->GetProcess();
  const int process_id1 = renderer_host1->GetID();
  renderer_host1->OverrideBinderForTesting(
      blink::mojom::SharedWorkerFactory::Name_,
      base::BindRepeating(&SharedWorkerServiceImplTest::BindSharedWorkerFactory,
                          base::Unretained(this), process_id1));

  std::unique_ptr<TestWebContents> web_contents2 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host2 = web_contents2->GetMainFrame();
  MockRenderProcessHost* renderer_host2 = render_frame_host2->GetProcess();
  const int process_id2 = renderer_host2->GetID();
  renderer_host2->OverrideBinderForTesting(
      blink::mojom::SharedWorkerFactory::Name_,
      base::BindRepeating(&SharedWorkerServiceImplTest::BindSharedWorkerFactory,
                          base::Unretained(this), process_id2));

  MockSharedWorkerClient client0;
  MessagePortChannel local_port0;
  ConnectToSharedWorker(MakeSharedWorkerConnector(
                            renderer_host0, render_frame_host0->GetRoutingID()),
                        kUrl, kName, &client0, &local_port0);

  mojo::PendingReceiver<blink::mojom::SharedWorkerFactory> factory_receiver0 =
      WaitForFactoryReceiver(process_id0);
  MockSharedWorkerFactory factory0(std::move(factory_receiver0));

  // Kill this process, which should make worker0 unavailable.
  KillProcess(std::move(web_contents0));

  // Start a new client, attempting to connect to the same worker.
  MockSharedWorkerClient client1;
  MessagePortChannel local_port1;
  ConnectToSharedWorker(MakeSharedWorkerConnector(
                            renderer_host1, render_frame_host1->GetRoutingID()),
                        kUrl, kName, &client1, &local_port1);

  base::RunLoop().RunUntilIdle();

  // The previous worker is unavailable, so a new worker is created.

  mojo::PendingReceiver<blink::mojom::SharedWorkerFactory> factory_receiver1 =
      WaitForFactoryReceiver(process_id1);
  MockSharedWorkerFactory factory1(std::move(factory_receiver1));

  EXPECT_TRUE(
      CheckNotReceivedFactoryReceiver(ChildProcessHost::kInvalidUniqueID));

  base::RunLoop().RunUntilIdle();

  mojo::Remote<blink::mojom::SharedWorkerHost> worker_host1;
  mojo::PendingReceiver<blink::mojom::SharedWorker> worker_receiver1;
  EXPECT_TRUE(factory1.CheckReceivedCreateSharedWorker(
      kUrl, kName, network::mojom::ContentSecurityPolicyType::kReport,
      &worker_host1, &worker_receiver1));
  MockSharedWorker worker1(std::move(worker_receiver1));

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(worker1.CheckReceivedConnect(nullptr, nullptr));
  EXPECT_TRUE(client1.CheckReceivedOnCreated());

  // Start another client to confirm that it can connect to the same worker.
  MockSharedWorkerClient client2;
  MessagePortChannel local_port2;
  ConnectToSharedWorker(MakeSharedWorkerConnector(
                            renderer_host2, render_frame_host2->GetRoutingID()),
                        kUrl, kName, &client2, &local_port2);

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(
      CheckNotReceivedFactoryReceiver(ChildProcessHost::kInvalidUniqueID));

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
  TestRenderFrameHost* render_frame_host0 = web_contents0->GetMainFrame();
  MockRenderProcessHost* renderer_host0 = render_frame_host0->GetProcess();
  const int process_id0 = renderer_host0->GetID();
  renderer_host0->OverrideBinderForTesting(
      blink::mojom::SharedWorkerFactory::Name_,
      base::BindRepeating(&SharedWorkerServiceImplTest::BindSharedWorkerFactory,
                          base::Unretained(this), process_id0));

  // The second renderer host.
  std::unique_ptr<TestWebContents> web_contents1 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host1 = web_contents1->GetMainFrame();
  MockRenderProcessHost* renderer_host1 = render_frame_host1->GetProcess();
  const int process_id1 = renderer_host1->GetID();
  renderer_host1->OverrideBinderForTesting(
      blink::mojom::SharedWorkerFactory::Name_,
      base::BindRepeating(&SharedWorkerServiceImplTest::BindSharedWorkerFactory,
                          base::Unretained(this), process_id1));

  // Both clients try to connect/create a worker.

  MockSharedWorkerClient client0;
  MessagePortChannel local_port0;
  ConnectToSharedWorker(MakeSharedWorkerConnector(
                            renderer_host0, render_frame_host0->GetRoutingID()),
                        kURL, kName, &client0, &local_port0);

  MockSharedWorkerClient client1;
  MessagePortChannel local_port1;
  ConnectToSharedWorker(MakeSharedWorkerConnector(
                            renderer_host1, render_frame_host1->GetRoutingID()),
                        kURL, kName, &client1, &local_port1);
  base::RunLoop().RunUntilIdle();

  // Expect a factory receiver. It can come from either process.
  mojo::PendingReceiver<blink::mojom::SharedWorkerFactory> factory_receiver =
      WaitForFactoryReceiver(ChildProcessHost::kInvalidUniqueID);
  MockSharedWorkerFactory factory(std::move(factory_receiver));
  base::RunLoop().RunUntilIdle();

  // Expect a create shared worker.
  mojo::Remote<blink::mojom::SharedWorkerHost> worker_host;
  mojo::PendingReceiver<blink::mojom::SharedWorker> worker_receiver;
  EXPECT_TRUE(factory.CheckReceivedCreateSharedWorker(
      kURL, kName, network::mojom::ContentSecurityPolicyType::kReport,
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
  ~TestSharedWorkerServiceObserver() override = default;

  // SharedWorkerService::Observer:
  void OnWorkerStarted(const SharedWorkerInstance& instance,
                       int worker_process_id,
                       const base::UnguessableToken& dev_tools_token) override {
    EXPECT_TRUE(running_workers_.insert({instance, {}}).second);
  }
  void OnBeforeWorkerTerminated(const SharedWorkerInstance& instance) override {
    EXPECT_EQ(1u, running_workers_.erase(instance));
  }
  void OnClientAdded(const SharedWorkerInstance& instance,
                     int client_process_id,
                     int frame_id) override {
    auto it = running_workers_.find(instance);
    EXPECT_TRUE(it != running_workers_.end());
    std::set<ClientInfo>& clients = it->second;
    EXPECT_TRUE(clients.insert({client_process_id, frame_id}).second);
  }
  void OnClientRemoved(const SharedWorkerInstance& instance,
                       int client_process_id,
                       int frame_id) override {
    auto it = running_workers_.find(instance);
    EXPECT_TRUE(it != running_workers_.end());
    std::set<ClientInfo>& clients = it->second;
    EXPECT_EQ(1u, clients.erase({client_process_id, frame_id}));
  }

  size_t GetWorkerCount() { return running_workers_.size(); }

  size_t GetClientCount() {
    size_t client_count = 0;
    for (const auto& worker : running_workers_)
      client_count += worker.second.size();
    return client_count;
  }

 private:
  using ClientInfo = std::pair<int, int>;

  base::flat_map<SharedWorkerInstance, std::set<ClientInfo>> running_workers_;

  DISALLOW_COPY_AND_ASSIGN(TestSharedWorkerServiceObserver);
};

TEST_F(SharedWorkerServiceImplTest, Observer) {
  TestSharedWorkerServiceObserver observer;

  ScopedObserver<SharedWorkerService, SharedWorkerService::Observer>
      scoped_observer(&observer);
  scoped_observer.Add(content::BrowserContext::GetDefaultStoragePartition(
                          browser_context_.get())
                          ->GetSharedWorkerService());

  std::unique_ptr<TestWebContents> web_contents =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host = web_contents->GetMainFrame();
  MockRenderProcessHost* renderer_host = render_frame_host->GetProcess();
  const int process_id = renderer_host->GetID();
  renderer_host->OverrideBinderForTesting(
      blink::mojom::SharedWorkerFactory::Name_,
      base::BindRepeating(&SharedWorkerServiceImplTest::BindSharedWorkerFactory,
                          base::Unretained(this), process_id));

  MockSharedWorkerClient client;
  MessagePortChannel local_port;
  const GURL kUrl("http://example.com/w.js");
  ConnectToSharedWorker(MakeSharedWorkerConnector(
                            renderer_host, render_frame_host->GetRoutingID()),
                        kUrl, "name", &client, &local_port);

  mojo::PendingReceiver<blink::mojom::SharedWorkerFactory> factory_receiver =
      WaitForFactoryReceiver(process_id);
  MockSharedWorkerFactory factory(std::move(factory_receiver));
  base::RunLoop().RunUntilIdle();

  mojo::Remote<blink::mojom::SharedWorkerHost> worker_host;
  mojo::PendingReceiver<blink::mojom::SharedWorker> worker_receiver;
  EXPECT_TRUE(factory.CheckReceivedCreateSharedWorker(
      kUrl, "name", network::mojom::ContentSecurityPolicyType::kReport,
      &worker_host, &worker_receiver));
  MockSharedWorker worker(std::move(worker_receiver));
  base::RunLoop().RunUntilIdle();

  int connection_request_id;
  MessagePortChannel port;
  EXPECT_TRUE(worker.CheckReceivedConnect(&connection_request_id, &port));

  EXPECT_TRUE(client.CheckReceivedOnCreated());

  EXPECT_EQ(1u, observer.GetWorkerCount());
  EXPECT_EQ(1u, observer.GetClientCount());

  // Tear down the worker host.
  worker_host->OnContextClosed();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0u, observer.GetWorkerCount());
  EXPECT_EQ(0u, observer.GetClientCount());
}

TEST_F(SharedWorkerServiceImplTest, CollapseDuplicateNotifications) {
  TestSharedWorkerServiceObserver observer;

  ScopedObserver<SharedWorkerService, SharedWorkerService::Observer>
      scoped_observer(&observer);
  scoped_observer.Add(content::BrowserContext::GetDefaultStoragePartition(
                          browser_context_.get())
                          ->GetSharedWorkerService());

  const GURL kUrl("http://example.com/w.js");
  const char kName[] = "name";

  // The first renderer host.
  std::unique_ptr<TestWebContents> web_contents =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host = web_contents->GetMainFrame();
  MockRenderProcessHost* renderer_host = render_frame_host->GetProcess();
  const int process_id = renderer_host->GetID();
  renderer_host->OverrideBinderForTesting(
      blink::mojom::SharedWorkerFactory::Name_,
      base::BindRepeating(&SharedWorkerServiceImplTest::BindSharedWorkerFactory,
                          base::Unretained(this), process_id));

  // First client, creates worker.

  MockSharedWorkerClient client0;
  MessagePortChannel local_port0;
  ConnectToSharedWorker(MakeSharedWorkerConnector(
                            renderer_host, render_frame_host->GetRoutingID()),
                        kUrl, kName, &client0, &local_port0);
  base::RunLoop().RunUntilIdle();

  mojo::PendingReceiver<blink::mojom::SharedWorkerFactory> factory_receiver =
      WaitForFactoryReceiver(process_id);
  MockSharedWorkerFactory factory(std::move(factory_receiver));
  base::RunLoop().RunUntilIdle();

  mojo::Remote<blink::mojom::SharedWorkerHost> worker_host;
  mojo::PendingReceiver<blink::mojom::SharedWorker> worker_receiver;
  EXPECT_TRUE(factory.CheckReceivedCreateSharedWorker(
      kUrl, kName, network::mojom::ContentSecurityPolicyType::kReport,
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
  ConnectToSharedWorker(MakeSharedWorkerConnector(
                            renderer_host, render_frame_host->GetRoutingID()),
                        kUrl, kName, &client1, &local_port1);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(CheckNotReceivedFactoryReceiver(process_id));

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

}  // namespace content
