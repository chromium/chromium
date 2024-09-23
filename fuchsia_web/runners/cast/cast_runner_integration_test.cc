// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chromium/cast/cpp/fidl.h>
#include <fuchsia/camera3/cpp/fidl.h>
#include <fuchsia/legacymetrics/cpp/fidl.h>
#include <fuchsia/media/cpp/fidl.h>
#include <fuchsia/ui/views/cpp/fidl.h>
#include <fuchsia/web/cpp/fidl.h>
#include <lib/fdio/directory.h>
#include <lib/fidl/cpp/binding.h>
#include <lib/sys/cpp/component_context.h>
#include <lib/zx/eventpair.h>

#include <string_view>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/fuchsia/file_utils.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/mem_buffer_util.h"
#include "base/fuchsia/scoped_service_binding.h"
#include "base/functional/callback_helpers.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/test/test_timeouts.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "components/fuchsia_component_support/dynamic_component_host.h"
#include "fuchsia_web/common/string_util.h"
#include "fuchsia_web/common/test/fit_adapter.h"
#include "fuchsia_web/common/test/frame_for_test.h"
#include "fuchsia_web/common/test/frame_test_util.h"
#include "fuchsia_web/common/test/test_debug_listener.h"
#include "fuchsia_web/common/test/test_devtools_list_fetcher.h"
#include "fuchsia_web/common/test/test_navigation_listener.h"
#include "fuchsia_web/common/test/url_request_rewrite_test_util.h"
#include "fuchsia_web/runners/cast/cast_runner.h"
#include "fuchsia_web/runners/cast/cast_runner_switches.h"
#include "fuchsia_web/runners/cast/test/cast_runner_features.h"
#include "fuchsia_web/runners/cast/test/cast_runner_launcher.h"
#include "fuchsia_web/runners/cast/test/fake_api_bindings.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kTestAppId[] = "00000000";
constexpr char kSecondTestAppId[] = "FFFFFFFF";

constexpr char kBlankAppUrl[] = "/defaultresponse";
constexpr char kEchoHeaderPath[] = "/echoheader?Test";

chromium::cast::ApplicationConfig CreateAppConfigWithTestData(
    std::string_view app_id,
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
    if (rules_sent_) {
      return;
    }
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

  chromium::cast::ApplicationController* application_controller() {
    if (!application_controller_) {
      return nullptr;
    }

    return application_controller_.get();
  }

  void WaitForSetApplicationController() {
    if (application_controller_) {
      return;
    }
    base::RunLoop loop;
    on_set_application_controller_ = loop.QuitClosure();
    loop.Run();
  }

  std::optional<int64_t> WaitForApplicationTerminated() {
    if (application_exit_code_.has_value()) {
      return application_exit_code_;
    }
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
      fidl::InterfaceHandle<chromium::cast::ApplicationController>
          application_controller) override {
    application_controller_ = application_controller.Bind();
    if (on_set_application_controller_) {
      std::move(on_set_application_controller_).Run();
    }
  }
  void OnApplicationExit(int64_t exit_code) override {
    application_exit_code_ = exit_code;
    if (on_application_terminated_) {
      std::move(on_application_terminated_).Run();
    }
  }

  chromium::cast::ApplicationControllerPtr application_controller_;
  base::OnceClosure on_set_application_controller_;

  std::optional<int64_t> application_exit_code_;
  base::OnceClosure on_application_terminated_;
};

class TestCastComponent {
 public:
  // `test_realm_services` is used to connect to the test `Realm` exposed by
  // the CastRunnerLauncher, that contains a collection capable of resolving
  // and running Cast components.
  explicit TestCastComponent(const sys::ServiceDirectory& test_realm_services)
      : test_realm_services_(test_realm_services) {
    // Instantiate the per-instance service directory and fakes by default,
    // for tests to configure & add expectations to.
    services_.emplace();
  }

  ~TestCastComponent() { ShutdownComponent(); }

  TestCastComponent(const TestCastComponent&) = delete;
  TestCastComponent& operator=(const TestCastComponent&) = delete;

  void disable_offer_services() { offer_services_ = false; }
  void offer_closed_services() { offer_closed_services_ = true; }

  // Attempts to start the Cast activity identified by `app_id`, and to inject
  // script bindings to support querying JavaScript/DOM state via the
  // the `ExecuteJavaScript()` API (see below).
  // Note that this function will not return until the activity has actually
  // launched.
  void StartCastComponentWithQueryApi(std::string_view app_id = kTestAppId) {
    auto component_url = base::StrCat({"cast:", app_id});
    InjectQueryApi();
    StartCastComponent(component_url);
    WaitQueryApiConnected();
  }

