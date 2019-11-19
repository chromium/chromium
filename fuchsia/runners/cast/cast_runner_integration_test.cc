// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/modular/cpp/fidl.h>
#include <lib/fdio/directory.h>
#include <lib/fidl/cpp/binding.h>
#include <lib/sys/cpp/component_context.h>
#include <lib/zx/channel.h>

#include "base/base_paths_fuchsia.h"
#include "base/callback_helpers.h"
#include "base/fuchsia/file_utils.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/scoped_service_binding.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "fuchsia/base/agent_impl.h"
#include "fuchsia/base/fake_component_context.h"
#include "fuchsia/base/fit_adapter.h"
#include "fuchsia/base/frame_test_util.h"
#include "fuchsia/base/mem_buffer_util.h"
#include "fuchsia/base/result_receiver.h"
#include "fuchsia/base/string_util.h"
#include "fuchsia/base/test_devtools_list_fetcher.h"
#include "fuchsia/base/test_navigation_listener.h"
#include "fuchsia/base/url_request_rewrite_test_util.h"
#include "fuchsia/runners/cast/cast_runner.h"
#include "fuchsia/runners/cast/fake_application_config_manager.h"
#include "fuchsia/runners/cast/test_api_bindings.h"
#include "fuchsia/runners/common/web_component.h"
#include "fuchsia/runners/common/web_content_runner.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/http_request.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace castrunner {

namespace {

const char kTestServerRoot[] =
    FILE_PATH_LITERAL("fuchsia/runners/cast/testdata");

void ComponentErrorHandler(zx_status_t status) {
  ZX_LOG(ERROR, status) << "Component launch failed";
  ADD_FAILURE();
}

class FakeUrlRequestRewriteRulesProvider
    : public chromium::cast::UrlRequestRewriteRulesProvider {
 public:
  FakeUrlRequestRewriteRulesProvider() = default;
  ~FakeUrlRequestRewriteRulesProvider() override = default;

 private:
  void GetUrlRequestRewriteRules(
      GetUrlRequestRewriteRulesCallback callback) override {
    // Only send the rules once. They do not expire
    if (rules_sent_)
      return;
    rules_sent_ = true;

    std::vector<fuchsia::web::UrlRequestRewrite> rewrites;
    rewrites.push_back(cr_fuchsia::CreateRewriteAddHeaders("Test", "Value"));
    fuchsia::web::UrlRequestRewriteRule rule;
    rule.set_rewrites(std::move(rewrites));
    std::vector<fuchsia::web::UrlRequestRewriteRule> rules;
    rules.push_back(std::move(rule));
    callback(std::move(rules));
  }

  bool rules_sent_ = false;

  DISALLOW_COPY_AND_ASSIGN(FakeUrlRequestRewriteRulesProvider);
};

class FakeApplicationControllerReceiver
    : public chromium::cast::ApplicationControllerReceiver {
 public:
  FakeApplicationControllerReceiver() = default;
  ~FakeApplicationControllerReceiver() final = default;

  chromium::cast::ApplicationController* controller() {
    if (!controller_)
      return nullptr;

    return controller_.get();
  }

 private:
  // chromium::cast::ApplicationControllerReceiver implementation.
  void SetApplicationController(
      fidl::InterfaceHandle<chromium::cast::ApplicationController> controller)
      final {
    controller_ = controller.Bind();
  }

  chromium::cast::ApplicationControllerPtr controller_;

  DISALLOW_COPY_AND_ASSIGN(FakeApplicationControllerReceiver);
};

class FakeComponentState : public cr_fuchsia::AgentImpl::ComponentStateBase {
 public:
  FakeComponentState(
      base::StringPiece component_url,
      chromium::cast::ApplicationConfigManager* app_config_manager,
      chromium::cast::ApiBindings* bindings_manager,
      chromium::cast::UrlRequestRewriteRulesProvider*
          url_request_rules_provider)
      : ComponentStateBase(component_url),
        app_config_binding_(outgoing_directory(), app_config_manager),
        bindings_manager_binding_(outgoing_directory(), bindings_manager),
        url_request_rules_provider_binding_(outgoing_directory(),
                                            url_request_rules_provider),
        controller_receiver_binding_(outgoing_directory(),
                                     &controller_receiver_) {}

