// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/camera3/cpp/fidl.h>
#include <fuchsia/legacymetrics/cpp/fidl.h>
#include <fuchsia/legacymetrics/cpp/fidl_test_base.h>
#include <fuchsia/media/cpp/fidl.h>
#include <fuchsia/modular/cpp/fidl.h>
#include <lib/fdio/directory.h>
#include <lib/fidl/cpp/binding.h>
#include <lib/sys/cpp/component_context.h>
#include <lib/ui/scenic/cpp/view_token_pair.h>
#include <lib/zx/channel.h>
#include <zircon/processargs.h>

#include "base/base_paths_fuchsia.h"
#include "base/callback_helpers.h"
#include "base/files/file_util.h"
#include "base/fuchsia/file_utils.h"
#include "base/fuchsia/filtered_service_directory.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/fuchsia/scoped_service_binding.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "build/build_config.h"
#include "fuchsia/base/agent_impl.h"
#include "fuchsia/base/context_provider_test_connector.h"
#include "fuchsia/base/fake_component_context.h"
#include "fuchsia/base/fit_adapter.h"
#include "fuchsia/base/frame_test_util.h"
#include "fuchsia/base/mem_buffer_util.h"
#include "fuchsia/base/result_receiver.h"
#include "fuchsia/base/string_util.h"
#include "fuchsia/base/test_devtools_list_fetcher.h"
#include "fuchsia/base/url_request_rewrite_test_util.h"
#include "fuchsia/runners/cast/cast_runner.h"
#include "fuchsia/runners/cast/cast_runner_switches.h"
#include "fuchsia/runners/cast/fake_application_config_manager.h"
#include "fuchsia/runners/cast/test_api_bindings.h"
#include "mojo/core/embedder/embedder.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/http_request.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kTestAppId[] = "00000000";
constexpr char kSecondTestAppId[] = "FFFFFFFF";

constexpr char kBlankAppUrl[] = "/defaultresponse";
constexpr char kEchoHeaderPath[] = "/echoheader?Test";

constexpr char kTestServerRoot[] = "fuchsia/runners/cast/testdata";

constexpr char kDummyAgentUrl[] =
    "fuchsia-pkg://fuchsia.com/dummy_agent#meta/dummy_agent.cmx";

class FakeUrlRequestRewriteRulesProvider
    : public chromium::cast::UrlRequestRewriteRulesProvider {
 public:
  FakeUrlRequestRewriteRulesProvider() = default;
  ~FakeUrlRequestRewriteRulesProvider() override = default;
  FakeUrlRequestRewriteRulesProvider(
      const FakeUrlRequestRewriteRulesProvider&) = delete;
  FakeUrlRequestRewriteRulesProvider& operator=(
      const FakeUrlRequestRewriteRulesProvider&) = delete;

 private:
  void GetUrlRequestRewriteRules(
      GetUrlRequestRewriteRulesCallback callback) override {
    // Only send the rules once. They do not expire
    if (rules_sent_)
      return;
    rules_sent_ = true;

    std::vector<fuchsia::web::UrlRequestRewrite> rewrites;
    rewrites.push_back(
        cr_fuchsia::CreateRewriteAddHeaders("Test", "TestHeaderValue"));
    fuchsia::web::UrlRequestRewriteRule rule;
    rule.set_rewrites(std::move(rewrites));
    std::vector<fuchsia::web::UrlRequestRewriteRule> rules;
    rules.push_back(std::move(rule));
    callback(std::move(rules));
  }

  bool rules_sent_ = false;
};

class FakeApplicationContext : public chromium::cast::ApplicationContext {
 public:
  FakeApplicationContext() = default;
  ~FakeApplicationContext() final = default;

  FakeApplicationContext(const FakeApplicationContext&) = delete;
  FakeApplicationContext& operator=(const FakeApplicationContext&) = delete;

  chromium::cast::ApplicationController* controller() {
    if (!controller_)
      return nullptr;

    return controller_.get();
  }

  base::Optional<int64_t> WaitForApplicationTerminated() {
    base::RunLoop loop;
    on_application_terminated_ = loop.QuitClosure();
    loop.Run();
    return application_exit_code_;
  }

 private:
  // chromium::cast::ApplicationContext implementation.
  void GetMediaSessionId(GetMediaSessionIdCallback callback) final {
    callback(0);
  }
  void SetApplicationController(
      fidl::InterfaceHandle<chromium::cast::ApplicationController> controller)
      final {
    controller_ = controller.Bind();
  }
  void OnApplicationExit(int64_t exit_code) final {
    application_exit_code_ = exit_code;
    if (on_application_terminated_)
      std::move(on_application_terminated_).Run();
  }

  chromium::cast::ApplicationControllerPtr controller_;

  base::Optional<int64_t> application_exit_code_;
  base::OnceClosure on_application_terminated_;
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
        context_binding_(outgoing_directory(), &application_context_) {
    if (url_request_rules_provider) {
      url_request_rules_provider_binding_.emplace(outgoing_directory(),
                                                  url_request_rules_provider);
    }
  }

  ~FakeComponentState() override {
    if (on_delete_)
      std::move(on_delete_).Run();
  }
  FakeComponentState(const FakeComponentState&) = delete;
  FakeComponentState& operator=(const FakeComponentState&) = delete;