  // Attempts to start the Cast activity identified by `app_id`.
  // Note that activity launch is asynchronous, tests that need to wait until
  // the activity has actually started (e.g. to interact with its
  // `ApplicationController`, etc), should normally use the
  // `StartCastComponentWithQueryApi()` call instead.
  void StartCastComponent(std::string_view component_url) {
    ASSERT_FALSE(component_) << "Component may only be started once";

    fidl::InterfaceHandle<fuchsia::io::Directory> services;
    if (offer_services_) {
      if (offer_closed_services_) {
        // Create the `services` channel, and immediately close the "request"
        // end, to simulate a missing service directory.
        std::ignore = services.NewRequest();
      } else {
        // Create a `fuchsia.io.Directory` connected to the directory of fake
        // services.
        zx_status_t status = services_->services.Serve(
            fuchsia::io::OpenFlags::RIGHT_READABLE |
                fuchsia::io::OpenFlags::RIGHT_WRITABLE |
                fuchsia::io::OpenFlags::DIRECTORY,
            services.NewRequest().TakeChannel());
        ZX_CHECK(status == ZX_OK, status) << "Serve()";
      }
    }

    // Create the new dynamic component in the test component collection for
    // the `cast_runner`.  The component is created with an Id chosen
    // at random to uniquely identify it, and supplied with `services` as
    // configured above.
    component_.emplace(
        test_realm_services_.Connect<fuchsia::component::Realm>(),
        test::CastRunnerLauncher::kTestCollectionName,
        base::Uuid::GenerateRandomV4().AsLowercaseString(), component_url,
        base::BindOnce(&TestCastComponent::OnComponentTeardown,
                       base::Unretained(this)),
        std::move(services));
  }

  // Executes |code| in the context of the test application and then returns
  // the result serialized as string. If the code evaluates to a promise then
  // execution is blocked until the promise is complete and the result of the
  // promise is returned.
  std::string ExecuteJavaScript(const std::string& code) {
    CHECK(test_port_);

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

    std::optional<std::string> response_string =
        base::StringFromMemBuffer(response.Get().data());
    EXPECT_TRUE(response_string.has_value());

    return response_string.value_or(std::string());
  }

  std::string QueryAppUrl() {
    return ExecuteJavaScript("window.location.href");
  }

  // Closes active connections to all services offered to the component,
  // to simulate the controlling agent tearing-down unexpectedly.
  void DisconnectServices() { services_.reset(); }

  // Destroys the component and runs until it is observed to have torn-down.
  void ShutdownComponent() {
    if (component_) {
      component_->Destroy();
      WaitForComponentDestroyed();
    }
  }

  // Runs until `component_` is observed to have torn-down.
  // Note that this may return before connections to services used by
  // the component have been observed to have closed.
  // This should be used after triggering component teardown, e.g. via an
  // explicit ComponentController.Kill() call, to wait for it to take effect.
  void WaitForComponentDestroyed() {
    ASSERT_TRUE(component_);
    base::RunLoop state_loop;
    base::AutoReset reset_callback(&on_component_destroyed_,
                                   state_loop.QuitClosure());
    state_loop.Run();
  }

  // Disables treatment of `component_` teardown as a test failure. This is
  // useful for test of teardown-time API behaviours.
  void SetIgnoreComponentDestroyed() {
    on_component_destroyed_ = base::DoNothing();
  }

  FakeApiBindingsImpl& api_bindings() { return services_->api_bindings; }
  FakeApplicationContext& application_context() {
    return services_->application_context;
  }

  sys::ServiceDirectory& exposed_by_component() {
    return component_->exposed();
  }

 private:
  // Used to manage fake services offered to each Cast component individually.
  // Most of the FIDL services used by Cast activities are ambient, provided by
  // the CastRunner itself. The services provided here are only those offered
  // directly to Cast activities by their owning agent.
  struct FakeComponentServices {
    FakeComponentServices()
        : api_bindings_binding(&services, &api_bindings),
          url_request_rewrite_rules_provider_binding(
              &services,
              &url_request_rewrite_rules_provider),
          context_binding(&services, &application_context) {}

    // Directory of services to offer to the Cast component.
    vfs::PseudoDir services;

    FakeApiBindingsImpl api_bindings;
    base::ScopedServiceBinding<chromium::cast::ApiBindings>
        api_bindings_binding;

