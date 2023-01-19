// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/camera3/cpp/fidl.h>
#include <fuchsia/legacymetrics/cpp/fidl.h>
#include <fuchsia/media/cpp/fidl.h>
#include <fuchsia/modular/cpp/fidl.h>
#include <fuchsia/web/cpp/fidl.h>
#include <lib/fdio/directory.h>
#include <lib/fidl/cpp/binding.h>
#include <lib/sys/cpp/component_context.h>
#include <lib/ui/scenic/cpp/view_ref_pair.h>
#include <lib/ui/scenic/cpp/view_token_pair.h>
#include <lib/zx/channel.h>
#include <zircon/processargs.h>

#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/barrier_closure.h"
#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/fuchsia/file_utils.h"
#include "base/fuchsia/filtered_service_directory.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/mem_buffer_util.h"
#include "base/fuchsia/scoped_service_binding.h"
#include "base/fuchsia/test_component_controller.h"
#include "base/functional/callback_helpers.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "fuchsia_web/common/string_util.h"
#include "fuchsia_web/common/test/fit_adapter.h"
#include "fuchsia_web/common/test/frame_test_util.h"
#include "fuchsia_web/common/test/test_devtools_list_fetcher.h"
#include "fuchsia_web/common/test/url_request_rewrite_test_util.h"
#include "fuchsia_web/runners/cast/cast_runner.h"
#include "fuchsia_web/runners/cast/cast_runner_integration_test_base.h"
#include "fuchsia_web/runners/cast/cast_runner_switches.h"
#include "fuchsia_web/runners/cast/fake_api_bindings.h"
#include "fuchsia_web/runners/cast/fake_application_config_manager.h"
#include "fuchsia_web/runners/cast/fidl/fidl/chromium/cast/cpp/fidl.h"
#include "fuchsia_web/runners/common/modular/agent_impl.h"
#include "fuchsia_web/runners/common/modular/fake_component_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kTestAppId[] = "00000000";
constexpr char kSecondTestAppId[] = "FFFFFFFF";

constexpr char kBlankAppUrl[] = "/defaultresponse";
constexpr char kEchoHeaderPath[] = "/echoheader?Test";

constexpr char kDummyAgentUrl[] =
    "fuchsia-pkg://fuchsia.com/dummy_agent#meta/dummy_agent.cmx";

chromium::cast::ApplicationConfig CreateAppConfigWithTestData(
    base::StringPiece app_id,
    GURL url) {
  fuchsia::web::ContentDirectoryProvider provider;
  provider.set_name("testdata");

  base::FilePath pkg_path;
  CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &pkg_path));

  provider.set_directory(base::OpenDirectoryHandle(
      pkg_path.AppendASCII("fuchsia_web/runners/cast/testdata")));
  std::vector<fuchsia::web::ContentDirectoryProvider> providers;
  providers.emplace_back(std::move(provider));

  auto app_config = FakeApplicationConfigManager::CreateConfig(app_id, url);
  app_config.set_content_directories_for_isolated_application(
      std::move(providers));
  return app_config;
}

class FakeUrlRequestRewriteRulesProvider final
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
    rewrites.push_back(CreateRewriteAddHeaders("Test", "TestHeaderValue"));
    fuchsia::web::UrlRequestRewriteRule rule;
    rule.set_rewrites(std::move(rewrites));
    std::vector<fuchsia::web::UrlRequestRewriteRule> rules;
    rules.push_back(std::move(rule));
    callback(std::move(rules));
  }

  bool rules_sent_ = false;
};

class FakeApplicationContext final : public chromium::cast::ApplicationContext {
 public:
  FakeApplicationContext() = default;
  ~FakeApplicationContext() override = default;

  FakeApplicationContext(const FakeApplicationContext&) = delete;
  FakeApplicationContext& operator=(const FakeApplicationContext&) = delete;

  chromium::cast::ApplicationController* controller() {
    if (!controller_)
      return nullptr;

    return controller_.get();
  }

  absl::optional<int64_t> WaitForApplicationTerminated() {
    base::RunLoop loop;
    on_application_terminated_ = loop.QuitClosure();
    loop.Run();
    return application_exit_code_;
  }

 private:
  // chromium::cast::ApplicationContext implementation.
  void GetMediaSessionId(GetMediaSessionIdCallback callback) override {
    callback(1);
  }
  void SetApplicationController(
      fidl::InterfaceHandle<chromium::cast::ApplicationController> controller)
      override {
    controller_ = controller.Bind();
  }
  void OnApplicationExit(int64_t exit_code) override {
    application_exit_code_ = exit_code;
    if (on_application_terminated_)
      std::move(on_application_terminated_).Run();
  }

  chromium::cast::ApplicationControllerPtr controller_;

  absl::optional<int64_t> application_exit_code_;
  base::OnceClosure on_application_terminated_;
};

class FakeComponentState : public cr_fuchsia::AgentImpl::ComponentStateBase {
 public:
  FakeComponentState(base::StringPiece component_url,
                     chromium::cast::ApiBindings* bindings_manager,
                     chromium::cast::UrlRequestRewriteRulesProvider*
                         url_request_rules_provider)
      : ComponentStateBase(component_url),
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
  const base::ScopedServiceBinding<chromium::cast::ApiBindings>
      bindings_manager_binding_;
  absl::optional<base::ScopedServiceBinding<
      chromium::cast::UrlRequestRewriteRulesProvider>>
      url_request_rules_provider_binding_;

  FakeApplicationContext application_context_;
  const base::ScopedServiceBinding<chromium::cast::ApplicationContext>
      context_binding_;
  base::OnceClosure on_delete_;
};