  // Make outgoing_directory() public.
  using ComponentStateBase::outgoing_directory;

  FakeApplicationContext* application_context() {
    return &application_context_;
  }

  void set_on_delete(base::OnceClosure on_delete) {
    on_delete_ = std::move(on_delete);
  }

  void Disconnect() { DisconnectClientsAndTeardown(); }

  bool api_bindings_has_clients() {
    return bindings_manager_binding_.has_clients();
  }

  bool url_request_rules_provider_has_clients() {
    if (url_request_rules_provider_binding_) {
      return url_request_rules_provider_binding_->has_clients();
    }
    return false;
  }

 protected:
  const base::fuchsia::ScopedServiceBinding<
      chromium::cast::ApplicationConfigManager>
      app_config_binding_;
  const base::fuchsia::ScopedServiceBinding<chromium::cast::ApiBindings>
      bindings_manager_binding_;
  base::Optional<base::fuchsia::ScopedServiceBinding<
      chromium::cast::UrlRequestRewriteRulesProvider>>
      url_request_rules_provider_binding_;
  FakeApplicationContext application_context_;
  const base::fuchsia::ScopedServiceBinding<chromium::cast::ApplicationContext>
      context_binding_;
  base::OnceClosure on_delete_;
};

sys::ServiceDirectory StartCastRunner(
    fidl::InterfaceHandle<fuchsia::io::Directory> web_engine_host_directory,
    bool enable_headless,
    bool enable_vulkan,
    fidl::InterfaceRequest<fuchsia::sys::ComponentController>
        component_controller_request) {
  fuchsia::sys::LaunchInfo launch_info;
  launch_info.url =
      "fuchsia-pkg://fuchsia.com/cast_runner#meta/cast_runner.cmx";

  // Clone stderr from the current process to CastRunner and ask it to
  // redirect all logs to stderr.
  launch_info.err = fuchsia::sys::FileDescriptor::New();
  launch_info.err->type0 = PA_FD;
  zx_status_t status = fdio_fd_clone(
      STDERR_FILENO, launch_info.err->handle0.reset_and_get_address());
  ZX_CHECK(status == ZX_OK, status);
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII("enable-logging", "stderr");
  if (enable_headless)
    command_line.AppendSwitch(kForceHeadlessForTestsSwitch);
  if (!enable_vulkan)
    command_line.AppendSwitch(kDisableVulkanForTestsSwitch);
  launch_info.arguments.emplace(std::vector<std::string>(
      command_line.argv().begin() + 1, command_line.argv().end()));

  fidl::InterfaceHandle<fuchsia::io::Directory> cast_runner_services_dir;
  launch_info.directory_request =
      cast_runner_services_dir.NewRequest().TakeChannel();

  // Redirect ContextProvider to |web_engine_host_directory|.
  launch_info.additional_services =
      std::make_unique<fuchsia::sys::ServiceList>();
  launch_info.additional_services->host_directory =
      web_engine_host_directory.TakeChannel();
  launch_info.additional_services->names.push_back(
      fuchsia::web::ContextProvider::Name_);

  fuchsia::sys::LauncherPtr launcher;
  base::ComponentContextForProcess()->svc()->Connect(launcher.NewRequest());
  launcher->CreateComponent(std::move(launch_info),
                            std::move(component_controller_request));
  return sys::ServiceDirectory(std::move(cast_runner_services_dir));
}

}  // namespace

class CastRunnerIntegrationTest : public testing::Test {
 public:
  CastRunnerIntegrationTest()
      : CastRunnerIntegrationTest(/*enable_headless=*/false,
                                  /*enable_vulkan=*/false) {}
  CastRunnerIntegrationTest(const CastRunnerIntegrationTest&) = delete;
  CastRunnerIntegrationTest& operator=(const CastRunnerIntegrationTest&) =
      delete;

  void SetUp() override { mojo::core::Init(); }

  void TearDown() override {
    if (component_controller_)
      ShutdownComponent();
  }

 protected:
  explicit CastRunnerIntegrationTest(bool enable_headless, bool enable_vulkan)
      : app_config_manager_binding_(&component_services_,
                                    &app_config_manager_) {
    StartAndPublishWebEngine();

    // Start CastRunner.
    fidl::InterfaceHandle<::fuchsia::io::Directory> incoming_services;
    services_for_cast_runner_.GetOrCreateDirectory("svc")->Serve(
        ::fuchsia::io::OPEN_RIGHT_READABLE | ::fuchsia::io::OPEN_RIGHT_WRITABLE,
        incoming_services.NewRequest().TakeChannel());
    sys::ServiceDirectory cast_runner_services =
        StartCastRunner(std::move(incoming_services), enable_headless,
                        enable_vulkan, cast_runner_controller_.NewRequest());

    // Connect to the CastRunner's fuchsia.sys.Runner interface.
    cast_runner_ = cast_runner_services.Connect<fuchsia::sys::Runner>();
    cast_runner_.set_error_handler([](zx_status_t status) {
      ZX_LOG(ERROR, status) << "CastRunner closed channel.";
      ADD_FAILURE();
    });

    test_server_.ServeFilesFromSourceDirectory(kTestServerRoot);
    net::test_server::RegisterDefaultHandlers(&test_server_);
    EXPECT_TRUE(test_server_.Start());

    // Inject ApiBinding that used by ExecuteJavaScript().
    std::vector<chromium::cast::ApiBinding> binding_list;
    chromium::cast::ApiBinding eval_js_binding;
    eval_js_binding.set_before_load_script(cr_fuchsia::MemBufferFromString(
        "function valueOrUndefinedString(value) {"
        "    return (typeof(value) == 'undefined') ? 'undefined' : value;"
        "}"
        "window.addEventListener('DOMContentLoaded', (event) => {"
        "  var port = cast.__platform__.PortConnector.bind('testport');"
        "  port.onmessage = (e) => {"
        "    var result = eval(e.data);"
        "    if (result && typeof(result.then) == 'function') {"
        "      result"
        "        .then(result =>"
        "                port.postMessage(valueOrUndefinedString(result)))"
        "        .catch(e => port.postMessage(JSON.stringify(e)));"
        "    } else {"
        "      port.postMessage(valueOrUndefinedString(result));"
        "    }"
        "  };"
        "});",
        "test"));
    binding_list.emplace_back(std::move(eval_js_binding));
    api_bindings_.set_bindings(std::move(binding_list));
  }