    FakeUrlRequestRewriteRulesProvider url_request_rewrite_rules_provider;
    base::ScopedServiceBinding<chromium::cast::UrlRequestRewriteRulesProvider>
        url_request_rewrite_rules_provider_binding;

    FakeApplicationContext application_context;
    base::ScopedServiceBinding<chromium::cast::ApplicationContext>
        context_binding;
  };

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
    services_->api_bindings.set_bindings(std::move(binding_list));
  }

  void WaitQueryApiConnected() {
    EXPECT_FALSE(test_port_);
    test_port_ =
        services_->api_bindings.RunAndReturnConnectedPort("testport").Bind();
  }

  void OnComponentTeardown() {
    component_.reset();

    if (on_component_destroyed_) {
      std::move(on_component_destroyed_).Run();
    } else {
      ADD_FAILURE() << "Unexpected TestCastComponent teardown.";
    }
  }

  const sys::ServiceDirectory& test_realm_services_;

  // True if the Cast component should be offered a service directory channel
  // that has already been closed, to simulate the providing agent having
  // torn-down the directory before the component Connect()s through it.
  bool offer_closed_services_ = false;

  // False if the Cast component should not be offered any service directory.
  bool offer_services_ = true;

  // Holds the service directory and fake services offered to `component_`.
  std::optional<FakeComponentServices> services_;

  std::optional<fuchsia_component_support::DynamicComponentHost> component_;

  fuchsia::web::MessagePortPtr test_port_;

  base::OnceClosure on_component_destroyed_;
};

// Base class for all integration tests, parameterized on the set of
// "features" to enable in the `cast_runner` under test.
class CastRunnerIntegrationTest : public testing::Test {
 protected:
  CastRunnerIntegrationTest()
      : CastRunnerIntegrationTest(test::kCastRunnerFeaturesNone) {}
  explicit CastRunnerIntegrationTest(test::CastRunnerFeatures runner_features)
      : cast_runner_(runner_features) {}

  ~CastRunnerIntegrationTest() override = default;

  CastRunnerIntegrationTest(const CastRunnerIntegrationTest&) = delete;
  CastRunnerIntegrationTest& operator=(const CastRunnerIntegrationTest&) =
      delete;

  // testing::Test overrides.
  void SetUp() override {
    static constexpr std::string_view kTestServerRoot(
        "fuchsia_web/runners/cast/testdata");
    test_server_.ServeFilesFromSourceDirectory(kTestServerRoot);
    net::test_server::RegisterDefaultHandlers(&test_server_);
    ASSERT_TRUE(test_server_.Start());
  }

  // Returns the services exposed by the `CastRunnerLauncher` test Realm,
  // including those exposed by the `cast_runner` component under test.
  const sys::ServiceDirectory& test_realm_services() {
    return cast_runner_.exposed_services();
  }

  test::CastRunnerLauncher& cast_runner_launcher() { return cast_runner_; }

  // Returns the HTTP server used to serve fake content for Cast components.
  net::EmbeddedTestServer& test_server() { return test_server_; }

  // Convenience accessors for elements managed by the launcher.
  FakeApplicationConfigManager& app_config_manager() {
    return cast_runner_.fake_cast_agent().app_config_manager();
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};

  // TODO(crbug.com/42050227): Override the RunLoop timeout set by
  // |task_environment_| to allow for the very high variability in web.Context
  // launch times.
  const base::test::ScopedRunLoopTimeout scoped_timeout_{
      FROM_HERE, TestTimeouts::action_max_timeout()};

  test::CastRunnerLauncher cast_runner_;
  net::EmbeddedTestServer test_server_;
};

}  // namespace

// A basic integration test ensuring a basic cast request launches the right
// URL in the Chromium service.
TEST_F(CastRunnerIntegrationTest, BasicRequest) {
  TestCastComponent component(test_realm_services());

  GURL app_url = test_server().GetURL(kBlankAppUrl);
  app_config_manager().AddApp(kTestAppId, app_url);
  component.StartCastComponentWithQueryApi();

  // Verify that the app has navigated to the expected URL.
  EXPECT_EQ(component.QueryAppUrl(), app_url.spec());
}

