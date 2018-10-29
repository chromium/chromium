// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_worker/shared_worker_service_impl.h"

#include <memory>
#include <queue>
#include <set>
#include <string>

#include "base/macros.h"
#include "base/run_loop.h"
#include "content/browser/shared_worker/mock_shared_worker.h"
#include "content/browser/shared_worker/shared_worker_connector_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_utils.h"
#include "content/test/not_implemented_network_url_loader_factory.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/messaging/message_port_channel.h"

using blink::MessagePortChannel;

namespace content {

class SharedWorkerServiceImplTest : public RenderViewHostImplTestHarness {
 public:
  mojom::SharedWorkerConnectorPtr MakeSharedWorkerConnector(
      RenderProcessHost* process_host,
      int frame_id) {
    mojom::SharedWorkerConnectorPtr connector;
    SharedWorkerConnectorImpl::Create(process_host->GetID(), frame_id,
                                      mojo::MakeRequest(&connector));
    return connector;
  }

  static bool CheckReceivedFactoryRequest(
      mojom::SharedWorkerFactoryRequest* request) {
    if (s_factory_request_received_.empty())
      return false;
    *request = std::move(s_factory_request_received_.front());
    s_factory_request_received_.pop();
    return true;
  }

  static bool CheckNotReceivedFactoryRequest() {
    return s_factory_request_received_.empty();
  }

  static void BindSharedWorkerFactory(mojo::ScopedMessagePipeHandle handle) {
    s_factory_request_received_.push(
        mojom::SharedWorkerFactoryRequest(std::move(handle)));
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
    url_loader_factory_ =
        std::make_unique<NotImplementedNetworkURLLoaderFactory>();
  }

  void TearDown() override {
    browser_context_.reset();
    RenderViewHostImplTestHarness::TearDown();
  }

  std::unique_ptr<TestBrowserContext> browser_context_;
  static std::queue<mojom::SharedWorkerFactoryRequest>
      s_factory_request_received_;
  std::unique_ptr<MockRenderProcessHostFactory> render_process_host_factory_;
  std::unique_ptr<NotImplementedNetworkURLLoaderFactory> url_loader_factory_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SharedWorkerServiceImplTest);
};

// static
std::queue<mojom::SharedWorkerFactoryRequest>
    SharedWorkerServiceImplTest::s_factory_request_received_;

namespace {

void ConnectToSharedWorker(mojom::SharedWorkerConnectorPtr connector,
                           const GURL& url,
                           const std::string& name,
                           MockSharedWorkerClient* client,
                           MessagePortChannel* local_port) {
  mojom::SharedWorkerInfoPtr info(mojom::SharedWorkerInfo::New(
      url, name, std::string(), blink::kWebContentSecurityPolicyTypeReport,
      blink::mojom::IPAddressSpace::kPublic));

  mojo::MessagePipe message_pipe;
  *local_port = MessagePortChannel(std::move(message_pipe.handle0));

  mojom::SharedWorkerClientPtr client_proxy;
  client->Bind(mojo::MakeRequest(&client_proxy));

  connector->Connect(std::move(info), std::move(client_proxy),
                     blink::mojom::SharedWorkerCreationContextType::kSecure,
                     std::move(message_pipe.handle1), nullptr);
}

}  // namespace

TEST_F(SharedWorkerServiceImplTest, BasicTest) {
  std::unique_ptr<TestWebContents> web_contents =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host = web_contents->GetMainFrame();
  MockRenderProcessHost* renderer_host = render_frame_host->GetProcess();
  renderer_host->OverrideBinderForTesting(
      mojom::SharedWorkerFactory::Name_,
      base::Bind(&SharedWorkerServiceImplTest::BindSharedWorkerFactory));
  renderer_host->OverrideURLLoaderFactory(url_loader_factory_.get());

  MockSharedWorkerClient client;
  MessagePortChannel local_port;
  const GURL kUrl("http://example.com/w.js");
  ConnectToSharedWorker(MakeSharedWorkerConnector(
                            renderer_host, render_frame_host->GetRoutingID()),
                        kUrl, "name", &client, &local_port);

  base::RunLoop().RunUntilIdle();

  mojom::SharedWorkerFactoryRequest factory_request;
  EXPECT_TRUE(CheckReceivedFactoryRequest(&factory_request));
  MockSharedWorkerFactory factory(std::move(factory_request));
  base::RunLoop().RunUntilIdle();

  mojom::SharedWorkerHostPtr worker_host;
  mojom::SharedWorkerRequest worker_request;
  EXPECT_TRUE(factory.CheckReceivedCreateSharedWorker(
      kUrl, "name", blink::kWebContentSecurityPolicyTypeReport, &worker_host,
      &worker_request));
  MockSharedWorker worker(std::move(worker_request));
  base::RunLoop().RunUntilIdle();

  int connection_request_id;
  MessagePortChannel port;
  EXPECT_TRUE(worker.CheckReceivedConnect(&connection_request_id, &port));

  EXPECT_TRUE(client.CheckReceivedOnCreated());

  // Simulate events the shared worker would send.
  worker_host->OnReadyForInspection();
  worker_host->OnScriptLoaded();
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
}