  ~FakeComponentState() override {
    if (on_delete_)
      std::move(on_delete_).Run();
  }

  FakeApplicationControllerReceiver* controller_receiver() {
    return &controller_receiver_;
  }

  void set_on_delete(base::OnceClosure on_delete) {
    on_delete_ = std::move(on_delete);
  }

  void Disconnect() { DisconnectClientsAndTeardown(); }

 protected:
  const base::fuchsia::ScopedServiceBinding<
      chromium::cast::ApplicationConfigManager>
      app_config_binding_;
  const base::fuchsia::ScopedServiceBinding<chromium::cast::ApiBindings>
      bindings_manager_binding_;
  const base::fuchsia::ScopedServiceBinding<
      chromium::cast::UrlRequestRewriteRulesProvider>
      url_request_rules_provider_binding_;
  FakeApplicationControllerReceiver controller_receiver_;
  const base::fuchsia::ScopedServiceBinding<
      chromium::cast::ApplicationControllerReceiver>
      controller_receiver_binding_;
  base::OnceClosure on_delete_;

  DISALLOW_COPY_AND_ASSIGN(FakeComponentState);
};

}  // namespace

class CastRunnerIntegrationTest : public testing::Test {
 public:
  CastRunnerIntegrationTest()
      : run_timeout_(
            TestTimeouts::action_timeout(),
            base::MakeExpectedNotRunClosure(FROM_HERE, "Run() timed out.")) {
    // Create the CastRunner, published into |outgoing_directory_|.
    constexpr fuchsia::web::ContextFeatureFlags kFeatures = {
        fuchsia::web::ContextFeatureFlags::NETWORK};
    fuchsia::web::CreateContextParams create_context_params =
        WebContentRunner::BuildCreateContextParams(
            fidl::InterfaceHandle<fuchsia::io::Directory>(), kFeatures);
    const uint16_t kRemoteDebuggingAnyPort = 0;
    create_context_params.set_remote_debugging_port(kRemoteDebuggingAnyPort);
    cast_runner_ = std::make_unique<CastRunner>(
        &outgoing_directory_, std::move(create_context_params));

    // Connect to the CastRunner's fuchsia.sys.Runner interface.
    fidl::InterfaceHandle<fuchsia::io::Directory> directory;
    outgoing_directory_.GetOrCreateDirectory("svc")->Serve(
        fuchsia::io::OPEN_RIGHT_READABLE | fuchsia::io::OPEN_RIGHT_WRITABLE,
        directory.NewRequest().TakeChannel());
    sys::ServiceDirectory public_directory_client(std::move(directory));
    cast_runner_ptr_ = public_directory_client.Connect<fuchsia::sys::Runner>();
    cast_runner_ptr_.set_error_handler([](zx_status_t status) {
      ZX_LOG(ERROR, status) << "CastRunner closed channel.";
      ADD_FAILURE();
    });
  }

  void SetUp() override {
    test_server_.ServeFilesFromSourceDirectory(kTestServerRoot);
    net::test_server::RegisterDefaultHandlers(&test_server_);
    ASSERT_TRUE(test_server_.Start());
  }

  void TearDown() override {
    // Disconnect the CastRunner & let things tear-down.
    cast_runner_ptr_.Unbind();
    base::RunLoop().RunUntilIdle();
  }