// Verify that the Runner can continue to be used even after its Context has
// crashed. Regression test for https://crbug.com/1066826.
// TODO(crbug.com/40682680): Replace this with a WebRunner test, ideally a
//   unit-test, which can simulate Context disconnection more simply.
TEST_F(CastRunnerIntegrationTest, CanRecreateContext) {
  TestCastComponent component(test_realm_services());
  const GURL app_url = test_server().GetURL(kBlankAppUrl);
  app_config_manager().AddApp(kTestAppId, app_url);

  // Create a Cast component and verify that it has loaded.
  component.StartCastComponentWithQueryApi(kTestAppId);

  // Connect to the CastRunner's Realm, and request to enumerate the contents
  // of the "web_instances" collection.
  fidl::SynchronousInterfacePtr<fuchsia::component::Realm> runner_realm;
  test_realm_services().Connect(
      runner_realm.NewRequest(),
      test::CastRunnerLauncher::kCastRunnerRealmProtocol);
  fidl::SynchronousInterfacePtr<fuchsia::component::ChildIterator>
      instance_iterator;
  fuchsia::component::Realm_ListChildren_Result list_children_result;
  zx_status_t status = runner_realm->ListChildren(
      fuchsia::component::decl::CollectionRef{.name = "web_instances"},
      instance_iterator.NewRequest(), &list_children_result);
  ASSERT_EQ(status, ZX_OK);
  ASSERT_FALSE(list_children_result.is_err());

  // The CastRunner's "web_instances" collection should contain exactly one
  // child component.
  std::vector<fuchsia::component::decl::ChildRef> web_instance_refs;
  status = instance_iterator->Next(&web_instance_refs);
  ASSERT_EQ(status, ZX_OK);
  ASSERT_EQ(web_instance_refs.size(), 1u);

  // Verify that no further children remain in "web_instances".
  std::vector<fuchsia::component::decl::ChildRef> empty_web_instance_refs;
  status = instance_iterator->Next(&empty_web_instance_refs);
  ASSERT_EQ(status, ZX_OK);
  ASSERT_TRUE(empty_web_instance_refs.empty());

  // Destroy the one child of the "web_instances" collection.
  fuchsia::component::Realm_DestroyChild_Result destroy_child_result;
  status = runner_realm->DestroyChild(std::move(web_instance_refs[0]),
                                      &destroy_child_result);
  ASSERT_EQ(status, ZX_OK);
  ASSERT_FALSE(destroy_child_result.is_err());

  // Expect that the Cast component goes away, since its container is gone.
  component.WaitForComponentDestroyed();

  // Create a second Cast component and verify that it has loaded.
  // There is no guarantee that the CastRunner has detected the old web.Context
  // disconnecting yet, so attempts to launch Cast components could fail.
  // WebContentRunner::CreateFrameWithParams() will synchronously verify that
  // the web.Context is not-yet-closed, to work-around that.
  TestCastComponent second_component(test_realm_services());
  second_component.StartCastComponentWithQueryApi(kTestAppId);
}

TEST_F(CastRunnerIntegrationTest, ApiBindings) {
  TestCastComponent component(test_realm_services());
  app_config_manager().AddApp(kTestAppId, test_server().GetURL(kBlankAppUrl));

  component.StartCastComponentWithQueryApi();

  // Verify that we can communicate with the query-API binding added by
  // `StartCastComponentWithQueryApi()`.
  EXPECT_EQ(component.ExecuteJavaScript("1+2+\"\""), "3");
}

TEST_F(CastRunnerIntegrationTest, UnknownCastAppId_Fails) {
  TestCastComponent component(test_realm_services());
  const char kUnknownComponentUrl[] = "cast:99999999";

  component.StartCastComponent(kUnknownComponentUrl);

  // Run the loop until the ComponentController is dropped.
  component.WaitForComponentDestroyed();
}

TEST_F(CastRunnerIntegrationTest, UrlRequestRewriteRulesProvider) {
  TestCastComponent component(test_realm_services());
  GURL echo_app_url = test_server().GetURL(kEchoHeaderPath);
  app_config_manager().AddApp(kTestAppId, echo_app_url);

  component.StartCastComponentWithQueryApi();

  EXPECT_EQ(component.ExecuteJavaScript("document.body.innerText"),
            "TestHeaderValue");
}

TEST_F(CastRunnerIntegrationTest, ApplicationControllerBound) {
  TestCastComponent component(test_realm_services());
  app_config_manager().AddApp(kTestAppId, test_server().GetURL(kBlankAppUrl));

  component.StartCastComponentWithQueryApi();

  // Run until the application calls SetApplicationController().
  component.application_context().WaitForSetApplicationController();
  EXPECT_TRUE(component.application_context().application_controller());
}