TEST_F(SharedWorkerServiceImplTest, TwoRendererTest) {
  // The first renderer host.
  std::unique_ptr<TestWebContents> web_contents0 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host0 = web_contents0->GetMainFrame();
  MockRenderProcessHost* renderer_host0 = render_frame_host0->GetProcess();
  renderer_host0->OverrideBinderForTesting(
      mojom::SharedWorkerFactory::Name_,
      base::Bind(&SharedWorkerServiceImplTest::BindSharedWorkerFactory));
  renderer_host0->OverrideURLLoaderFactory(url_loader_factory_.get());

  MockSharedWorkerClient client0;
  MessagePortChannel local_port0;
  const GURL kUrl("http://example.com/w.js");
  ConnectToSharedWorker(MakeSharedWorkerConnector(
                            renderer_host0, render_frame_host0->GetRoutingID()),
                        kUrl, "name", &client0, &local_port0);

  base::RunLoop().RunUntilIdle();

  mojom::SharedWorkerFactoryRequest factory_request;
  EXPECT_TRUE(CheckReceivedFactoryRequest(&factory_request));
  MockSharedWorkerFactory factory(std::move(factory_request));
  base::RunLoop().RunUntilIdle();

  mojom::SharedWorkerHostPtr worker_host;
  mojom::SharedWorkerRequest worker_request;
  EXPECT_TRUE(factory.CheckReceivedCreateSharedWorker(
      kUrl, "name", blink::kWebContentSecurityPolicyTypeReport, &worker_host,
      &worker_request));
  MockSharedWorker worker(std::move(worker_request));
  base::RunLoop().RunUntilIdle();

  int connection_request_id0;
  MessagePortChannel port0;
  EXPECT_TRUE(worker.CheckReceivedConnect(&connection_request_id0, &port0));

  EXPECT_TRUE(client0.CheckReceivedOnCreated());

  // Simulate events the shared worker would send.
  worker_host->OnReadyForInspection();
  worker_host->OnScriptLoaded();
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
  renderer_host1->OverrideBinderForTesting(
      mojom::SharedWorkerFactory::Name_,
      base::Bind(&SharedWorkerServiceImplTest::BindSharedWorkerFactory));
  renderer_host1->OverrideURLLoaderFactory(url_loader_factory_.get());

  MockSharedWorkerClient client1;
  MessagePortChannel local_port1;
  ConnectToSharedWorker(MakeSharedWorkerConnector(
                            renderer_host1, render_frame_host1->GetRoutingID()),
                        kUrl, "name", &client1, &local_port1);

  base::RunLoop().RunUntilIdle();

  // Should not have tried to create a new shared worker.
  EXPECT_TRUE(CheckNotReceivedFactoryRequest());

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
}