  fuchsia::sys::ComponentControllerPtr StartCastComponent(
      base::StringPiece component_url,
      bool start_component_context) {
    DCHECK(!component_state_);

    if (start_component_context) {
      // Create a FakeComponentContext and publish it into component_services_.
      component_context_ = std::make_unique<cr_fuchsia::FakeComponentContext>(
          base::BindRepeating(&CastRunnerIntegrationTest::OnComponentConnect,
                              base::Unretained(this)),
          &component_services_, component_url);
    }

    // Configure the Runner, including a service directory channel to publish
    // services to.
    fidl::InterfaceHandle<fuchsia::io::Directory> directory;
    component_services_.GetOrCreateDirectory("svc")->Serve(
        fuchsia::io::OPEN_RIGHT_READABLE | fuchsia::io::OPEN_RIGHT_WRITABLE,
        directory.NewRequest().TakeChannel());
    fuchsia::sys::StartupInfo startup_info;
    startup_info.launch_info.url = component_url.as_string();

    fidl::InterfaceHandle<fuchsia::io::Directory> outgoing_directory;
    startup_info.launch_info.directory_request =
        outgoing_directory.NewRequest().TakeChannel();

    fidl::InterfaceHandle<::fuchsia::io::Directory> svc_directory;
    CHECK_EQ(fdio_service_connect_at(
                 outgoing_directory.channel().get(), "svc",
                 svc_directory.NewRequest().TakeChannel().release()),
             ZX_OK);

    component_services_client_ =
        std::make_unique<sys::ServiceDirectory>(std::move(svc_directory));

    // Place the ServiceDirectory in the |flat_namespace|.
    startup_info.flat_namespace.paths.emplace_back(
        base::fuchsia::kServiceDirectoryPath);
    startup_info.flat_namespace.directories.emplace_back(
        directory.TakeChannel());

    fuchsia::sys::Package package;
    package.resolved_url = component_url.as_string();

    fuchsia::sys::ComponentControllerPtr component_controller;
    cast_runner_ptr_->StartComponent(std::move(package),
                                     std::move(startup_info),
                                     component_controller.NewRequest());

    EXPECT_TRUE(component_controller.is_bound());
    return component_controller;
  }

 protected:
  std::unique_ptr<cr_fuchsia::AgentImpl::ComponentStateBase> OnComponentConnect(
      base::StringPiece component_url) {
    auto component_state = std::make_unique<FakeComponentState>(
        component_url, &app_config_manager_, &api_bindings_,
        &url_request_rewrite_rules_provider_);
    component_state_ = component_state.get();
    return component_state;
  }

  const base::RunLoop::ScopedRunTimeoutForTest run_timeout_;
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
  net::EmbeddedTestServer test_server_;

  FakeApplicationConfigManager app_config_manager_;
  TestApiBindings api_bindings_;
  FakeUrlRequestRewriteRulesProvider url_request_rewrite_rules_provider_;

  // Incoming service directory, ComponentContext and per-component state.
  sys::OutgoingDirectory component_services_;
  std::unique_ptr<cr_fuchsia::FakeComponentContext> component_context_;
  std::unique_ptr<sys::ServiceDirectory> component_services_client_;
  FakeComponentState* component_state_ = nullptr;

  // ServiceDirectory into which the CastRunner will publish itself.
  sys::OutgoingDirectory outgoing_directory_;

  std::unique_ptr<CastRunner> cast_runner_;
  fuchsia::sys::RunnerPtr cast_runner_ptr_;

  DISALLOW_COPY_AND_ASSIGN(CastRunnerIntegrationTest);
};

// A basic integration test ensuring a basic cast request launches the right
// URL in the Chromium service.
TEST_F(CastRunnerIntegrationTest, BasicRequest) {
  const char kBlankAppId[] = "00000000";
  const char kBlankAppPath[] = "/defaultresponse";
  app_config_manager_.AddAppMapping(kBlankAppId,
                                    test_server_.GetURL(kBlankAppPath), false);

  // Launch the test-app component.
  fuchsia::sys::ComponentControllerPtr component_controller =
      StartCastComponent(base::StringPrintf("cast:%s", kBlankAppId), true);
  component_controller.set_error_handler(&ComponentErrorHandler);

  // Access the NavigationController from the WebComponent. The test will hang
  // here if no WebComponent was created.
  fuchsia::web::NavigationControllerPtr nav_controller;
  {
    base::RunLoop run_loop;
    cr_fuchsia::ResultReceiver<WebComponent*> web_component(
        run_loop.QuitClosure());
    cast_runner_->SetWebComponentCreatedCallbackForTest(
        base::AdaptCallbackForRepeating(web_component.GetReceiveCallback()));
    run_loop.Run();
    ASSERT_NE(*web_component, nullptr);
    (*web_component)
        ->frame()
        ->GetNavigationController(nav_controller.NewRequest());
  }

  // Ensure the NavigationState has the expected URL.
  {
    base::RunLoop run_loop;
    cr_fuchsia::ResultReceiver<fuchsia::web::NavigationState> nav_entry(
        run_loop.QuitClosure());
    nav_controller->GetVisibleEntry(
        cr_fuchsia::CallbackToFitFunction(nav_entry.GetReceiveCallback()));
    run_loop.Run();
    ASSERT_TRUE(nav_entry->has_url());
    EXPECT_EQ(nav_entry->url(), test_server_.GetURL(kBlankAppPath).spec());
  }

  // Verify that the component is torn down when |component_controller| is
  // unbound.
  base::RunLoop run_loop;
  component_state_->set_on_delete(run_loop.QuitClosure());
  component_controller.Unbind();
  run_loop.Run();
}