// Verify an App launched with remote debugging enabled is properly reachable.
TEST_F(CastRunnerIntegrationTest, RemoteDebugging) {
  TestCastComponent component(test_realm_services());
  GURL app_url = test_server().GetURL(kBlankAppUrl);
  auto app_config =
      FakeApplicationConfigManager::CreateConfig(kTestAppId, app_url);
  app_config.set_enable_remote_debugging(true);
  app_config_manager().AddAppConfig(std::move(app_config));

  component.StartCastComponentWithQueryApi();

  // Connect to the debug service and ensure we get the proper response.
  base::Value::List devtools_list =
      GetDevToolsListFromPort(CastRunner::kRemoteDebuggingPort);
  EXPECT_EQ(devtools_list.size(), 1u);

  const auto* devtools_url = devtools_list[0].GetDict().FindString("url");
  ASSERT_TRUE(devtools_url);
  EXPECT_EQ(*devtools_url, app_url.spec());
}

TEST_F(CastRunnerIntegrationTest, IsolatedContext) {
  TestCastComponent component(test_realm_services());
  const GURL kContentDirectoryUrl("fuchsia-dir://testdata/empty.html");

  app_config_manager().AddAppConfig(
      CreateAppConfigWithTestData(kTestAppId, kContentDirectoryUrl));
  component.StartCastComponentWithQueryApi();

  // Verify that the app was navigated to the isolated content URL.
  EXPECT_EQ(component.QueryAppUrl(), kContentDirectoryUrl.spec());
}

// Verify that the component fails to start if no service directory is offered.
TEST_F(CastRunnerIntegrationTest, ServiceDirectoryMissing_FailToStart) {
  TestCastComponent component(test_realm_services());
  component.disable_offer_services();
  app_config_manager().AddApp(kTestAppId,
                              test_server().GetURL(kEchoHeaderPath));

  component.StartCastComponent(base::StrCat({"cast:", kTestAppId}));

  // Expect that the component stops.
  component.WaitForComponentDestroyed();
}

// Verify that the component fails to start if the offered service directory
// channel has already been closed, such that Connect() calls will result in
// service request channels being dropped.
TEST_F(CastRunnerIntegrationTest, ServiceDirectoryEmpty_FailToStart) {
  TestCastComponent component(test_realm_services());
  component.offer_closed_services();
  app_config_manager().AddApp(kTestAppId,
                              test_server().GetURL(kEchoHeaderPath));

  component.StartCastComponent(base::StrCat({"cast:", kTestAppId}));

  // Expect that the component stops.
  component.WaitForComponentDestroyed();
}

// Simulate an Agent crash by tearing down `services_`, resulting in the
// service-directory and bindings passed to the Cast activity itself being
// closed. This should cause the component to terminate.
TEST_F(CastRunnerIntegrationTest, ServicesClose_TerminatesComponent) {
  TestCastComponent component(test_realm_services());
  app_config_manager().AddApp(kTestAppId,
                              test_server().GetURL(kEchoHeaderPath));

  component.StartCastComponentWithQueryApi();

  // Disconnect all service bindings.
  component.DisconnectServices();

  component.WaitForComponentDestroyed();
}

class AudioCastRunnerIntegrationTest : public CastRunnerIntegrationTest {
 public:
  AudioCastRunnerIntegrationTest()
      : CastRunnerIntegrationTest(
            test::kCastRunnerFeaturesFakeAudioDeviceEnumerator) {}
};

TEST_F(AudioCastRunnerIntegrationTest, Microphone) {
  TestCastComponent component(test_realm_services());
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

  component.StartCastComponentWithQueryApi();
  component.ExecuteJavaScript("connectMicrophone();");

  // Will quit once AudioCapturer is connected.
  run_loop.Run();
}

TEST_F(CastRunnerIntegrationTest, Camera) {
  TestCastComponent component(test_realm_services());
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

  component.StartCastComponentWithQueryApi();

  component.ExecuteJavaScript("connectCamera();");
}

TEST_F(CastRunnerIntegrationTest, CameraAccessAfterComponentShutdown) {
  TestCastComponent component(test_realm_services());
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
  TestCastComponent second_component(test_realm_services());
  auto app_config_2 =
      FakeApplicationConfigManager::CreateConfig(kSecondTestAppId, app_url);
  app_config_manager().AddAppConfig(std::move(app_config_2));

  // Start and then shutdown the first app.
  component.StartCastComponentWithQueryApi(kTestAppId);
  component.ShutdownComponent();

  // Start the second app and try to connect the camera. It's expected to fail
  // to initialize the camera without crashing CastRunner.
  second_component.StartCastComponentWithQueryApi(kSecondTestAppId);
  EXPECT_EQ(second_component.ExecuteJavaScript("connectCamera();"),
            "getUserMediaFailed");
}