class TestCastComponent {
 public:
  explicit TestCastComponent(fuchsia::sys::RunnerPtr& cast_runner)
      : cast_runner_(cast_runner.get()) {
    EXPECT_TRUE(cast_runner_);
  }

  ~TestCastComponent() {
    if (component_controller_)
      ShutdownComponent();
  }

  TestCastComponent(const TestCastComponent&) = delete;
  TestCastComponent& operator=(const FakeComponentState&) = delete;

  void CreateComponentContextAndStartComponent(
      base::StringPiece app_id = kTestAppId) {
    ASSERT_FALSE(component_context_)
        << "ComponentContext may only be created once";
    auto component_url = base::StrCat({"cast:", app_id});
    InjectQueryApi();
    CreateComponentContext(component_url);
    StartCastComponent(component_url);
    WaitComponentStateCreated();
    WaitQueryApiConnected();
  }

  void CreateComponentContext(const base::StringPiece& component_url,
                              bool with_fake_agent = true) {
    ASSERT_FALSE(component_context_)
        << "ComponentContext may only be created once";
    url_request_rewrite_rules_provider_ =
        std::make_unique<FakeUrlRequestRewriteRulesProvider>();
    component_context_ = std::make_unique<cr_fuchsia::FakeComponentContext>(
        &component_services_, component_url);
    if (with_fake_agent) {
      component_context_->RegisterCreateComponentStateCallback(
          FakeApplicationConfigManager::kFakeAgentUrl,
          base::BindRepeating(&TestCastComponent::OnComponentConnect,
                              base::Unretained(this)));
    }
  }