  std::unique_ptr<cr_fuchsia::AgentImpl::ComponentStateBase> OnComponentConnect(
      base::StringPiece component_url) {
    auto component_state = std::make_unique<FakeComponentState>(
        component_url, &app_config_manager_, &api_bindings_,
        url_request_rewrite_rules_provider_.get());
    component_state_ = component_state.get();

    if (component_state_created_callback_)
      std::move(component_state_created_callback_).Run();

    return component_state;
  }

  void StartAndPublishWebEngine() {
    fidl::InterfaceHandle<fuchsia::io::Directory> web_engine_outgoing_dir =
        cr_fuchsia::StartWebEngineForTests(web_engine_controller_.NewRequest());
    sys::ServiceDirectory web_engine_outgoing_services(
        std::move(web_engine_outgoing_dir));

    services_for_cast_runner_
        .RemovePublicService<fuchsia::web::ContextProvider>();
    services_for_cast_runner_.AddPublicService(
        std::make_unique<vfs::Service>(
            [web_engine_outgoing_services =
                 std::move(web_engine_outgoing_services)](
                zx::channel channel, async_dispatcher_t* dispatcher) {
              web_engine_outgoing_services.Connect(
                  fuchsia::web::ContextProvider::Name_, std::move(channel));
            }),
        fuchsia::web::ContextProvider::Name_);
  }

  void RegisterAppWithTestData(GURL url) {
    fuchsia::web::ContentDirectoryProvider provider;
    provider.set_name("testdata");
    base::FilePath pkg_path;
    CHECK(base::PathService::Get(base::DIR_ASSETS, &pkg_path));
    provider.set_directory(base::fuchsia::OpenDirectory(
        pkg_path.AppendASCII("fuchsia/runners/cast/testdata")));
    std::vector<fuchsia::web::ContentDirectoryProvider> providers;
    providers.emplace_back(std::move(provider));

    auto app_config =
        FakeApplicationConfigManager::CreateConfig(kTestAppId, url);
    app_config.set_content_directories_for_isolated_application(
        std::move(providers));
    app_config_manager_.AddAppConfig(std::move(app_config));
  }

  void CreateComponentContextAndStartComponent(
      const char* app_id = kTestAppId) {
    auto component_url = base::StringPrintf("cast:%s", app_id);
    CreateComponentContext(component_url);
    StartCastComponent(component_url);
    WaitComponentState();
    WaitTestPort();
  }

  void CreateComponentContext(const base::StringPiece& component_url) {
    url_request_rewrite_rules_provider_ =
        std::make_unique<FakeUrlRequestRewriteRulesProvider>();
    component_context_ = std::make_unique<cr_fuchsia::FakeComponentContext>(
        &component_services_, component_url);
    component_context_->RegisterCreateComponentStateCallback(
        FakeApplicationConfigManager::kFakeAgentUrl,
        base::BindRepeating(&CastRunnerIntegrationTest::OnComponentConnect,
                            base::Unretained(this)));
  }

  void StartCastComponent(base::StringPiece component_url) {
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

    fidl::InterfaceHandle<fuchsia::io::Directory> svc_directory;
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

    cast_runner_->StartComponent(std::move(package), std::move(startup_info),
                                 component_controller_.NewRequest());
    component_controller_.set_error_handler([](zx_status_t status) {
      ZX_LOG(ERROR, status) << "Component launch failed";
      ADD_FAILURE();
    });
  }

  // Executes |code| in the context of the test application and then returns
  // the result serialized as string. If the code evaluates to a promise then
  // execution is blocked until the promise is complete and the result of the
  // promise is returned.
  std::string ExecuteJavaScript(const std::string& code) {
    fuchsia::web::WebMessage message;
    message.set_data(cr_fuchsia::MemBufferFromString(code, "test-msg"));
    test_port_->PostMessage(
        std::move(message),
        [](fuchsia::web::MessagePort_PostMessage_Result result) {
          EXPECT_TRUE(result.is_response());
        });

    base::RunLoop response_loop;
    cr_fuchsia::ResultReceiver<fuchsia::web::WebMessage> response(
        response_loop.QuitClosure());
    test_port_->ReceiveMessage(
        cr_fuchsia::CallbackToFitFunction(response.GetReceiveCallback()));
    response_loop.Run();

    std::string response_string;
    EXPECT_TRUE(
        cr_fuchsia::StringFromMemBuffer(response->data(), &response_string));

    return response_string;
  }