TEST_F(CastRunnerIntegrationTest, ApiBindings) {
  const char kBlankAppId[] = "00000000";
  const char kBlankAppPath[] = "/echo.html";
  app_config_manager_.AddAppMapping(kBlankAppId,
                                    test_server_.GetURL(kBlankAppPath), false);

  std::vector<chromium::cast::ApiBinding> binding_list;
  chromium::cast::ApiBinding echo_binding;
  echo_binding.set_before_load_script(cr_fuchsia::MemBufferFromString(
      "window.echo = cast.__platform__.PortConnector.bind('echoService');",
      "test"));
  binding_list.emplace_back(std::move(echo_binding));
  api_bindings_.set_bindings(std::move(binding_list));

  // Launch the test-app component.
  fuchsia::sys::ComponentControllerPtr component_controller =
      StartCastComponent(base::StringPrintf("cast:%s", kBlankAppId), true);
  component_controller.set_error_handler(&ComponentErrorHandler);

  fuchsia::web::MessagePortPtr port =
      api_bindings_.RunUntilMessagePortReceived("echoService").Bind();

  fuchsia::web::WebMessage message;
  message.set_data(cr_fuchsia::MemBufferFromString("ping", "ping-msg"));
  port->PostMessage(std::move(message),
                    [](fuchsia::web::MessagePort_PostMessage_Result result) {
                      EXPECT_TRUE(result.is_response());
                    });

  base::RunLoop response_loop;
  cr_fuchsia::ResultReceiver<fuchsia::web::WebMessage> response(
      response_loop.QuitClosure());
  port->ReceiveMessage(
      cr_fuchsia::CallbackToFitFunction(response.GetReceiveCallback()));
  response_loop.Run();

  std::string response_string;
  EXPECT_TRUE(
      cr_fuchsia::StringFromMemBuffer(response->data(), &response_string));
  EXPECT_EQ("ack ping", response_string);
}

TEST_F(CastRunnerIntegrationTest, IncorrectCastAppId) {
  // Launch the a component with an invalid Cast app Id.
  fuchsia::sys::ComponentControllerPtr component_controller =
      StartCastComponent("cast:99999999", true);
  component_controller.set_error_handler(&ComponentErrorHandler);

  // Run the loop until the ComponentController is dropped, or a WebComponent is
  // created.
  base::RunLoop run_loop;
  component_controller.set_error_handler([&run_loop](zx_status_t status) {
    EXPECT_EQ(status, ZX_ERR_PEER_CLOSED);
    run_loop.Quit();
  });
  cr_fuchsia::ResultReceiver<WebComponent*> web_component(
      run_loop.QuitClosure());
  cast_runner_->SetWebComponentCreatedCallbackForTest(
      AdaptCallbackForRepeating(web_component.GetReceiveCallback()));
  run_loop.Run();
  EXPECT_FALSE(web_component.has_value());
}