TEST_F(SharedWorkerServiceImplTest, CreateWorkerTest_NormalCase) {
  const GURL kUrl("http://example.com/w.js");
  const char kName[] = "name";

  // The first renderer host.
  std::unique_ptr<TestWebContents> web_contents0 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host0 = web_contents0->GetMainFrame();
  MockRenderProcessHost* renderer_host0 = render_frame_host0->GetProcess();
  renderer_host0->OverrideBinderForTesting(
      mojom::SharedWorkerFactory::Name_,
      base::Bind(&SharedWorkerServiceImplTest::BindSharedWorkerFactory));
  renderer_host0->OverrideURLLoaderFactory(url_loader_factory_.get());

  // The second renderer host.
  std::unique_ptr<TestWebContents> web_contents1 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host1 = web_contents1->GetMainFrame();
  MockRenderProcessHost* renderer_host1 = render_frame_host1->GetProcess();
  renderer_host1->OverrideBinderForTesting(
      mojom::SharedWorkerFactory::Name_,
      base::Bind(&SharedWorkerServiceImplTest::BindSharedWorkerFactory));

  // First client, creates worker.

  MockSharedWorkerClient client0;
  MessagePortChannel local_port0;
  ConnectToSharedWorker(MakeSharedWorkerConnector(
                            renderer_host0, render_frame_host0->GetRoutingID()),
                        kUrl, kName, &client0, &local_port0);
  base::RunLoop().RunUntilIdle();

  mojom::SharedWorkerFactoryRequest factory_request;
  EXPECT_TRUE(CheckReceivedFactoryRequest(&factory_request));
  MockSharedWorkerFactory factory(std::move(factory_request));
  base::RunLoop().RunUntilIdle();

  mojom::SharedWorkerHostPtr worker_host;
  mojom::SharedWorkerRequest worker_request;
  EXPECT_TRUE(factory.CheckReceivedCreateSharedWorker(
      kUrl, kName, blink::kWebContentSecurityPolicyTypeReport, &worker_host,
      &worker_request));
  MockSharedWorker worker(std::move(worker_request));
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

  EXPECT_TRUE(CheckNotReceivedFactoryRequest());

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
  renderer_host0->OverrideBinderForTesting(
      mojom::SharedWorkerFactory::Name_,
      base::Bind(&SharedWorkerServiceImplTest::BindSharedWorkerFactory));
  renderer_host0->OverrideURLLoaderFactory(url_loader_factory_.get());

  // The second renderer host.
  std::unique_ptr<TestWebContents> web_contents1 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host1 = web_contents1->GetMainFrame();
  MockRenderProcessHost* renderer_host1 = render_frame_host1->GetProcess();
  renderer_host1->OverrideBinderForTesting(
      mojom::SharedWorkerFactory::Name_,
      base::Bind(&SharedWorkerServiceImplTest::BindSharedWorkerFactory));
  renderer_host1->OverrideURLLoaderFactory(url_loader_factory_.get());

  // First client, creates worker.

  MockSharedWorkerClient client0;
  MessagePortChannel local_port0;
  ConnectToSharedWorker(MakeSharedWorkerConnector(
                            renderer_host0, render_frame_host0->GetRoutingID()),
                        kUrl0, kName, &client0, &local_port0);
  base::RunLoop().RunUntilIdle();

  mojom::SharedWorkerFactoryRequest factory_request0;
  EXPECT_TRUE(CheckReceivedFactoryRequest(&factory_request0));
  MockSharedWorkerFactory factory0(std::move(factory_request0));
  base::RunLoop().RunUntilIdle();

  mojom::SharedWorkerHostPtr worker_host0;
  mojom::SharedWorkerRequest worker_request0;
  EXPECT_TRUE(factory0.CheckReceivedCreateSharedWorker(
      kUrl0, kName, blink::kWebContentSecurityPolicyTypeReport, &worker_host0,
      &worker_request0));
  MockSharedWorker worker0(std::move(worker_request0));
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

  mojom::SharedWorkerFactoryRequest factory_request1;
  EXPECT_TRUE(CheckReceivedFactoryRequest(&factory_request1));
  MockSharedWorkerFactory factory1(std::move(factory_request1));
  base::RunLoop().RunUntilIdle();

  mojom::SharedWorkerHostPtr worker_host1;
  mojom::SharedWorkerRequest worker_request1;
  EXPECT_TRUE(factory1.CheckReceivedCreateSharedWorker(
      kUrl1, kName, blink::kWebContentSecurityPolicyTypeReport, &worker_host1,
      &worker_request1));
  MockSharedWorker worker1(std::move(worker_request1));
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
  renderer_host0->OverrideBinderForTesting(
      mojom::SharedWorkerFactory::Name_,
      base::Bind(&SharedWorkerServiceImplTest::BindSharedWorkerFactory));
  renderer_host0->OverrideURLLoaderFactory(url_loader_factory_.get());

  // The second renderer host.
  std::unique_ptr<TestWebContents> web_contents1 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host1 = web_contents1->GetMainFrame();
  MockRenderProcessHost* renderer_host1 = render_frame_host1->GetProcess();
  renderer_host1->OverrideBinderForTesting(
      mojom::SharedWorkerFactory::Name_,
      base::Bind(&SharedWorkerServiceImplTest::BindSharedWorkerFactory));
  renderer_host1->OverrideURLLoaderFactory(url_loader_factory_.get());

  // First client, creates worker.

  MockSharedWorkerClient client0;
  MessagePortChannel local_port0;
  ConnectToSharedWorker(MakeSharedWorkerConnector(
                            renderer_host0, render_frame_host0->GetRoutingID()),
                        kUrl, kName0, &client0, &local_port0);
  base::RunLoop().RunUntilIdle();

  mojom::SharedWorkerFactoryRequest factory_request0;
  EXPECT_TRUE(CheckReceivedFactoryRequest(&factory_request0));
  MockSharedWorkerFactory factory0(std::move(factory_request0));
  base::RunLoop().RunUntilIdle();

  mojom::SharedWorkerHostPtr worker_host0;
  mojom::SharedWorkerRequest worker_request0;
  EXPECT_TRUE(factory0.CheckReceivedCreateSharedWorker(
      kUrl, kName0, blink::kWebContentSecurityPolicyTypeReport, &worker_host0,
      &worker_request0));
  MockSharedWorker worker0(std::move(worker_request0));
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

  mojom::SharedWorkerFactoryRequest factory_request1;
  EXPECT_TRUE(CheckReceivedFactoryRequest(&factory_request1));
  MockSharedWorkerFactory factory1(std::move(factory_request1));
  base::RunLoop().RunUntilIdle();

  mojom::SharedWorkerHostPtr worker_host1;
  mojom::SharedWorkerRequest worker_request1;
  EXPECT_TRUE(factory1.CheckReceivedCreateSharedWorker(
      kUrl, kName1, blink::kWebContentSecurityPolicyTypeReport, &worker_host1,
      &worker_request1));
  MockSharedWorker worker1(std::move(worker_request1));
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
  renderer_host0->OverrideBinderForTesting(
      mojom::SharedWorkerFactory::Name_,
      base::Bind(&SharedWorkerServiceImplTest::BindSharedWorkerFactory));
  renderer_host0->OverrideURLLoaderFactory(url_loader_factory_.get());

  // The second renderer host.
  std::unique_ptr<TestWebContents> web_contents1 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host1 = web_contents1->GetMainFrame();
  MockRenderProcessHost* renderer_host1 = render_frame_host1->GetProcess();
  renderer_host1->OverrideBinderForTesting(
      mojom::SharedWorkerFactory::Name_,
      base::Bind(&SharedWorkerServiceImplTest::BindSharedWorkerFactory));
  renderer_host1->OverrideURLLoaderFactory(url_loader_factory_.get());

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

  // Check that the worker was created.

  mojom::SharedWorkerFactoryRequest factory_request;
  EXPECT_TRUE(CheckReceivedFactoryRequest(&factory_request));
  MockSharedWorkerFactory factory(std::move(factory_request));

  base::RunLoop().RunUntilIdle();

  mojom::SharedWorkerHostPtr worker_host;
  mojom::SharedWorkerRequest worker_request;
  EXPECT_TRUE(factory.CheckReceivedCreateSharedWorker(
      kUrl, kName, blink::kWebContentSecurityPolicyTypeReport, &worker_host,
      &worker_request));
  MockSharedWorker worker(std::move(worker_request));

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
      mojom::SharedWorkerFactory::Name_,
      base::Bind(&SharedWorkerServiceImplTest::BindSharedWorkerFactory));
  renderer_host0->OverrideURLLoaderFactory(url_loader_factory_.get());

  // The second renderer host.
  std::unique_ptr<TestWebContents> web_contents1 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host1 = web_contents1->GetMainFrame();
  MockRenderProcessHost* renderer_host1 = render_frame_host1->GetProcess();
  renderer_host1->OverrideBinderForTesting(
      mojom::SharedWorkerFactory::Name_,
      base::Bind(&SharedWorkerServiceImplTest::BindSharedWorkerFactory));
  renderer_host1->OverrideURLLoaderFactory(url_loader_factory_.get());

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

  mojom::SharedWorkerFactoryRequest factory_request0;
  EXPECT_TRUE(CheckReceivedFactoryRequest(&factory_request0));
  MockSharedWorkerFactory factory0(std::move(factory_request0));

  mojom::SharedWorkerFactoryRequest factory_request1;
  EXPECT_TRUE(CheckReceivedFactoryRequest(&factory_request1));
  MockSharedWorkerFactory factory1(std::move(factory_request1));

  base::RunLoop().RunUntilIdle();

  mojom::SharedWorkerHostPtr worker_host0;
  mojom::SharedWorkerRequest worker_request0;
  EXPECT_TRUE(factory0.CheckReceivedCreateSharedWorker(
      kUrl0, kName, blink::kWebContentSecurityPolicyTypeReport, &worker_host0,
      &worker_request0));
  MockSharedWorker worker0(std::move(worker_request0));

  mojom::SharedWorkerHostPtr worker_host1;
  mojom::SharedWorkerRequest worker_request1;
  EXPECT_TRUE(factory1.CheckReceivedCreateSharedWorker(
      kUrl1, kName, blink::kWebContentSecurityPolicyTypeReport, &worker_host1,
      &worker_request1));
  MockSharedWorker worker1(std::move(worker_request1));

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
  renderer_host0->OverrideBinderForTesting(
      mojom::SharedWorkerFactory::Name_,
      base::Bind(&SharedWorkerServiceImplTest::BindSharedWorkerFactory));
  renderer_host0->OverrideURLLoaderFactory(url_loader_factory_.get());

  // The second renderer host.
  std::unique_ptr<TestWebContents> web_contents1 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host1 = web_contents1->GetMainFrame();
  MockRenderProcessHost* renderer_host1 = render_frame_host1->GetProcess();
  renderer_host1->OverrideBinderForTesting(
      mojom::SharedWorkerFactory::Name_,
      base::Bind(&SharedWorkerServiceImplTest::BindSharedWorkerFactory));
  renderer_host1->OverrideURLLoaderFactory(url_loader_factory_.get());

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

  mojom::SharedWorkerFactoryRequest factory_request0;
  EXPECT_TRUE(CheckReceivedFactoryRequest(&factory_request0));
  MockSharedWorkerFactory factory0(std::move(factory_request0));

  mojom::SharedWorkerFactoryRequest factory_request1;
  EXPECT_TRUE(CheckReceivedFactoryRequest(&factory_request1));
  MockSharedWorkerFactory factory1(std::move(factory_request1));

  base::RunLoop().RunUntilIdle();

  mojom::SharedWorkerHostPtr worker_host0;
  mojom::SharedWorkerRequest worker_request0;
  EXPECT_TRUE(factory0.CheckReceivedCreateSharedWorker(
      kUrl, kName0, blink::kWebContentSecurityPolicyTypeReport, &worker_host0,
      &worker_request0));
  MockSharedWorker worker0(std::move(worker_request0));

  mojom::SharedWorkerHostPtr worker_host1;
  mojom::SharedWorkerRequest worker_request1;
  EXPECT_TRUE(factory1.CheckReceivedCreateSharedWorker(
      kUrl, kName1, blink::kWebContentSecurityPolicyTypeReport, &worker_host1,
      &worker_request1));
  MockSharedWorker worker1(std::move(worker_request1));

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
  renderer_host0->OverrideBinderForTesting(
      mojom::SharedWorkerFactory::Name_,
      base::Bind(&SharedWorkerServiceImplTest::BindSharedWorkerFactory));
  renderer_host0->OverrideURLLoaderFactory(url_loader_factory_.get());

  std::unique_ptr<TestWebContents> web_contents1 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host1 = web_contents1->GetMainFrame();
  MockRenderProcessHost* renderer_host1 = render_frame_host1->GetProcess();
  renderer_host1->OverrideBinderForTesting(
      mojom::SharedWorkerFactory::Name_,
      base::Bind(&SharedWorkerServiceImplTest::BindSharedWorkerFactory));
  renderer_host1->OverrideURLLoaderFactory(url_loader_factory_.get());

  std::unique_ptr<TestWebContents> web_contents2 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host2 = web_contents2->GetMainFrame();
  MockRenderProcessHost* renderer_host2 = render_frame_host2->GetProcess();
  renderer_host2->OverrideBinderForTesting(
      mojom::SharedWorkerFactory::Name_,
      base::Bind(&SharedWorkerServiceImplTest::BindSharedWorkerFactory));
  renderer_host2->OverrideURLLoaderFactory(url_loader_factory_.get());

  MockSharedWorkerClient client0;
  MessagePortChannel local_port0;
  ConnectToSharedWorker(MakeSharedWorkerConnector(
                            renderer_host0, render_frame_host0->GetRoutingID()),
                        kUrl, kName, &client0, &local_port0);

  base::RunLoop().RunUntilIdle();

  // Starts a worker.

  mojom::SharedWorkerFactoryRequest factory_request0;
  EXPECT_TRUE(CheckReceivedFactoryRequest(&factory_request0));
  MockSharedWorkerFactory factory0(std::move(factory_request0));

  base::RunLoop().RunUntilIdle();

  mojom::SharedWorkerHostPtr worker_host0;
  mojom::SharedWorkerRequest worker_request0;
  EXPECT_TRUE(factory0.CheckReceivedCreateSharedWorker(
      kUrl, kName, blink::kWebContentSecurityPolicyTypeReport, &worker_host0,
      &worker_request0));
  MockSharedWorker worker0(std::move(worker_request0));

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(worker0.CheckReceivedConnect(nullptr, nullptr));
  EXPECT_TRUE(client0.CheckReceivedOnCreated());

  // Kill this process, which should make worker0 unavailable.
  web_contents0.reset();
  renderer_host0->FastShutdownIfPossible(0, true);
  ASSERT_TRUE(renderer_host0->FastShutdownStarted());

  // Start a new client, attemping to connect to the same worker.
  MockSharedWorkerClient client1;
  MessagePortChannel local_port1;
  ConnectToSharedWorker(MakeSharedWorkerConnector(
                            renderer_host1, render_frame_host1->GetRoutingID()),
                        kUrl, kName, &client1, &local_port1);

  base::RunLoop().RunUntilIdle();

  // The previous worker is unavailable, so a new worker is created.

  mojom::SharedWorkerFactoryRequest factory_request1;
  EXPECT_TRUE(CheckReceivedFactoryRequest(&factory_request1));
  MockSharedWorkerFactory factory1(std::move(factory_request1));

  base::RunLoop().RunUntilIdle();

  mojom::SharedWorkerHostPtr worker_host1;
  mojom::SharedWorkerRequest worker_request1;
  EXPECT_TRUE(factory1.CheckReceivedCreateSharedWorker(
      kUrl, kName, blink::kWebContentSecurityPolicyTypeReport, &worker_host1,
      &worker_request1));
  MockSharedWorker worker1(std::move(worker_request1));

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

  EXPECT_TRUE(CheckNotReceivedFactoryRequest());

  EXPECT_TRUE(worker1.CheckReceivedConnect(nullptr, nullptr));
  EXPECT_TRUE(client2.CheckReceivedOnCreated());
}