TEST_F(CastRunnerIntegrationTest, MultipleComponentsUsingCamera) {
  TestCastComponent first_component(test_realm_services());
  TestCastComponent second_component(test_realm_services());

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
  first_component.StartCastComponentWithQueryApi(kTestAppId);

  auto app_config2 =
      FakeApplicationConfigManager::CreateConfig(kSecondTestAppId, app_url);
  fuchsia::web::PermissionDescriptor camera_permission2;
  camera_permission2.set_type(fuchsia::web::PermissionType::CAMERA);
  app_config2.mutable_permissions()->push_back(std::move(camera_permission2));
  app_config_manager().AddAppConfig(std::move(app_config2));
  second_component.StartCastComponentWithQueryApi(kSecondTestAppId);

  // Shut down the first component.
  first_component.ShutdownComponent();

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
  TestCastComponent component(test_realm_services());

  const char kAnimationPath[] = "/css_animation.html";
  const GURL animation_url = test_server().GetURL(kAnimationPath);
  app_config_manager().AddApp(kTestAppId, animation_url);

  component.StartCastComponentWithQueryApi();

  fuchsia::ui::views::ViewToken view_token;
  fuchsia::ui::views::ViewHolderToken view_holder_token;
  auto status =
      zx::eventpair::create(0u, &view_token.value, &view_holder_token.value);
  CHECK_EQ(ZX_OK, status);

  fuchsia::ui::views::ViewRefControl view_ref_control;
  fuchsia::ui::views::ViewRef view_ref;
  status = zx::eventpair::create(
      /*options*/ 0u, &view_ref_control.reference, &view_ref.reference);
  CHECK_EQ(ZX_OK, status);
  view_ref_control.reference.replace(
      ZX_DEFAULT_EVENTPAIR_RIGHTS & (~ZX_RIGHT_DUPLICATE),
      &view_ref_control.reference);
  view_ref.reference.replace(ZX_RIGHTS_BASIC, &view_ref.reference);

  // Create a view.
  auto view_provider = component.exposed_by_component()
                           .Connect<fuchsia::ui::app::ViewProvider>();
  view_provider->CreateViewWithViewRef(std::move(view_holder_token.value),
                                       std::move(view_ref_control),
                                       std::move(view_ref));

  component.api_bindings().RunAndReturnConnectedPort("animation_finished");

  // Verify that dropped "view" EventPair is handled properly.
  view_token.value.reset();
  component.api_bindings().RunAndReturnConnectedPort("view_hidden");
}

// Isolated *and* headless? Doesn't sound like much fun!
TEST_F(HeadlessCastRunnerIntegrationTest, IsolatedAndHeadless) {
  TestCastComponent component(test_realm_services());
  const GURL kContentDirectoryUrl("fuchsia-dir://testdata/empty.html");

  app_config_manager().AddAppConfig(
      CreateAppConfigWithTestData(kTestAppId, kContentDirectoryUrl));
  component.StartCastComponentWithQueryApi();

  // Verify that the app was able to navigate to the isolated content URL.
  EXPECT_EQ(component.QueryAppUrl(), kContentDirectoryUrl.spec());
}

// Verifies that the Context can establish a connection to the Agent's
// MetricsRecorder service.
TEST_F(CastRunnerIntegrationTest, LegacyMetricsRedirect) {
  TestCastComponent component(test_realm_services());
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
  component.StartCastComponentWithQueryApi();
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
  TestCastComponent component(test_realm_services());
  const GURL url = test_server().GetURL(kBlankAppUrl);
  app_config_manager().AddApp(kTestAppId, url);

  component.StartCastComponentWithQueryApi();

  // It is possible to observe the component teardown before
  // OnApplicationTerminated() is received, so ignore that.
  component.SetIgnoreComponentDestroyed();

  // Have the web content close itself, and wait for OnApplicationTerminated().
  EXPECT_EQ(component.ExecuteJavaScript("window.close()"), "undefined");
  std::optional<zx_status_t> exit_code =
      component.application_context().WaitForApplicationTerminated();
  ASSERT_TRUE(exit_code);
  EXPECT_EQ(exit_code.value(), ZX_OK);
}