  void StartCastComponent(base::StringPiece component_url) {
    ASSERT_FALSE(component_services_client_)
        << "Component may only be started once";

    // Configure the Runner, including a service directory channel to publish
    // services to.
    fuchsia::sys::StartupInfo startup_info;
    startup_info.launch_info.url = std::string(component_url);

    fidl::InterfaceHandle<fuchsia::io::Directory> outgoing_directory;
    startup_info.launch_info.directory_request =
        outgoing_directory.NewRequest();

    fidl::InterfaceHandle<fuchsia::io::Directory> svc_directory;
    EXPECT_EQ(fdio_service_connect_at(
                  outgoing_directory.channel().get(), "svc",
                  svc_directory.NewRequest().TakeChannel().release()),
              ZX_OK);

    component_services_client_ =
        std::make_unique<sys::ServiceDirectory>(std::move(svc_directory));

    // Populate |component_services_| with services for the component to use.
    fidl::InterfaceHandle<fuchsia::io::Directory> directory;
    component_services_.GetOrCreateDirectory("svc")->Serve(
        fuchsia::io::OpenFlags::RIGHT_READABLE |
            fuchsia::io::OpenFlags::RIGHT_WRITABLE,
        directory.NewRequest().TakeChannel());

    // Provide the directory of services in the |flat_namespace|.
    startup_info.flat_namespace.paths.emplace_back(base::kServiceDirectoryPath);
    startup_info.flat_namespace.directories.emplace_back(
        directory.TakeChannel());

    fuchsia::sys::Package package;
    package.resolved_url = std::string(component_url);

    cast_runner_->StartComponent(std::move(package), std::move(startup_info),
                                 component_controller_.ptr().NewRequest());
    component_controller_.ptr().set_error_handler([](zx_status_t status) {
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
    message.set_data(base::MemBufferFromString(code, "test-msg"));
    test_port_->PostMessage(
        std::move(message),
        [](fuchsia::web::MessagePort_PostMessage_Result result) {
          EXPECT_TRUE(result.is_response());
        });

    base::test::TestFuture<fuchsia::web::WebMessage> response;
    test_port_->ReceiveMessage(CallbackToFitFunction(response.GetCallback()));
    EXPECT_TRUE(response.Wait());

    absl::optional<std::string> response_string =
        base::StringFromMemBuffer(response.Get().data());
    EXPECT_TRUE(response_string.has_value());

    return response_string.value_or(std::string());
  }

  void CheckAppUrl(const GURL& app_url) {
    EXPECT_EQ(ExecuteJavaScript("window.location.href"), app_url.spec());
  }

  void ShutdownComponent() {
    EXPECT_TRUE(component_controller_);

    if (component_state_) {
      base::RunLoop run_loop;
      component_state_->set_on_delete(run_loop.QuitClosure());
      component_controller_.ptr().Unbind();
      run_loop.Run();
    }
  }

  void ExpectControllerDisconnectWithStatus(zx_status_t expected_status) {
    EXPECT_TRUE(component_controller_);

    base::RunLoop loop;
    component_controller_.ptr().set_error_handler(
        [&loop, expected_status](zx_status_t status) {
          loop.Quit();
          EXPECT_EQ(expected_status, status);
        });

    loop.Run();
  }

  void WaitForComponentDestroyed() {
    ASSERT_TRUE(component_state_);
    base::RunLoop state_loop;
    component_state_->set_on_delete(state_loop.QuitClosure());

    if (component_controller_)
      ExpectControllerDisconnectWithStatus(ZX_ERR_PEER_CLOSED);

    state_loop.Run();

    ResetComponentState();
  }

  void ResetComponentState() {
    component_context_ = nullptr;
    component_services_client_ = nullptr;
    component_state_ = nullptr;
    test_port_ = nullptr;
  }

  void OnComponentStateCreated(base::OnceClosure callback) {
    ASSERT_FALSE(component_state_created_callback_);
    component_state_created_callback_ = std::move(callback);
  }

  FakeApiBindingsImpl* api_bindings() { return &api_bindings_; }
  cr_fuchsia::FakeComponentContext* component_context() {
    return component_context_.get();
  }
  base::TestComponentController* component_controller() {
    return &component_controller_;
  }
  sys::OutgoingDirectory* component_services() { return &component_services_; }
  sys::ServiceDirectory* component_services_client() {
    return component_services_client_.get();
  }
  FakeComponentState* component_state() { return component_state_; }

 private:
  void InjectQueryApi() {
    // Inject an API which can be used to evaluate arbitrary Javascript and
    // return the results over a MessagePort.
    std::vector<chromium::cast::ApiBinding> binding_list;
    chromium::cast::ApiBinding eval_js_binding;
    eval_js_binding.set_before_load_script(base::MemBufferFromString(
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

  void WaitQueryApiConnected() {
    EXPECT_FALSE(test_port_);
    test_port_ = api_bindings_.RunAndReturnConnectedPort("testport").Bind();
  }

  void WaitComponentStateCreated() {
    base::RunLoop run_loop;
    base::OnceClosure old_component_state_created_callback =
        std::move(component_state_created_callback_);
    component_state_created_callback_ = base::BindLambdaForTesting([&] {
      if (old_component_state_created_callback) {
        std::move(old_component_state_created_callback).Run();
      }
      run_loop.Quit();
    });
    run_loop.Run();
  }

  std::unique_ptr<cr_fuchsia::AgentImpl::ComponentStateBase> OnComponentConnect(
      base::StringPiece component_url) {
    auto component_state = std::make_unique<FakeComponentState>(
        component_url, &api_bindings_,
        url_request_rewrite_rules_provider_.get());
    component_state_ = component_state.get();

    if (component_state_created_callback_)
      std::move(component_state_created_callback_).Run();

    return component_state;
  }

  FakeApiBindingsImpl api_bindings_;
  std::unique_ptr<FakeUrlRequestRewriteRulesProvider>
      url_request_rewrite_rules_provider_;

  // Incoming service directory, ComponentContext and per-component state.
  sys::OutgoingDirectory component_services_;
  std::unique_ptr<cr_fuchsia::FakeComponentContext> component_context_;
  base::TestComponentController component_controller_;
  std::unique_ptr<sys::ServiceDirectory> component_services_client_;
  FakeComponentState* component_state_ = nullptr;
  fuchsia::web::MessagePortPtr test_port_;

  base::OnceClosure component_state_created_callback_;
  fuchsia::sys::Runner* const cast_runner_;
};

}  // namespace

// A basic integration test ensuring a basic cast request launches the right
// URL in the Chromium service.
TEST_F(CastRunnerIntegrationTest, BasicRequest) {
  TestCastComponent component(cast_runner());

  GURL app_url = test_server().GetURL(kBlankAppUrl);
  app_config_manager().AddApp(kTestAppId, app_url);
  component.CreateComponentContextAndStartComponent();

  component.CheckAppUrl(app_url);
}

// Verify that the Runner can continue to be used even after its Context has
// crashed. Regression test for https://crbug.com/1066826.
// TODO(crbug.com/1066833): Replace this with a WebRunner test, ideally a
//   unit-test, which can simulate Context disconnection more simply.
// TODO(crbug.com/1010222): Once CastRunner migrates to creating the WebEngine
//   component directly, it should be possible to rehabilitate and re-enable
//   this test. At present it is not straightforward to terminate the
//   WebEngine component instance, only the ContextProvider, which will not
//   result in the WebEngine instance being torn-down.
TEST_F(CastRunnerIntegrationTest, DISABLED_CanRecreateContext) {
  TestCastComponent component(cast_runner());
  const GURL app_url = test_server().GetURL(kBlankAppUrl);
  app_config_manager().AddApp(kTestAppId, app_url);

  // Create a Cast component and verify that it has loaded.
  component.CreateComponentContextAndStartComponent(kTestAppId);
  component.CheckAppUrl(app_url);

  // Terminate the component that provides the ContextProvider service and
  // wait for the Cast component to terminate, without allowing the message-
  // loop to spin in-between.
  component.WaitForComponentDestroyed();

  // Create a second Cast component and verify that it has loaded.
  // There is no guarantee that the CastRunner has detected the old web.Context
  // disconnecting yet, so attempts to launch Cast components could fail.
  // WebContentRunner::CreateFrameWithParams() will synchronously verify that
  // the web.Context is not-yet-closed, to work-around that.
  TestCastComponent second_component(cast_runner());
  app_config_manager().AddApp(kTestAppId, app_url);
  second_component.CreateComponentContextAndStartComponent(kTestAppId);
  second_component.CheckAppUrl(app_url);
}

TEST_F(CastRunnerIntegrationTest, ApiBindings) {
  TestCastComponent component(cast_runner());
  app_config_manager().AddApp(kTestAppId, test_server().GetURL(kBlankAppUrl));

  component.CreateComponentContextAndStartComponent();

  // Verify that we can communicate with the binding added in
  // CastRunnerIntegrationTest().
  EXPECT_EQ(component.ExecuteJavaScript("1+2+\"\""), "3");
}

TEST_F(CastRunnerIntegrationTest, IncorrectCastAppId) {
  TestCastComponent component(cast_runner());
  const char kIncorrectComponentUrl[] = "cast:99999999";

  component.CreateComponentContext(kIncorrectComponentUrl);
  component.StartCastComponent(kIncorrectComponentUrl);

  // Run the loop until the ComponentController is dropped.
  component.ExpectControllerDisconnectWithStatus(ZX_ERR_PEER_CLOSED);

  EXPECT_TRUE(!component.component_state());
}

TEST_F(CastRunnerIntegrationTest, UrlRequestRewriteRulesProvider) {
  TestCastComponent component(cast_runner());
  GURL echo_app_url = test_server().GetURL(kEchoHeaderPath);
  app_config_manager().AddApp(kTestAppId, echo_app_url);

  component.CreateComponentContextAndStartComponent();

  component.CheckAppUrl(echo_app_url);

  EXPECT_EQ(component.ExecuteJavaScript("document.body.innerText"),
            "TestHeaderValue");
}

TEST_F(CastRunnerIntegrationTest, ApplicationControllerBound) {
  TestCastComponent component(cast_runner());
  app_config_manager().AddApp(kTestAppId, test_server().GetURL(kBlankAppUrl));

  component.CreateComponentContextAndStartComponent();

  // Spin the message loop to handle creation of the component state.
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(component.component_state());
  EXPECT_TRUE(component.component_state()->application_context()->controller());
}

// Verify an App launched with remote debugging enabled is properly reachable.
TEST_F(CastRunnerIntegrationTest, RemoteDebugging) {
  TestCastComponent component(cast_runner());
  GURL app_url = test_server().GetURL(kBlankAppUrl);
  auto app_config =
      FakeApplicationConfigManager::CreateConfig(kTestAppId, app_url);
  app_config.set_enable_remote_debugging(true);
  app_config_manager().AddAppConfig(std::move(app_config));

  component.CreateComponentContextAndStartComponent();

  // Connect to the debug service and ensure we get the proper response.
  base::Value::List devtools_list =
      GetDevToolsListFromPort(CastRunner::kRemoteDebuggingPort);
  EXPECT_EQ(devtools_list.size(), 1u);

  base::Value* devtools_url = devtools_list[0].FindPath("url");
  ASSERT_TRUE(devtools_url->is_string());
  EXPECT_EQ(devtools_url->GetString(), app_url.spec());
}

TEST_F(CastRunnerIntegrationTest, IsolatedContext) {
  TestCastComponent component(cast_runner());
  const GURL kContentDirectoryUrl("fuchsia-dir://testdata/empty.html");

  app_config_manager().AddAppConfig(
      CreateAppConfigWithTestData(kTestAppId, kContentDirectoryUrl));
  component.CreateComponentContextAndStartComponent();
  component.CheckAppUrl(kContentDirectoryUrl);
}

// Test the lack of CastAgent service does not cause a CastRunner crash.
TEST_F(CastRunnerIntegrationTest, NoCastAgent) {
  TestCastComponent component(cast_runner());
  app_config_manager().AddApp(kTestAppId,
                              test_server().GetURL(kEchoHeaderPath));

  component.StartCastComponent(base::StrCat({"cast:", kTestAppId}));

  component.ExpectControllerDisconnectWithStatus(ZX_ERR_PEER_CLOSED);
}

// Test the CastAgent disconnecting does not cause a CastRunner crash.
TEST_F(CastRunnerIntegrationTest, DisconnectedCastAgent) {
  TestCastComponent component(cast_runner());
  app_config_manager().AddApp(kTestAppId,
                              test_server().GetURL(kEchoHeaderPath));

  component.CreateComponentContextAndStartComponent();

  // Tear down the ComponentState, this should close the Agent connection and
  // shut down the CastComponent.
  component.component_state()->Disconnect();

  component.ExpectControllerDisconnectWithStatus(ZX_ERR_PEER_CLOSED);
}

// Test that the ApiBindings and RewriteRules are received from the secondary
// DummyAgent. This validates that the |agent_url| retrieved from
// AppConfigManager is the one used to retrieve the bindings and the rewrite
// rules.
TEST_F(CastRunnerIntegrationTest, ApplicationConfigAgentUrl) {
  TestCastComponent component(cast_runner());

  // These are part of the secondary agent, and CastRunner will contact
  // the secondary agent for both of them.
  FakeUrlRequestRewriteRulesProvider dummy_url_request_rewrite_rules_provider;
  FakeApiBindingsImpl dummy_agent_api_bindings;

  // Indicate that this app is to get bindings from a secondary agent.
  auto app_config = FakeApplicationConfigManager::CreateConfig(
      kTestAppId, test_server().GetURL(kBlankAppUrl));
  app_config.set_agent_url(kDummyAgentUrl);
  app_config_manager().AddAppConfig(std::move(app_config));

  // Instantiate the bindings that are returned in the multi-agent scenario. The
  // bindings returned for the single-agent scenario are not initialized.
  std::vector<chromium::cast::ApiBinding> binding_list;
  chromium::cast::ApiBinding echo_binding;
  echo_binding.set_before_load_script(base::MemBufferFromString(
      "window.echo = cast.__platform__.PortConnector.bind('dummyService');",
      "test"));
  binding_list.emplace_back(std::move(echo_binding));
  // Assign the bindings to the multi-agent binding.
  dummy_agent_api_bindings.set_bindings(std::move(binding_list));

  auto component_url = base::StrCat({"cast:", kTestAppId});
  component.CreateComponentContext(component_url, /*with_fake_agent=*/false);
  EXPECT_TRUE(component.component_context());

  base::RunLoop run_loop;
  FakeComponentState* dummy_component_state = nullptr;
  component.component_context()->RegisterCreateComponentStateCallback(
      kDummyAgentUrl,
      base::BindLambdaForTesting(
          [&](base::StringPiece component_url)
              -> std::unique_ptr<cr_fuchsia::AgentImpl::ComponentStateBase> {
            run_loop.Quit();
            auto result = std::make_unique<FakeComponentState>(
                component_url, &dummy_agent_api_bindings,
                &dummy_url_request_rewrite_rules_provider);
            dummy_component_state = result.get();
            return result;
          }));

  component.StartCastComponent(component_url);

  // Wait for the component state to be created.
  run_loop.Run();

  // Validate that the component state in the default agent wasn't crated.
  EXPECT_FALSE(component.component_state());

  // Shutdown component before destroying dummy_agent_api_bindings.
  base::RunLoop shutdown_run_loop;
  dummy_component_state->set_on_delete(shutdown_run_loop.QuitClosure());
  component.component_controller()->ptr().Unbind();
  shutdown_run_loop.Run();
}

// Test that when RewriteRules are not provided, a WebComponent is still
// created. Further validate that the primary agent does not provide ApiBindings
// or RewriteRules.
TEST_F(CastRunnerIntegrationTest, ApplicationConfigAgentUrlRewriteOptional) {
  TestCastComponent component(cast_runner());
  FakeApiBindingsImpl dummy_agent_api_bindings;

  // Indicate that this app is to get bindings from a secondary agent.
  auto app_config = FakeApplicationConfigManager::CreateConfig(
      kTestAppId, test_server().GetURL(kBlankAppUrl));
  app_config.set_agent_url(kDummyAgentUrl);
  app_config_manager().AddAppConfig(std::move(app_config));

  // Instantiate the bindings that are returned in the multi-agent scenario. The
  // bindings returned for the single-agent scenario are not initialized.
  std::vector<chromium::cast::ApiBinding> binding_list;
  chromium::cast::ApiBinding echo_binding;
  echo_binding.set_before_load_script(base::MemBufferFromString(
      "window.echo = cast.__platform__.PortConnector.bind('dummyService');",
      "test"));
  binding_list.emplace_back(std::move(echo_binding));
  // Assign the bindings to the multi-agent binding.
  dummy_agent_api_bindings.set_bindings(std::move(binding_list));

  auto component_url = base::StrCat({"cast:", kTestAppId});
  component.CreateComponentContext(component_url, /*with_fake_agent=*/false);
  base::RunLoop run_loop;
  FakeComponentState* dummy_component_state = nullptr;
  component.component_context()->RegisterCreateComponentStateCallback(
      kDummyAgentUrl,
      base::BindLambdaForTesting(
          [&](base::StringPiece component_url)
              -> std::unique_ptr<cr_fuchsia::AgentImpl::ComponentStateBase> {
            run_loop.Quit();
            auto result = std::make_unique<FakeComponentState>(
                component_url, &dummy_agent_api_bindings, nullptr);
            dummy_component_state = result.get();
            return result;
          }));

  component.StartCastComponent(component_url);

  // Wait for the component state to be created.
  run_loop.Run();

  // Validate that the component state in the default agent wasn't crated.
  EXPECT_FALSE(component.component_state());

  // Shutdown component before destroying dummy_agent_api_bindings.
  base::RunLoop shutdown_run_loop;
  dummy_component_state->set_on_delete(shutdown_run_loop.QuitClosure());
  component.component_controller()->ptr().Unbind();
  shutdown_run_loop.Run();
}

class AudioCastRunnerIntegrationTest : public CastRunnerIntegrationTest {
 public:
  AudioCastRunnerIntegrationTest()
      : CastRunnerIntegrationTest(
            test::kCastRunnerFeaturesFakeAudioDeviceEnumerator) {}
};

TEST_F(AudioCastRunnerIntegrationTest, Microphone) {
  TestCastComponent component(cast_runner());
  GURL app_url = test_server().GetURL("/microphone.html");
  auto app_config =
      FakeApplicationConfigManager::CreateConfig(kTestAppId, app_url);

  fuchsia::web::PermissionDescriptor mic_permission;
  mic_permission.set_type(fuchsia::web::PermissionType::MICROPHONE);
  app_config.mutable_permissions()->push_back(std::move(mic_permission));
  app_config_manager().AddAppConfig(std::move(app_config));

  // Expect fuchsia.media.Audio connection to be requested.
  base::RunLoop run_loop;
  cast_runner_launcher().fake_cast_agent().RegisterOnConnectClosure(
      fuchsia::media::Audio::Name_, run_loop.QuitClosure());

  component.CreateComponentContextAndStartComponent();
  component.ExecuteJavaScript("connectMicrophone();");

  // Will quit once AudioCapturer is connected.
  run_loop.Run();
}

TEST_F(CastRunnerIntegrationTest, Camera) {
  TestCastComponent component(cast_runner());
  GURL app_url = test_server().GetURL("/camera.html");
  auto app_config =
      FakeApplicationConfigManager::CreateConfig(kTestAppId, app_url);

  fuchsia::web::PermissionDescriptor camera_permission;
  camera_permission.set_type(fuchsia::web::PermissionType::CAMERA);
  app_config.mutable_permissions()->push_back(std::move(camera_permission));
  app_config_manager().AddAppConfig(std::move(app_config));

  // Expect fuchsia.camera3.DeviceWatcher connection to be requested.
  cast_runner_launcher().fake_cast_agent().RegisterOnConnectClosure(
      fuchsia::camera3::DeviceWatcher::Name_,
      base::MakeExpectedRunAtLeastOnceClosure(FROM_HERE));

  component.CreateComponentContextAndStartComponent();

  component.ExecuteJavaScript("connectCamera();");
}

TEST_F(CastRunnerIntegrationTest, CameraAccessAfterComponentShutdown) {
  TestCastComponent component(cast_runner());
  GURL app_url = test_server().GetURL("/camera.html");

  // First app with camera permission.
  auto app_config =
      FakeApplicationConfigManager::CreateConfig(kTestAppId, app_url);
  fuchsia::web::PermissionDescriptor camera_permission;
  camera_permission.set_type(fuchsia::web::PermissionType::CAMERA);
  app_config.mutable_permissions()->push_back(std::move(camera_permission));
  app_config_manager().AddAppConfig(std::move(app_config));

  // Second app without camera permission (but it will still try to access
  // fuchsia.camera3.DeviceWatcher service to enumerate devices).
  TestCastComponent second_component(cast_runner());
  auto app_config_2 =
      FakeApplicationConfigManager::CreateConfig(kSecondTestAppId, app_url);
  app_config_manager().AddAppConfig(std::move(app_config_2));

  // Start and then shutdown the first app.
  component.CreateComponentContextAndStartComponent(kTestAppId);
  component.ShutdownComponent();
  component.ResetComponentState();

  // Start the second app and try to connect the camera. It's expected to fail
  // to initialize the camera without crashing CastRunner.
  second_component.CreateComponentContextAndStartComponent(kSecondTestAppId);
  EXPECT_EQ(second_component.ExecuteJavaScript("connectCamera();"),
            "getUserMediaFailed");
}

TEST_F(CastRunnerIntegrationTest, MultipleComponentsUsingCamera) {
  TestCastComponent first_component(cast_runner());
  TestCastComponent second_component(cast_runner());

  GURL app_url = test_server().GetURL("/camera.html");

  // Expect fuchsia.camera3.DeviceWatcher connection to be requested.
  cast_runner_launcher().fake_cast_agent().RegisterOnConnectClosure(
      fuchsia::camera3::DeviceWatcher::Name_,
      base::MakeExpectedRunAtLeastOnceClosure(FROM_HERE));

  // Start two apps, both with camera permission.
  auto app_config1 =
      FakeApplicationConfigManager::CreateConfig(kTestAppId, app_url);
  fuchsia::web::PermissionDescriptor camera_permission1;
  camera_permission1.set_type(fuchsia::web::PermissionType::CAMERA);
  app_config1.mutable_permissions()->push_back(std::move(camera_permission1));
  app_config_manager().AddAppConfig(std::move(app_config1));
  first_component.CreateComponentContextAndStartComponent(kTestAppId);

  auto app_config2 =
      FakeApplicationConfigManager::CreateConfig(kSecondTestAppId, app_url);
  fuchsia::web::PermissionDescriptor camera_permission2;
  camera_permission2.set_type(fuchsia::web::PermissionType::CAMERA);
  app_config2.mutable_permissions()->push_back(std::move(camera_permission2));
  app_config_manager().AddAppConfig(std::move(app_config2));
  second_component.CreateComponentContextAndStartComponent(kSecondTestAppId);

  // Shut down the first component.
  first_component.ShutdownComponent();
  first_component.ResetComponentState();

  second_component.ExecuteJavaScript("connectCamera();");
}

class HeadlessCastRunnerIntegrationTest : public CastRunnerIntegrationTest {
 public:
  HeadlessCastRunnerIntegrationTest()
      : CastRunnerIntegrationTest(test::kCastRunnerFeaturesHeadless) {}
};

// A basic integration test ensuring a basic cast request launches the right
// URL in the Chromium service.
TEST_F(HeadlessCastRunnerIntegrationTest, Headless) {
  TestCastComponent component(cast_runner());

  const char kAnimationPath[] = "/css_animation.html";
  const GURL animation_url = test_server().GetURL(kAnimationPath);
  app_config_manager().AddApp(kTestAppId, animation_url);

  component.CreateComponentContextAndStartComponent();
  auto tokens = scenic::ViewTokenPair::New();
  auto view_ref_pair = scenic::ViewRefPair::New();

  // Create a view.
  auto view_provider = component.component_services_client()
                           ->Connect<fuchsia::ui::app::ViewProvider>();
  view_provider->CreateViewWithViewRef(
      std::move(tokens.view_holder_token.value),
      std::move(view_ref_pair.control_ref), std::move(view_ref_pair.view_ref));

  component.api_bindings()->RunAndReturnConnectedPort("animation_finished");

  // Verify that dropped "view" EventPair is handled properly.
  tokens.view_token.value.reset();
  component.api_bindings()->RunAndReturnConnectedPort("view_hidden");
}

// Isolated *and* headless? Doesn't sound like much fun!
TEST_F(HeadlessCastRunnerIntegrationTest, IsolatedAndHeadless) {
  TestCastComponent component(cast_runner());
  const GURL kContentDirectoryUrl("fuchsia-dir://testdata/empty.html");

  app_config_manager().AddAppConfig(
      CreateAppConfigWithTestData(kTestAppId, kContentDirectoryUrl));
  component.CreateComponentContextAndStartComponent();
  component.CheckAppUrl(kContentDirectoryUrl);
}

// Verifies that the Context can establish a connection to the Agent's
// MetricsRecorder service.
TEST_F(CastRunnerIntegrationTest, LegacyMetricsRedirect) {
  TestCastComponent component(cast_runner());
  GURL app_url = test_server().GetURL(kBlankAppUrl);
  app_config_manager().AddApp(kTestAppId, app_url);

  bool connected_to_metrics_recorder_service = false;

  cast_runner_launcher().fake_cast_agent().RegisterOnConnectClosure(
      fuchsia::legacymetrics::MetricsRecorder::Name_,
      base::BindLambdaForTesting([&connected_to_metrics_recorder_service]() {
        connected_to_metrics_recorder_service = true;
      }));

  // If the Component is going to connect to the MetricsRecorder service, it
  // will have done so by the time the Component is responding.
  component.CreateComponentContextAndStartComponent();
  ASSERT_EQ(connected_to_metrics_recorder_service,
#if BUILDFLAG(ENABLE_CAST_RECEIVER)
            true
#else
            false
#endif
  );
}

// Verifies that the ApplicationContext::OnApplicationTerminated() is notified
// with the component exit code if the web content closes itself.
TEST_F(CastRunnerIntegrationTest, OnApplicationTerminated_WindowClose) {
  TestCastComponent component(cast_runner());
  const GURL url = test_server().GetURL(kBlankAppUrl);
  app_config_manager().AddApp(kTestAppId, url);

  component.CreateComponentContextAndStartComponent();

  // It is possible to observe the ComponentController close before
  // OnApplicationTerminated() is received, so ignore that.
  component.component_controller()->ptr().set_error_handler([](zx_status_t) {});

  // Have the web content close itself, and wait for OnApplicationTerminated().
  EXPECT_EQ(component.ExecuteJavaScript("window.close()"), "undefined");
  absl::optional<zx_status_t> exit_code = component.component_state()
                                              ->application_context()
                                              ->WaitForApplicationTerminated();
  ASSERT_TRUE(exit_code);
  EXPECT_EQ(exit_code.value(), ZX_OK);
}

// Verifies that the ComponentController reports TerminationReason::EXITED and
// exit code ZX_OK if the web content terminates itself.
// TODO(https://crbug.com/1066833): Make this a WebRunner test.
// TODO(crbug.com/1408810): Re-enable this test
#if BUILDFLAG(IS_FUCHSIA)
#define MAYBE_OnTerminated_WindowClose DISABLED_OnTerminated_WindowClose
#else
#define MAYBE_OnTerminated_WindowClose OnTerminated_WindowClose
#endif
TEST_F(CastRunnerIntegrationTest, MAYBE_OnTerminated_WindowClose) {
  TestCastComponent component(cast_runner());
  const GURL url = test_server().GetURL(kBlankAppUrl);
  app_config_manager().AddApp(kTestAppId, url);

  component.CreateComponentContextAndStartComponent();

  // Register an handler on the ComponentController channel, for the
  // OnTerminated event.
  base::RunLoop exit_code_loop;
  component.component_controller()->ptr().set_error_handler(
      [quit_loop = exit_code_loop.QuitClosure()](zx_status_t) {
        quit_loop.Run();
        ADD_FAILURE();
      });
  component.component_controller()->ptr().events().OnTerminated =
      [quit_loop = exit_code_loop.QuitClosure()](
          int64_t exit_code, fuchsia::sys::TerminationReason reason) {
        quit_loop.Run();
        EXPECT_EQ(reason, fuchsia::sys::TerminationReason::EXITED);
        EXPECT_EQ(exit_code, ZX_OK);
      };

  // Have the web content close itself, and wait for OnTerminated().
  EXPECT_EQ(component.ExecuteJavaScript("window.close()"), "undefined");
  exit_code_loop.Run();

  component.component_controller()->ptr().Unbind();
}

// Verifies that the ComponentController reports TerminationReason::EXITED and
// exit code ZX_OK if Kill() is used.
// TODO(https://crbug.com/1066833): Make this a WebRunner test.
TEST_F(CastRunnerIntegrationTest, OnTerminated_ComponentKill) {
  TestCastComponent component(cast_runner());
  const GURL url = test_server().GetURL(kBlankAppUrl);
  app_config_manager().AddApp(kTestAppId, url);

  component.CreateComponentContextAndStartComponent();

  // Register an handler on the ComponentController channel, for the
  // OnTerminated event.
  base::RunLoop exit_code_loop;
  component.component_controller()->ptr().set_error_handler(
      [quit_loop = exit_code_loop.QuitClosure()](zx_status_t) {
        quit_loop.Run();
        ADD_FAILURE();
      });
  component.component_controller()->ptr().events().OnTerminated =
      [quit_loop = exit_code_loop.QuitClosure()](
          int64_t exit_code, fuchsia::sys::TerminationReason reason) {
        quit_loop.Run();
        EXPECT_EQ(reason, fuchsia::sys::TerminationReason::EXITED);
        EXPECT_EQ(exit_code, ZX_OK);
      };

  // Kill() the component and wait for OnTerminated().
  component.component_controller()->ptr()->Kill();
  exit_code_loop.Run();

  component.component_controller()->ptr().Unbind();
}

// Ensures that CastRunner handles the value not being specified.
// TODO(https://crrev.com/c/2516246): Check for no logging.
TEST_F(CastRunnerIntegrationTest, InitialMinConsoleLogSeverity_NotSet) {
  TestCastComponent component(cast_runner());
  GURL app_url = test_server().GetURL(kBlankAppUrl);
  auto app_config =
      FakeApplicationConfigManager::CreateConfig(kTestAppId, app_url);

  EXPECT_FALSE(app_config.has_initial_min_console_log_severity());
  app_config_manager().AddAppConfig(std::move(app_config));

  component.CreateComponentContextAndStartComponent();

  component.CheckAppUrl(app_url);
}

// TODO(https://crrev.com/c/2516246): Check for logging.
TEST_F(CastRunnerIntegrationTest, InitialMinConsoleLogSeverity_DEBUG) {
  TestCastComponent component(cast_runner());
  GURL app_url = test_server().GetURL(kBlankAppUrl);
  auto app_config =
      FakeApplicationConfigManager::CreateConfig(kTestAppId, app_url);

  *app_config.mutable_initial_min_console_log_severity() =
      fuchsia::diagnostics::Severity::DEBUG;
  app_config_manager().AddAppConfig(std::move(app_config));

  component.CreateComponentContextAndStartComponent();

  component.CheckAppUrl(app_url);
}

TEST_F(CastRunnerIntegrationTest, WebGLContextAbsentWithoutVulkanFeature) {
  TestCastComponent component(cast_runner());
  const char kTestPath[] = "/webgl_presence.html";
  const GURL test_url = test_server().GetURL(kTestPath);
  app_config_manager().AddApp(kTestAppId, test_url);

  component.CreateComponentContextAndStartComponent();

  EXPECT_EQ(component.ExecuteJavaScript("document.title"), "absent");
}

TEST_F(CastRunnerIntegrationTest,
       WebGLContextAbsentWithoutVulkanFeature_IsolatedRunner) {
  TestCastComponent component(cast_runner());
  const GURL kContentDirectoryUrl("fuchsia-dir://testdata/webgl_presence.html");

  app_config_manager().AddAppConfig(
      CreateAppConfigWithTestData(kTestAppId, kContentDirectoryUrl));
  component.CreateComponentContextAndStartComponent();
  component.CheckAppUrl(kContentDirectoryUrl);

  EXPECT_EQ(component.ExecuteJavaScript("document.title"), "absent");
}

// Verifies that starting a component fails if CORS exempt headers cannot be
// fetched.
TEST_F(CastRunnerIntegrationTest, MissingCorsExemptHeaderProvider) {
  TestCastComponent component(cast_runner());
  GURL app_url = test_server().GetURL(kBlankAppUrl);
  app_config_manager().AddApp(kTestAppId, app_url);

  // Start the Cast component, and wait for the controller to disconnect.
  component.StartCastComponent(base::StrCat({"cast:", kTestAppId}));

  component.ExpectControllerDisconnectWithStatus(ZX_ERR_PEER_CLOSED);

  EXPECT_FALSE(component.component_state());
}

// Verifies that CastRunner offers a chromium.cast.DataReset service.
// Verifies that after the DeletePersistentData() API is invoked, no further
// component-start requests are honoured.
// TODO(crbug.com/1146474): Expand the test to verify that the persisted data is
// correctly cleared (e.g. using a custom test HTML app that uses persisted
// data).
TEST_F(CastRunnerIntegrationTest, DataReset_Service) {
  base::RunLoop loop;
  auto data_reset = cast_runner_services().Connect<chromium::cast::DataReset>();
  data_reset.set_error_handler([quit_loop = loop.QuitClosure()](zx_status_t) {
    quit_loop.Run();
    ADD_FAILURE();
  });
  bool succeeded = false;
  data_reset->DeletePersistentData([&succeeded, &loop](bool result) {
    succeeded = result;
    loop.Quit();
  });
  loop.Run();

  EXPECT_TRUE(succeeded);

  // Verify that it is no longer possible to launch a component.
  TestCastComponent component(cast_runner());
  GURL app_url = test_server().GetURL(kBlankAppUrl);
  app_config_manager().AddApp(kTestAppId, app_url);
  component.StartCastComponent(base::StrCat({"cast:", kTestAppId}));
  component.ExpectControllerDisconnectWithStatus(ZX_ERR_PEER_CLOSED);
}

// Verifies that the CastRunner exposes a fuchsia.web.FrameHost protocol
// capability, without requiring any special configuration.
TEST_F(CastRunnerIntegrationTest, FrameHost_Service) {
  // Connect to the fuchsia.web.FrameHost service and create a Frame.
  auto frame_host = cast_runner_services().Connect<fuchsia::web::FrameHost>();
  fuchsia::web::FramePtr frame;
  frame_host->CreateFrameWithParams(fuchsia::web::CreateFrameParams(),
                                    frame.NewRequest());

  // Verify that a response is received for a LoadUrl() request to the frame.
  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());
  const GURL url = test_server().GetURL(kBlankAppUrl);
  EXPECT_TRUE(LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), url.spec()));
}

#if defined(ARCH_CPU_ARM_FAMILY)
// TODO(crbug.com/1377994): Enable on ARM64 when bots support Vulkan.
#define MAYBE_VulkanCastRunnerIntegrationTest \
  DISABLED_VulkanCastRunnerIntegrationTest
#else
#define MAYBE_VulkanCastRunnerIntegrationTest VulkanCastRunnerIntegrationTest
#endif

class MAYBE_VulkanCastRunnerIntegrationTest : public CastRunnerIntegrationTest {
 public:
  MAYBE_VulkanCastRunnerIntegrationTest()
      : CastRunnerIntegrationTest(test::kCastRunnerFeaturesVulkan) {}
};

TEST_F(MAYBE_VulkanCastRunnerIntegrationTest,
       WebGLContextPresentWithVulkanFeature) {
  TestCastComponent component(cast_runner());
  const char kTestPath[] = "/webgl_presence.html";
  const GURL test_url = test_server().GetURL(kTestPath);
  app_config_manager().AddApp(kTestAppId, test_url);

  component.CreateComponentContextAndStartComponent();

  EXPECT_EQ(component.ExecuteJavaScript("document.title"), "present");
}

TEST_F(MAYBE_VulkanCastRunnerIntegrationTest,
       WebGLContextPresentWithVulkanFeature_IsolatedRunner) {
  TestCastComponent component(cast_runner());
  const GURL kContentDirectoryUrl("fuchsia-dir://testdata/webgl_presence.html");

  app_config_manager().AddAppConfig(
      CreateAppConfigWithTestData(kTestAppId, kContentDirectoryUrl));
  component.CreateComponentContextAndStartComponent();
  component.CheckAppUrl(kContentDirectoryUrl);

  EXPECT_EQ(component.ExecuteJavaScript("document.title"), "present");
}