TEST_F(SharedWorkerServiceImplTest, CreateWorkerRaceTest2) {
  const GURL kUrl("http://example.com/w.js");
  const char kName[] = "name";

  // Create three renderer hosts.

  std::unique_ptr<TestWebContents> web_contents0 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host0 = web_contents0->GetMainFrame();
  MockRenderProcessHost* renderer_host0 = render_frame_host0->GetProcess();
  renderer_host0->OverrideBinderForTesting(
      mojom::SharedWorkerFactory::Name_,
      base::Bind(&SharedWorkerServiceImplTest::BindSharedWorkerFactory));
  renderer_host0->OverrideURLLoaderFactory(url_loader_factory_.get());

  std::unique_ptr<TestWebContents> web_contents1 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host1 = web_contents1->GetMainFrame();
  MockRenderProcessHost* renderer_host1 = render_frame_host1->GetProcess();
  renderer_host1->OverrideBinderForTesting(
      mojom::SharedWorkerFactory::Name_,
      base::Bind(&SharedWorkerServiceImplTest::BindSharedWorkerFactory));
  renderer_host1->OverrideURLLoaderFactory(url_loader_factory_.get());

  std::unique_ptr<TestWebContents> web_contents2 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host2 = web_contents2->GetMainFrame();
  MockRenderProcessHost* renderer_host2 = render_frame_host2->GetProcess();
  renderer_host2->OverrideBinderForTesting(
      mojom::SharedWorkerFactory::Name_,
      base::Bind(&SharedWorkerServiceImplTest::BindSharedWorkerFactory));
  renderer_host2->OverrideURLLoaderFactory(url_loader_factory_.get());

  MockSharedWorkerClient client0;
  MessagePortChannel local_port0;
  ConnectToSharedWorker(MakeSharedWorkerConnector(
                            renderer_host0, render_frame_host0->GetRoutingID()),
                        kUrl, kName, &client0, &local_port0);

  // Kill this process, which should make worker0 unavailable.
  renderer_host0->FastShutdownIfPossible(0, true);

  // Start a new client, attemping to connect to the same worker.
  MockSharedWorkerClient client1;
  MessagePortChannel local_port1;
  ConnectToSharedWorker(MakeSharedWorkerConnector(
                            renderer_host1, render_frame_host1->GetRoutingID()),
                        kUrl, kName, &client1, &local_port1);

  base::RunLoop().RunUntilIdle();

  // The previous worker is unavailable, so a new worker is created.

  mojom::SharedWorkerFactoryRequest factory_request1;
  EXPECT_TRUE(CheckReceivedFactoryRequest(&factory_request1));
  MockSharedWorkerFactory factory1(std::move(factory_request1));

  EXPECT_TRUE(CheckNotReceivedFactoryRequest());

  base::RunLoop().RunUntilIdle();

  mojom::SharedWorkerHostPtr worker_host1;
  mojom::SharedWorkerRequest worker_request1;
  EXPECT_TRUE(factory1.CheckReceivedCreateSharedWorker(
      kUrl, kName, blink::kWebContentSecurityPolicyTypeReport, &worker_host1,
      &worker_request1));
  MockSharedWorker worker1(std::move(worker_request1));

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

  EXPECT_TRUE(CheckNotReceivedFactoryRequest());

  EXPECT_TRUE(worker1.CheckReceivedConnect(nullptr, nullptr));
  EXPECT_TRUE(client2.CheckReceivedOnCreated());
}