// Verifies that the ApplicationContext::OnApplicationTerminated() is notified
// with the component exit code if the component is requested to stop.
TEST_F(CastRunnerIntegrationTest, OnApplicationTerminated_ComponentStop) {
  TestCastComponent component(test_realm_services());
  const GURL url = test_server().GetURL(kBlankAppUrl);
  app_config_manager().AddApp(kTestAppId, url);

  component.StartCastComponentWithQueryApi();

  // It is possible to observe the component teardown before
  // OnApplicationTerminated() is received, so ignore that.
  component.SetIgnoreComponentDestroyed();

  // Request that the component be destroyed, and wait for
  // OnApplicationTerminated().
  component.ShutdownComponent();
  std::optional<zx_status_t> exit_code =
      component.application_context().WaitForApplicationTerminated();
  ASSERT_TRUE(exit_code);
  EXPECT_EQ(exit_code.value(), ZX_OK);
}

// Ensures that CastRunner handles the value not being specified.
// TODO(https://crrev.com/c/2516246): Check for no logging.
TEST_F(CastRunnerIntegrationTest, InitialMinConsoleLogSeverity_NotSet) {
  TestCastComponent component(test_realm_services());
  GURL app_url = test_server().GetURL(kBlankAppUrl);
  auto app_config =
      FakeApplicationConfigManager::CreateConfig(kTestAppId, app_url);

  EXPECT_FALSE(app_config.has_initial_min_console_log_severity());
  app_config_manager().AddAppConfig(std::move(app_config));

  component.StartCastComponentWithQueryApi();
}

// TODO(https://crrev.com/c/2516246): Check for logging.
TEST_F(CastRunnerIntegrationTest, InitialMinConsoleLogSeverity_DEBUG) {
  TestCastComponent component(test_realm_services());
  GURL app_url = test_server().GetURL(kBlankAppUrl);
  auto app_config =
      FakeApplicationConfigManager::CreateConfig(kTestAppId, app_url);

  *app_config.mutable_initial_min_console_log_severity() =
      fuchsia::diagnostics::Severity::DEBUG;
  app_config_manager().AddAppConfig(std::move(app_config));

  component.StartCastComponentWithQueryApi();
}

TEST_F(CastRunnerIntegrationTest, WebGLContextAbsentWithoutVulkanFeature) {
  TestCastComponent component(test_realm_services());
  const char kTestPath[] = "/webgl_presence.html";
  const GURL test_url = test_server().GetURL(kTestPath);
  app_config_manager().AddApp(kTestAppId, test_url);

  component.StartCastComponentWithQueryApi();

  EXPECT_EQ(component.ExecuteJavaScript("document.title"), "absent");
}

TEST_F(CastRunnerIntegrationTest,
       WebGLContextAbsentWithoutVulkanFeature_IsolatedRunner) {
  TestCastComponent component(test_realm_services());
  const GURL kContentDirectoryUrl("fuchsia-dir://testdata/webgl_presence.html");

  app_config_manager().AddAppConfig(
      CreateAppConfigWithTestData(kTestAppId, kContentDirectoryUrl));
  component.StartCastComponentWithQueryApi();

  EXPECT_EQ(component.ExecuteJavaScript("document.title"), "absent");
}

// Verifies that starting a component fails if CORS exempt headers cannot be
// fetched.
TEST_F(CastRunnerIntegrationTest, MissingCorsExemptHeaderProvider) {
  // Prevent the FakeCastAgent from publishing the
  // chromium.cast.CorsExemptHeaderProvider service.
  cast_runner_launcher().fake_cast_agent().RegisterOnConnectClosure(
      chromium::cast::CorsExemptHeaderProvider::Name_, base::DoNothing());

  // Start the Cast component, and wait for it to be destroyed.
  TestCastComponent component(test_realm_services());
  GURL app_url = test_server().GetURL(kBlankAppUrl);
  app_config_manager().AddApp(kTestAppId, app_url);
  component.StartCastComponent(base::StrCat({"cast:", kTestAppId}));

  // Expect it to be more or less immediately torn-down.
  component.WaitForComponentDestroyed();
}

// Verifies that CastRunner offers a chromium.cast.DataReset service.
// Verifies that after the DeletePersistentData() API is invoked, no further
// component-start requests are honoured.
// TODO(crbug.com/40730094): Expand the test to verify that the persisted data
// is correctly cleared (e.g. using a custom test HTML app that uses persisted
// data).
TEST_F(CastRunnerIntegrationTest, DataReset_Service) {
  base::RunLoop loop;
  auto data_reset = test_realm_services().Connect<chromium::cast::DataReset>();
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
  TestCastComponent component(test_realm_services());
  GURL app_url = test_server().GetURL(kBlankAppUrl);
  app_config_manager().AddApp(kTestAppId, app_url);
  component.StartCastComponent(base::StrCat({"cast:", kTestAppId}));
  component.WaitForComponentDestroyed();
}