  void WaitComponentState() {
    base::RunLoop run_loop;
    component_state_created_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void WaitTestPort() {
    CHECK(!test_port_);
    test_port_ = api_bindings_.RunUntilMessagePortReceived("testport").Bind();
  }

  void CheckAppUrl(const GURL& app_url) {
    EXPECT_EQ(ExecuteJavaScript("window.location.href"), app_url.spec());
  }

  void ShutdownComponent() {
    DCHECK(component_controller_);

    base::RunLoop run_loop;
    component_state_->set_on_delete(run_loop.QuitClosure());
    component_controller_.Unbind();
    run_loop.Run();

    component_controller_ = nullptr;
  }

  void WaitForComponentDestroyed() {
    ASSERT_TRUE(component_state_);
    base::RunLoop state_loop;
    component_state_->set_on_delete(state_loop.QuitClosure());

    base::RunLoop controller_loop;
    if (component_controller_) {
      component_controller_.set_error_handler(
          [&controller_loop](zx_status_t status) {
            EXPECT_EQ(status, ZX_ERR_PEER_CLOSED);
            controller_loop.Quit();
          });
    }

    web_engine_controller_->Kill();

    if (component_controller_) {
      controller_loop.Run();
    }

    state_loop.Run();

    ResetComponentState();
  }

  void ResetComponentState() {
    component_context_ = nullptr;
    component_services_client_ = nullptr;
    component_state_ = nullptr;
    test_port_ = nullptr;
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
  net::EmbeddedTestServer test_server_;

  fuchsia::sys::ComponentControllerPtr web_engine_controller_;
  fuchsia::sys::ComponentControllerPtr cast_runner_controller_;

  FakeApplicationConfigManager app_config_manager_;
  TestApiBindings api_bindings_;
  std::unique_ptr<FakeUrlRequestRewriteRulesProvider>
      url_request_rewrite_rules_provider_;

  // Incoming service directory, ComponentContext and per-component state.
  sys::OutgoingDirectory component_services_;
  base::fuchsia::ScopedServiceBinding<chromium::cast::ApplicationConfigManager>
      app_config_manager_binding_;
  std::unique_ptr<cr_fuchsia::FakeComponentContext> component_context_;
  fuchsia::sys::ComponentControllerPtr component_controller_;
  std::unique_ptr<sys::ServiceDirectory> component_services_client_;
  FakeComponentState* component_state_ = nullptr;
  fuchsia::web::MessagePortPtr test_port_;

  base::OnceClosure component_state_created_callback_;

  // Directory used to publish test ContextProvider to CastRunner. Some tests
  // restart ContextProvider, so we can't pass the services directory from
  // ContextProvider to CastRunner directly.
  sys::OutgoingDirectory services_for_cast_runner_;

  fuchsia::sys::RunnerPtr cast_runner_;
};

// A basic integration test ensuring a basic cast request launches the right
// URL in the Chromium service.
TEST_F(CastRunnerIntegrationTest, BasicRequest) {
  GURL app_url = test_server_.GetURL(kBlankAppUrl);
  app_config_manager_.AddApp(kTestAppId, app_url);

  CreateComponentContextAndStartComponent();

  CheckAppUrl(app_url);
}

// Verify that the Runner can continue to be used even after its Context has
// crashed. Regression test for https://crbug.com/1066826).
// TODO(https://crbug.com/1066833): Make this a WebRunner test.
TEST_F(CastRunnerIntegrationTest, CanRecreateContext) {
  // Execute two iterations of launching the component and verifying that it
  // reaches the expected URL.
  for (int i = 0; i < 2; ++i) {
    SCOPED_TRACE(testing::Message() << "Test iteration " << i);

    const GURL app_url = test_server_.GetURL(kBlankAppUrl);
    app_config_manager_.AddApp(kTestAppId, app_url);

    CreateComponentContextAndStartComponent();

    CheckAppUrl(app_url);

    web_engine_controller_->Kill();

    // Wait for the component and the Context to be torn down.
    WaitForComponentDestroyed();

    // Start a new WebEngine instance for the next iteration.
    if (i < 1)
      StartAndPublishWebEngine();
  }
}

TEST_F(CastRunnerIntegrationTest, ApiBindings) {
  app_config_manager_.AddApp(kTestAppId, test_server_.GetURL(kBlankAppUrl));

  CreateComponentContextAndStartComponent();

  // Verify that we can communicate with the binding added in
  // CastRunnerIntegrationTest().
  EXPECT_EQ(ExecuteJavaScript("1+2+\"\""), "3");
}

TEST_F(CastRunnerIntegrationTest, IncorrectCastAppId) {
  const char kIncorrectComponentUrl[] = "cast:99999999";

  CreateComponentContext(kIncorrectComponentUrl);
  StartCastComponent(kIncorrectComponentUrl);

  // Run the loop until the ComponentController is dropped.
  base::RunLoop run_loop;
  component_controller_.set_error_handler([&run_loop](zx_status_t status) {
    EXPECT_EQ(status, ZX_ERR_PEER_CLOSED);
    run_loop.Quit();
  });
  run_loop.Run();

  EXPECT_TRUE(!component_state_);
}

TEST_F(CastRunnerIntegrationTest, UrlRequestRewriteRulesProvider) {
  GURL echo_app_url = test_server_.GetURL(kEchoHeaderPath);
  app_config_manager_.AddApp(kTestAppId, echo_app_url);

  CreateComponentContextAndStartComponent();

  CheckAppUrl(echo_app_url);

  EXPECT_EQ(ExecuteJavaScript("document.body.innerText"), "TestHeaderValue");
}

TEST_F(CastRunnerIntegrationTest, ApplicationControllerBound) {
  app_config_manager_.AddApp(kTestAppId, test_server_.GetURL(kBlankAppUrl));

  CreateComponentContextAndStartComponent();

  // Spin the message loop to handle creation of the component state.
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(component_state_);
  EXPECT_TRUE(component_state_->application_context()->controller());
}

// Verify an App launched with remote debugging enabled is properly reachable.
TEST_F(CastRunnerIntegrationTest, RemoteDebugging) {
  GURL app_url = test_server_.GetURL(kBlankAppUrl);
  auto app_config =
      FakeApplicationConfigManager::CreateConfig(kTestAppId, app_url);
  app_config.set_enable_remote_debugging(true);
  app_config_manager_.AddAppConfig(std::move(app_config));

  CreateComponentContextAndStartComponent();

  // Connect to the debug service and ensure we get the proper response.
  base::Value devtools_list =
      cr_fuchsia::GetDevToolsListFromPort(CastRunner::kRemoteDebuggingPort);
  ASSERT_TRUE(devtools_list.is_list());
  EXPECT_EQ(devtools_list.GetList().size(), 1u);

  base::Value* devtools_url = devtools_list.GetList()[0].FindPath("url");
  ASSERT_TRUE(devtools_url->is_string());
  EXPECT_EQ(devtools_url->GetString(), app_url.spec());
}

TEST_F(CastRunnerIntegrationTest, IsolatedContext) {
  const GURL kContentDirectoryUrl("fuchsia-dir://testdata/empty.html");

  RegisterAppWithTestData(kContentDirectoryUrl);
  CreateComponentContextAndStartComponent();
  CheckAppUrl(kContentDirectoryUrl);
}

// Test the lack of CastAgent service does not cause a CastRunner crash.
TEST_F(CastRunnerIntegrationTest, NoCastAgent) {
  app_config_manager_.AddApp(kTestAppId, test_server_.GetURL(kEchoHeaderPath));

  StartCastComponent(base::StringPrintf("cast:%s", kTestAppId));

  base::RunLoop run_loop;
  component_controller_.set_error_handler([&run_loop](zx_status_t error) {
    EXPECT_EQ(error, ZX_ERR_PEER_CLOSED);
    run_loop.Quit();
  });
  run_loop.Run();
}

// Test the CastAgent disconnecting does not cause a CastRunner crash.
TEST_F(CastRunnerIntegrationTest, DisconnectedCastAgent) {
  app_config_manager_.AddApp(kTestAppId, test_server_.GetURL(kEchoHeaderPath));

  CreateComponentContextAndStartComponent();

  base::RunLoop run_loop;
  component_controller_.set_error_handler([&run_loop](zx_status_t error) {
    EXPECT_EQ(error, ZX_ERR_PEER_CLOSED);
    run_loop.Quit();
  });

  // Tear down the ComponentState, this should close the Agent connection and
  // shut down the CastComponent.
  component_state_->Disconnect();

  run_loop.Run();
}

// Test that the ApiBindings and RewriteRules are received from the secondary
// DummyAgent. This validates that the |agent_url| retrieved from
// AppConfigManager is the one used to retrieve the bindings and the rewrite
// rules.
TEST_F(CastRunnerIntegrationTest, ApplicationConfigAgentUrl) {
  // These are part of the secondary agent, and CastRunner will contact
  // the secondary agent for both of them.
  FakeUrlRequestRewriteRulesProvider dummy_url_request_rewrite_rules_provider;
  TestApiBindings dummy_agent_api_bindings;

  // Indicate that this app is to get bindings from a secondary agent.
  auto app_config = FakeApplicationConfigManager::CreateConfig(
      kTestAppId, test_server_.GetURL(kBlankAppUrl));
  app_config.set_agent_url(kDummyAgentUrl);
  app_config_manager_.AddAppConfig(std::move(app_config));

  // Instantiate the bindings that are returned in the multi-agent scenario. The
  // bindings returned for the single-agent scenario are not initialized.
  std::vector<chromium::cast::ApiBinding> binding_list;
  chromium::cast::ApiBinding echo_binding;
  echo_binding.set_before_load_script(cr_fuchsia::MemBufferFromString(
      "window.echo = cast.__platform__.PortConnector.bind('dummyService');",
      "test"));
  binding_list.emplace_back(std::move(echo_binding));
  // Assign the bindings to the multi-agent binding.
  dummy_agent_api_bindings.set_bindings(std::move(binding_list));

  auto component_url = base::StringPrintf("cast:%s", kTestAppId);
  CreateComponentContext(component_url);
  EXPECT_NE(component_context_, nullptr);

  base::RunLoop run_loop;
  FakeComponentState* dummy_component_state = nullptr;
  component_context_->RegisterCreateComponentStateCallback(
      kDummyAgentUrl,
      base::BindLambdaForTesting(
          [&](base::StringPiece component_url)
              -> std::unique_ptr<cr_fuchsia::AgentImpl::ComponentStateBase> {
            run_loop.Quit();
            auto result = std::make_unique<FakeComponentState>(
                component_url, &app_config_manager_, &dummy_agent_api_bindings,
                &dummy_url_request_rewrite_rules_provider);
            dummy_component_state = result.get();
            return result;
          }));

  StartCastComponent(component_url);

  // Wait for the component state to be created.
  run_loop.Run();

  // Validate that the component state in the default agent wasn't crated.
  EXPECT_FALSE(component_state_);

  // Shutdown component before destroying dummy_agent_api_bindings.
  base::RunLoop shutdown_run_loop;
  dummy_component_state->set_on_delete(shutdown_run_loop.QuitClosure());
  component_controller_.Unbind();
  shutdown_run_loop.Run();
}

// Test that when RewriteRules are not provided, a WebComponent is still
// created. Further validate that the primary agent does not provide ApiBindings
// or RewriteRules.
TEST_F(CastRunnerIntegrationTest, ApplicationConfigAgentUrlRewriteOptional) {
  TestApiBindings dummy_agent_api_bindings;
  // Indicate that this app is to get bindings from a secondary agent.
  auto app_config = FakeApplicationConfigManager::CreateConfig(
      kTestAppId, test_server_.GetURL(kBlankAppUrl));
  app_config.set_agent_url(kDummyAgentUrl);
  app_config_manager_.AddAppConfig(std::move(app_config));

  // Instantiate the bindings that are returned in the multi-agent scenario. The
  // bindings returned for the single-agent scenario are not initialized.
  std::vector<chromium::cast::ApiBinding> binding_list;
  chromium::cast::ApiBinding echo_binding;
  echo_binding.set_before_load_script(cr_fuchsia::MemBufferFromString(
      "window.echo = cast.__platform__.PortConnector.bind('dummyService');",
      "test"));
  binding_list.emplace_back(std::move(echo_binding));
  // Assign the bindings to the multi-agent binding.
  dummy_agent_api_bindings.set_bindings(std::move(binding_list));

  auto component_url = base::StringPrintf("cast:%s", kTestAppId);
  CreateComponentContext(component_url);
  base::RunLoop run_loop;
  FakeComponentState* dummy_component_state = nullptr;
  component_context_->RegisterCreateComponentStateCallback(
      kDummyAgentUrl,
      base::BindLambdaForTesting(
          [&](base::StringPiece component_url)
              -> std::unique_ptr<cr_fuchsia::AgentImpl::ComponentStateBase> {
            run_loop.Quit();
            auto result = std::make_unique<FakeComponentState>(
                component_url, &app_config_manager_, &dummy_agent_api_bindings,
                nullptr);
            dummy_component_state = result.get();
            return result;
          }));

  StartCastComponent(component_url);

  // Wait for the component state to be created.
  run_loop.Run();

  // Validate that the component state in the default agent wasn't crated.
  EXPECT_FALSE(component_state_);

  // Shutdown component before destroying dummy_agent_api_bindings.
  base::RunLoop shutdown_run_loop;
  dummy_component_state->set_on_delete(shutdown_run_loop.QuitClosure());
  component_controller_.Unbind();
  shutdown_run_loop.Run();
}

TEST_F(CastRunnerIntegrationTest, MicrophoneRedirect) {
  GURL app_url = test_server_.GetURL("/microphone.html");
  auto app_config =
      FakeApplicationConfigManager::CreateConfig(kTestAppId, app_url);

  fuchsia::web::PermissionDescriptor mic_permission;
  mic_permission.set_type(fuchsia::web::PermissionType::MICROPHONE);
  app_config.mutable_permissions()->push_back(std::move(mic_permission));
  app_config_manager_.AddAppConfig(std::move(app_config));

  CreateComponentContextAndStartComponent();

  // Expect fuchsia.media.Audio connection to be redirected to the agent.
  base::RunLoop run_loop;
  component_state_->outgoing_directory()->AddPublicService(
      std::make_unique<vfs::Service>(
          [quit_closure = run_loop.QuitClosure()](
              zx::channel channel, async_dispatcher_t* dispatcher) mutable {
            std::move(quit_closure).Run();
          }),
      fuchsia::media::Audio::Name_);

  ExecuteJavaScript("connectMicrophone();");

  // Will quit once AudioCapturer is connected.
  run_loop.Run();
}

TEST_F(CastRunnerIntegrationTest, CameraRedirect) {
  GURL app_url = test_server_.GetURL("/camera.html");
  auto app_config =
      FakeApplicationConfigManager::CreateConfig(kTestAppId, app_url);

  fuchsia::web::PermissionDescriptor camera_permission;
  camera_permission.set_type(fuchsia::web::PermissionType::CAMERA);
  app_config.mutable_permissions()->push_back(std::move(camera_permission));
  app_config_manager_.AddAppConfig(std::move(app_config));

  CreateComponentContextAndStartComponent();

  // Expect fuchsia.camera3.DeviceWatcher connection to be redirected to the
  // agent.
  bool received_device_watcher_request = false;
  component_state_->outgoing_directory()->AddPublicService(
      std::make_unique<vfs::Service>(
          [&received_device_watcher_request](
              zx::channel channel, async_dispatcher_t* dispatcher) mutable {
            received_device_watcher_request = true;
          }),
      fuchsia::camera3::DeviceWatcher::Name_);

  ExecuteJavaScript("connectCamera();");
  EXPECT_TRUE(received_device_watcher_request);
}

TEST_F(CastRunnerIntegrationTest, CameraAccessAfterComponentShutdown) {
  GURL app_url = test_server_.GetURL("/camera.html");

  // First app with camera permission.
  auto app_config =
      FakeApplicationConfigManager::CreateConfig(kTestAppId, app_url);
  fuchsia::web::PermissionDescriptor camera_permission;
  camera_permission.set_type(fuchsia::web::PermissionType::CAMERA);
  app_config.mutable_permissions()->push_back(std::move(camera_permission));
  app_config_manager_.AddAppConfig(std::move(app_config));

  // Second app without camera permission (but it will still try to access
  // fuchsia.camera3.DeviceWatcher service to enumerate devices).
  auto app_config_2 =
      FakeApplicationConfigManager::CreateConfig(kSecondTestAppId, app_url);
  app_config_manager_.AddAppConfig(std::move(app_config_2));

  // Start and then shutdown the first app.
  CreateComponentContextAndStartComponent(kTestAppId);
  ShutdownComponent();
  ResetComponentState();

  // Start the second app and try to connect the camera. It's expected to fail
  // to initialize the camera without crashing CastRunner.
  CreateComponentContextAndStartComponent(kSecondTestAppId);
  EXPECT_EQ(ExecuteJavaScript("connectCamera();"), "getUserMediaFailed");
}

class HeadlessCastRunnerIntegrationTest : public CastRunnerIntegrationTest {
 public:
  HeadlessCastRunnerIntegrationTest()
      : CastRunnerIntegrationTest(/*enable_headless=*/true,
                                  /*enable_vulkan=*/false) {}
};

// A basic integration test ensuring a basic cast request launches the right
// URL in the Chromium service.
TEST_F(HeadlessCastRunnerIntegrationTest, Headless) {
  const char kAnimationPath[] = "/css_animation.html";
  const GURL animation_url = test_server_.GetURL(kAnimationPath);
  app_config_manager_.AddApp(kTestAppId, animation_url);

  CreateComponentContextAndStartComponent();
  auto tokens = scenic::ViewTokenPair::New();

  // Create a view.
  auto view_provider =
      component_services_client_->Connect<fuchsia::ui::app::ViewProvider>();
  view_provider->CreateView(std::move(tokens.view_holder_token.value), {}, {});

  api_bindings_.RunUntilMessagePortReceived("animation_finished");

  // Verify that dropped "view" EventPair is handled properly.
  tokens.view_token.value.reset();
  api_bindings_.RunUntilMessagePortReceived("view_hidden");
}

// Isolated *and* headless? Doesn't sound like much fun!
TEST_F(HeadlessCastRunnerIntegrationTest, IsolatedAndHeadless) {
  const GURL kContentDirectoryUrl("fuchsia-dir://testdata/empty.html");

  RegisterAppWithTestData(kContentDirectoryUrl);
  CreateComponentContextAndStartComponent();
  CheckAppUrl(kContentDirectoryUrl);
}

// Verifies that the Context can establish a connection to the Agent's
// MetricsRecorder service.
TEST_F(CastRunnerIntegrationTest, LegacyMetricsRedirect) {
  GURL app_url = test_server_.GetURL(kBlankAppUrl);
  app_config_manager_.AddApp(kTestAppId, app_url);

  auto component_url = base::StringPrintf("cast:%s", kTestAppId);
  CreateComponentContext(component_url);
  EXPECT_NE(component_context_, nullptr);

  base::RunLoop run_loop;

  // Add MetricsRecorder the the component's incoming_services.
  component_services_.AddPublicService(
      std::make_unique<vfs::Service>(
          [&run_loop](zx::channel request, async_dispatcher_t* dispatcher) {
            run_loop.Quit();
          }),
      fuchsia::legacymetrics::MetricsRecorder::Name_);

  StartCastComponent(component_url);

  // Wait until we see the CastRunner connect to the MetricsRecorder service.
  run_loop.Run();
}

// Verifies that the ApplicationContext::OnApplicationTerminated() is notified
// with the component exit code if the web content closes itself.
TEST_F(CastRunnerIntegrationTest, OnApplicationTerminated_WindowClose) {
  const GURL url = test_server_.GetURL(kBlankAppUrl);
  app_config_manager_.AddApp(kTestAppId, url);

  CreateComponentContextAndStartComponent();

  // It is possible to observe the ComponentController close before
  // OnApplicationTerminated() is received, so ignore that.
  component_controller_.set_error_handler([](zx_status_t) {});

  // Have the web content close itself, and wait for OnApplicationTerminated().
  EXPECT_EQ(ExecuteJavaScript("window.close()"), "undefined");
  base::Optional<zx_status_t> exit_code =
      component_state_->application_context()->WaitForApplicationTerminated();
  ASSERT_TRUE(exit_code);
  EXPECT_EQ(exit_code.value(), ZX_OK);
}

// Verifies that the ComponentController reports TerminationReason::EXITED and
// exit code ZX_OK if the web content terminates itself.
// TODO(https://crbug.com/1066833): Make this a WebRunner test.
TEST_F(CastRunnerIntegrationTest, OnTerminated_WindowClose) {
  const GURL url = test_server_.GetURL(kBlankAppUrl);
  app_config_manager_.AddApp(kTestAppId, url);

  CreateComponentContextAndStartComponent();

  // Register an handler on the ComponentController channel, for the
  // OnTerminated event.
  base::RunLoop exit_code_loop;
  component_controller_.set_error_handler(
      [quit_loop = exit_code_loop.QuitClosure()](zx_status_t) {
        quit_loop.Run();
        ADD_FAILURE();
      });
  component_controller_.events().OnTerminated =
      [quit_loop = exit_code_loop.QuitClosure()](
          int64_t exit_code, fuchsia::sys::TerminationReason reason) {
        quit_loop.Run();
        EXPECT_EQ(reason, fuchsia::sys::TerminationReason::EXITED);
        EXPECT_EQ(exit_code, ZX_OK);
      };

  // Have the web content close itself, and wait for OnTerminated().
  EXPECT_EQ(ExecuteJavaScript("window.close()"), "undefined");
  exit_code_loop.Run();

  component_controller_.Unbind();
}

// Verifies that the ComponentController reports TerminationReason::EXITED and
// exit code ZX_OK if Kill() is used.
// TODO(https://crbug.com/1066833): Make this a WebRunner test.
TEST_F(CastRunnerIntegrationTest, OnTerminated_ComponentKill) {
  const GURL url = test_server_.GetURL(kBlankAppUrl);
  app_config_manager_.AddApp(kTestAppId, url);

  CreateComponentContextAndStartComponent();

  // Register an handler on the ComponentController channel, for the
  // OnTerminated event.
  base::RunLoop exit_code_loop;
  component_controller_.set_error_handler(
      [quit_loop = exit_code_loop.QuitClosure()](zx_status_t) {
        quit_loop.Run();
        ADD_FAILURE();
      });
  component_controller_.events().OnTerminated =
      [quit_loop = exit_code_loop.QuitClosure()](
          int64_t exit_code, fuchsia::sys::TerminationReason reason) {
        quit_loop.Run();
        EXPECT_EQ(reason, fuchsia::sys::TerminationReason::EXITED);
        EXPECT_EQ(exit_code, ZX_OK);
      };

  // Kill() the component and wait for OnTerminated().
  component_controller_->Kill();
  exit_code_loop.Run();

  component_controller_.Unbind();
}

TEST_F(CastRunnerIntegrationTest, WebGLContextAbsentWithoutVulkanFeature) {
  const char kTestPath[] = "/webgl_presence.html";
  const GURL test_url = test_server_.GetURL(kTestPath);
  app_config_manager_.AddApp(kTestAppId, test_url);

  CreateComponentContextAndStartComponent();

  EXPECT_EQ(ExecuteJavaScript("document.title"), "absent");
}

TEST_F(CastRunnerIntegrationTest,
       WebGLContextAbsentWithoutVulkanFeature_IsolatedRunner) {
  const GURL kContentDirectoryUrl("fuchsia-dir://testdata/webgl_presence.html");

  RegisterAppWithTestData(kContentDirectoryUrl);
  CreateComponentContextAndStartComponent();
  CheckAppUrl(kContentDirectoryUrl);

  EXPECT_EQ(ExecuteJavaScript("document.title"), "absent");
}

#if defined(ARCH_CPU_ARM_FAMILY)
// TODO(crbug.com/1058247): Support Vulkan in tests on ARM64.
#define MAYBE_VulkanCastRunnerIntegrationTest \
  DISABLED_VulkanCastRunnerIntegrationTest
#else
#define MAYBE_VulkanCastRunnerIntegrationTest VulkanCastRunnerIntegrationTest
#endif

class MAYBE_VulkanCastRunnerIntegrationTest : public CastRunnerIntegrationTest {
 public:
  MAYBE_VulkanCastRunnerIntegrationTest()
      : CastRunnerIntegrationTest(/*enable_headless=*/false,
                                  /*enable_vulkan=*/true) {}
};

TEST_F(MAYBE_VulkanCastRunnerIntegrationTest,
       WebGLContextPresentWithVulkanFeature) {
  const char kTestPath[] = "/webgl_presence.html";
  const GURL test_url = test_server_.GetURL(kTestPath);
  app_config_manager_.AddApp(kTestAppId, test_url);

  CreateComponentContextAndStartComponent();

  EXPECT_EQ(ExecuteJavaScript("document.title"), "present");
}

TEST_F(MAYBE_VulkanCastRunnerIntegrationTest,
       WebGLContextPresentWithVulkanFeature_IsolatedRunner) {
  const GURL kContentDirectoryUrl("fuchsia-dir://testdata/webgl_presence.html");

  RegisterAppWithTestData(kContentDirectoryUrl);
  CreateComponentContextAndStartComponent();
  CheckAppUrl(kContentDirectoryUrl);

  EXPECT_EQ(ExecuteJavaScript("document.title"), "present");
}