TEST_F(SharedWorkerServiceImplTest, CreateWorkerRaceTest3) {
  const GURL kURL("http://example.com/w.js");
  const char kName[] = "name";

  // The first renderer host.
  std::unique_ptr<TestWebContents> web_contents0 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host0 = web_contents0->GetMainFrame();
  MockRenderProcessHost* renderer_host0 = render_frame_host0->GetProcess();
  renderer_host0->OverrideBinderForTesting(
      mojom::SharedWorkerFactory::Name_,
      base::BindRepeating(
          &SharedWorkerServiceImplTest::BindSharedWorkerFactory));
  renderer_host0->OverrideURLLoaderFactory(url_loader_factory_.get());

  // The second renderer host.
  std::unique_ptr<TestWebContents> web_contents1 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host1 = web_contents1->GetMainFrame();
  MockRenderProcessHost* renderer_host1 = render_frame_host1->GetProcess();
  renderer_host1->OverrideBinderForTesting(
      mojom::SharedWorkerFactory::Name_,
      base::BindRepeating(
          &SharedWorkerServiceImplTest::BindSharedWorkerFactory));
  renderer_host1->OverrideURLLoaderFactory(url_loader_factory_.get());

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

  // Expect a factory request.
  mojom::SharedWorkerFactoryRequest factory_request;
  EXPECT_TRUE(CheckReceivedFactoryRequest(&factory_request));
  MockSharedWorkerFactory factory(std::move(factory_request));
  base::RunLoop().RunUntilIdle();

  // Expect a create shared worker.
  mojom::SharedWorkerHostPtr worker_host;
  mojom::SharedWorkerRequest worker_request;
  EXPECT_TRUE(factory.CheckReceivedCreateSharedWorker(
      kURL, kName, blink::kWebContentSecurityPolicyTypeReport, &worker_host,
      &worker_request));
  MockSharedWorker worker(std::move(worker_request));
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

}  // namespace content