TEST_F(CastRunnerIntegrationTest, UrlRequestRewriteRulesProvider) {
  const char kEchoAppId[] = "00000000";
  const char kEchoAppPath[] = "/echoheader?Test";
  const GURL echo_app_url = test_server_.GetURL(kEchoAppPath);
  app_config_manager_.AddAppMapping(kEchoAppId, echo_app_url, false);

  // Launch the test-app component.
  fuchsia::sys::ComponentControllerPtr component_controller =
      StartCastComponent(base::StringPrintf("cast:%s", kEchoAppId), true);
  component_controller.set_error_handler(&ComponentErrorHandler);

  WebComponent* web_component = nullptr;
  {
    base::RunLoop run_loop;
    cr_fuchsia::ResultReceiver<WebComponent*> web_component_receiver(
        run_loop.QuitClosure());
    cast_runner_->SetWebComponentCreatedCallbackForTest(
        AdaptCallbackForRepeating(web_component_receiver.GetReceiveCallback()));
    run_loop.Run();
    ASSERT_NE(*web_component_receiver, nullptr);
    web_component = *web_component_receiver;
  }

  // Bind a TestNavigationListener to the Frame.
  cr_fuchsia::TestNavigationListener navigation_listener;
  fidl::Binding<fuchsia::web::NavigationEventListener>
      navigation_listener_binding(&navigation_listener);
  web_component->frame()->SetNavigationEventListener(
      navigation_listener_binding.NewBinding());
  navigation_listener.RunUntilUrlEquals(echo_app_url);

  // Check the header was properly set.
  base::Optional<base::Value> result = cr_fuchsia::ExecuteJavaScript(
      web_component->frame(), "document.body.innerText");
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_string());
  EXPECT_EQ(result->GetString(), "Value");
}

TEST_F(CastRunnerIntegrationTest, ApplicationControllerBound) {
  const char kCastChannelAppId[] = "00000001";
  const char kCastChannelAppPath[] = "/defaultresponse";
  app_config_manager_.AddAppMapping(
      kCastChannelAppId, test_server_.GetURL(kCastChannelAppPath), false);

  fuchsia::sys::ComponentControllerPtr component_controller =
      StartCastComponent(base::StringPrintf("cast:%s", kCastChannelAppId),
                         true);

  // Spin the message loop to handle creation of the component state.
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(component_state_);
  EXPECT_TRUE(component_state_->controller_receiver()->controller());
}

// Verify an App launched with remote debugging enabled is properly reachable.
TEST_F(CastRunnerIntegrationTest, RemoteDebugging) {
  const char kBlankAppId[] = "00000000";
  const char kBlankAppPath[] = "/defaultresponse";
  const GURL kBlankAppUrl = test_server_.GetURL(kBlankAppPath);

  app_config_manager_.AddAppMapping(kBlankAppId, kBlankAppUrl, true);

  // Launch the test-app component.
  fuchsia::sys::ComponentControllerPtr component_controller =
      StartCastComponent(base::StringPrintf("cast:%s", kBlankAppId), true);
  component_controller.set_error_handler(&ComponentErrorHandler);

  // Get the remote debugging port from the Context.
  uint16_t remote_debugging_port = 0;
  {
    base::RunLoop run_loop;
    cr_fuchsia::ResultReceiver<
        fuchsia::web::Context_GetRemoteDebuggingPort_Result>
        port_receiver(run_loop.QuitClosure());
    cast_runner_->GetContext()->GetRemoteDebuggingPort(
        cr_fuchsia::CallbackToFitFunction(port_receiver.GetReceiveCallback()));
    run_loop.Run();

    ASSERT_TRUE(port_receiver->is_response());
    remote_debugging_port = port_receiver->response().port;
    ASSERT_TRUE(remote_debugging_port != 0);
  }

  // Connect to the debug service and ensure we get the proper response.
  base::Value devtools_list =
      cr_fuchsia::GetDevToolsListFromPort(remote_debugging_port);
  ASSERT_TRUE(devtools_list.is_list());
  EXPECT_EQ(devtools_list.GetList().size(), 1u);

  base::Value* devtools_url = devtools_list.GetList()[0].FindPath("url");
  ASSERT_TRUE(devtools_url->is_string());
  EXPECT_EQ(devtools_url->GetString(), kBlankAppUrl.spec());
}