// Verifies that the CastRunner exposes a fuchsia.web.FrameHost protocol
// capability, without requiring any special configuration.
TEST_F(CastRunnerIntegrationTest, FrameHost_Service) {
  // Connect to the fuchsia.web.FrameHost service and create a Frame.
  auto frame_host = test_realm_services().Connect<fuchsia::web::FrameHost>();
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

// Check that connecting and disconnecting to the FrameHost service does not
// trigger shutdown of the devtools service.
TEST_F(CastRunnerIntegrationTest, FrameHostDebugging) {
  // Before triggering the launch of any `web_instance` by the `cast_runner`,
  // attach a `TestDebugListener`, to be notified when the DevTools port becomes
  // available.
  TestDebugListener dev_tools_listener;
  fidl::Binding<fuchsia::web::DevToolsListener> dev_tools_listener_binding(
      &dev_tools_listener);
  auto debug = test_realm_services().Connect<fuchsia::web::Debug>();
  debug.set_error_handler([](zx_status_t status) {
    ZX_LOG(ERROR, status) << "Failed to use debug protocol";
    ADD_FAILURE();
  });
  base::RunLoop dev_tools_enabled;
  debug->EnableDevTools(
      dev_tools_listener_binding.NewBinding(),
      [done = dev_tools_enabled.QuitClosure()]() { done.Run(); });
  dev_tools_enabled.Run();

  // Connect a `FrameHost` client, create a `Frame`, and navigate it to some
  // test content. Loading the content will result in the DevTools port becoming
  // available for the test to connect to.
  auto frame_host = test_realm_services().Connect<fuchsia::web::FrameHost>();
  fuchsia::web::CreateFrameParams create_frame_params;
  create_frame_params.set_enable_remote_debugging(true);
  auto frame = FrameForTest::Create(frame_host, std::move(create_frame_params));
  GURL url = test_server().GetURL("/defaultresponse");
  ASSERT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       url.spec()));
  frame.navigation_listener().RunUntilUrlEquals(url);

  // Wait for the DevTools port to become available, and then connect to it to
  // verify that there is a single debuggable page listed.
  dev_tools_listener.RunUntilNumberOfPortsIs(1);
  uint16_t remote_debugging_port = *(dev_tools_listener.debug_ports().begin());

  base::Value::List devtools_list =
      GetDevToolsListFromPort(remote_debugging_port);
  EXPECT_EQ(devtools_list.size(), 1u);
  {
    const auto* devtools_url = devtools_list[0].GetDict().FindString("url");
    ASSERT_TRUE(devtools_url);
    EXPECT_EQ(*devtools_url, url);
  }

  // Create a new `FrameHost` client, and immediately close it. The DevTools
  // port should remain open regardless.
  auto frame_host_2 = test_realm_services().Connect<fuchsia::web::FrameHost>();
  frame_host_2.Unbind();

  // Navigate to a different page. The devtools service should still be active
  // and report the new page.
  GURL url2 = test_server().GetURL("/title1.html");
  ASSERT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       url2.spec()));
  frame.navigation_listener().RunUntilUrlEquals(url2);

  devtools_list = GetDevToolsListFromPort(remote_debugging_port);
  EXPECT_EQ(devtools_list.size(), 1u);
  {
    const auto* devtools_url = devtools_list[0].GetDict().FindString("url");
    ASSERT_TRUE(devtools_url);
    EXPECT_EQ(*devtools_url, url2);
  }
}

#if defined(ARCH_CPU_ARM_FAMILY)
// TODO(crbug.com/42050537): Enable on ARM64 when bots support Vulkan.
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
  TestCastComponent component(test_realm_services());
  const char kTestPath[] = "/webgl_presence.html";
  const GURL test_url = test_server().GetURL(kTestPath);
  app_config_manager().AddApp(kTestAppId, test_url);

  component.StartCastComponentWithQueryApi();

  EXPECT_EQ(component.ExecuteJavaScript("document.title"), "present");
}

TEST_F(MAYBE_VulkanCastRunnerIntegrationTest,
       WebGLContextPresentWithVulkanFeature_IsolatedRunner) {
  TestCastComponent component(test_realm_services());
  const GURL kContentDirectoryUrl("fuchsia-dir://testdata/webgl_presence.html");

  app_config_manager().AddAppConfig(
      CreateAppConfigWithTestData(kTestAppId, kContentDirectoryUrl));
  component.StartCastComponentWithQueryApi();

  EXPECT_EQ(component.ExecuteJavaScript("document.title"), "present");
}