TEST_F(CastRunnerIntegrationTest, IsolatedContext) {
  const char kBlankAppId[] = "00000000";
  const GURL kContentDirectoryUrl("fuchsia-dir://testdata/echo.html");

  EXPECT_EQ(cast_runner_->GetChildCastRunnerCountForTest(), 0u);

  fuchsia::web::ContentDirectoryProvider provider;
  provider.set_name("testdata");
  base::FilePath pkg_path;
  CHECK(base::PathService::Get(base::DIR_ASSETS, &pkg_path));
  provider.set_directory(base::fuchsia::OpenDirectory(
      pkg_path.AppendASCII("fuchsia/runners/cast/testdata")));
  std::vector<fuchsia::web::ContentDirectoryProvider> providers;
  providers.emplace_back(std::move(provider));
  app_config_manager_.AddAppMappingWithContentDirectories(
      kBlankAppId, kContentDirectoryUrl, std::move(providers));

  // Launch the test-app component.
  fuchsia::sys::ComponentControllerPtr component_controller =
      StartCastComponent(base::StringPrintf("cast:%s", kBlankAppId), true);
  component_controller.set_error_handler(&ComponentErrorHandler);

  // Navigate to the page and verify that we read it.
  {
    base::RunLoop run_loop;
    cr_fuchsia::ResultReceiver<WebComponent*> web_component(
        run_loop.QuitClosure());
    cast_runner_->SetWebComponentCreatedCallbackForTest(
        AdaptCallbackForRepeating(web_component.GetReceiveCallback()));
    run_loop.Run();
    ASSERT_NE(*web_component, nullptr);

    EXPECT_EQ(cast_runner_->GetChildCastRunnerCountForTest(), 1u);

    cr_fuchsia::TestNavigationListener listener;
    fidl::Binding<fuchsia::web::NavigationEventListener> listener_binding(
        &listener);
    (*web_component)
        ->frame()
        ->SetNavigationEventListener(listener_binding.NewBinding());
    listener.RunUntilUrlAndTitleEquals(kContentDirectoryUrl, "echo");
  }

  // Verify that the component is torn down when |component_controller| is
  // unbound.
  base::RunLoop run_loop;
  component_state_->set_on_delete(run_loop.QuitClosure());
  component_controller.Unbind();
  run_loop.Run();

  EXPECT_EQ(cast_runner_->GetChildCastRunnerCountForTest(), 0u);
}

// Test the lack of CastAgent service does not cause a CastRunner crash.
TEST_F(CastRunnerIntegrationTest, NoCastAgent) {
  const char kEchoAppId[] = "00000000";
  const char kEchoAppPath[] = "/echoheader?Test";
  const GURL echo_app_url = test_server_.GetURL(kEchoAppPath);
  app_config_manager_.AddAppMapping(kEchoAppId, echo_app_url, false);

  // Launch the test-app component.
  fuchsia::sys::ComponentControllerPtr component_controller =
      StartCastComponent(base::StringPrintf("cast:%s", kEchoAppId), false);

  base::RunLoop run_loop;
  component_controller.set_error_handler([&run_loop](zx_status_t error) {
    EXPECT_EQ(error, ZX_ERR_PEER_CLOSED);
    run_loop.Quit();
  });
  run_loop.Run();
}

// Test the CastAgent disconnecting does not cause a CastRunner crash.
TEST_F(CastRunnerIntegrationTest, DisconnectedCastAgent) {
  const char kEchoAppId[] = "00000000";
  const char kEchoAppPath[] = "/echoheader?Test";
  const GURL echo_app_url = test_server_.GetURL(kEchoAppPath);
  app_config_manager_.AddAppMapping(kEchoAppId, echo_app_url, false);

  // Launch the test-app component.
  fuchsia::sys::ComponentControllerPtr component_controller =
      StartCastComponent(base::StringPrintf("cast:%s", kEchoAppId), true);

  // Access the NavigationController from the WebComponent. The test will hang
  // here if no WebComponent was created.
  fuchsia::web::NavigationControllerPtr nav_controller;
  {
    base::RunLoop run_loop;
    cr_fuchsia::ResultReceiver<WebComponent*> web_component(
        run_loop.QuitClosure());
    cast_runner_->SetWebComponentCreatedCallbackForTest(
        base::AdaptCallbackForRepeating(web_component.GetReceiveCallback()));
    run_loop.Run();
    ASSERT_NE(*web_component, nullptr);
    (*web_component)
        ->frame()
        ->GetNavigationController(nav_controller.NewRequest());
  }

  base::RunLoop run_loop;
  component_controller.set_error_handler([&run_loop](zx_status_t error) {
    EXPECT_EQ(error, ZX_ERR_PEER_CLOSED);
    run_loop.Quit();
  });

  // Tear down the ComponentState, this should close the Agent connection and
  // shut down the CastComponent.
  component_state_->Disconnect();

  run_loop.Run();
}

}  // namespace castrunner
