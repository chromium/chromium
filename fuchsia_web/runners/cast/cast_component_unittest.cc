// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/runners/cast/cast_component.h"

#include <chromium/cast/cpp/fidl.h>
#include <fidl/chromium.cast/cpp/test_base.h>
#include <fidl/fuchsia.io/cpp/common_types.h>
#include <fidl/fuchsia.io/cpp/markers.h>
#include <fuchsia/component/runner/cpp/fidl.h>
#include <fuchsia/io/cpp/fidl.h>
#include <fuchsia/web/cpp/fidl.h>
#include <fuchsia/web/cpp/fidl_test_base.h>
#include <lib/async/default.h>
#include <lib/fidl/cpp/binding.h>
#include <lib/fidl/cpp/interface_request.h>
#include <lib/fpromise/result.h>
#include <lib/vfs/cpp/pseudo_dir.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/fuchsia/file_utils.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/mem_buffer_util.h"
#include "base/fuchsia/scoped_service_binding.h"
#include "base/fuchsia/startup_context.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chromecast/bindings/bindings_manager_fuchsia.h"
#include "components/cast/message_port/fuchsia/message_port_fuchsia.h"
#include "components/cast/message_port/test_message_port_receiver.h"
#include "fuchsia_web/common/fuchsia_dir_scheme.h"
#include "fuchsia_web/runners/cast/api_bindings_client.h"
#include "fuchsia_web/runners/cast/cast_streaming.h"
#include "fuchsia_web/runners/cast/test/fake_application_config_manager.h"
#include "fuchsia_web/runners/cast/test/scoped_port_handler.h"
#include "fuchsia_web/runners/common/web_content_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/url_util.h"

namespace {

constexpr char kTestAppId[] = "00000000";
constexpr char kCastStreamingReceiverUrl[] =
    "fuchsia-dir://cast-streaming/receiver.html";
constexpr char kCastStreamingOrigin[] = "fuchsia-dir://cast-streaming";

class FakeFrame final : public fuchsia::web::testing::Frame_TestBase {
 public:
  struct ConnectorPostMessage {
    std::string origin;
    std::string data;
    std::unique_ptr<cast_api_bindings::MessagePort> port;
  };

  FakeFrame() = default;
  ~FakeFrame() override = default;

  FakeFrame(const FakeFrame&) = delete;
  FakeFrame& operator=(const FakeFrame&) = delete;

  void WaitForNavigationListener() {
    if (navigation_listener_.is_bound()) {
      return;
    }
    ASSERT_TRUE(navigation_listener_bound_.Wait());
  }

  void SendNavigationState(fuchsia::web::NavigationState state) {
    ASSERT_TRUE(navigation_listener_.is_bound());
    base::test::TestFuture<void> ack;
    navigation_listener_->OnNavigationStateChanged(
        std::move(state),
        [quit = ack.GetCallback()]() mutable { std::move(quit).Run(); });
    ASSERT_TRUE(ack.Wait());
  }

  std::unique_ptr<cast_api_bindings::MessagePort> TakeStreamingPort() {
    return streaming_port_future_.Take();
  }

  ConnectorPostMessage TakeConnectorPostMessage() {
    return connector_post_message_.Take();
  }

  // fuchsia::web::testing::Frame_TestBase implementation:
  void NotImplemented_(const std::string& name) override {
    ADD_FAILURE() << "Unexpected Frame call: " << name;
  }

  void SetConsoleLogSink(fuchsia::logger::LogSinkHandle) override {}

  void SetContentAreaSettings(fuchsia::web::ContentAreaSettings) override {}

  void SetNavigationEventListener2(
      fidl::InterfaceHandle<fuchsia::web::NavigationEventListener> listener,
      fuchsia::web::NavigationEventListenerFlags) override {
    navigation_listener_.Bind(std::move(listener));
    if (!navigation_listener_bound_.IsReady()) {
      navigation_listener_bound_.SetValue();
    }
  }

  void AddBeforeLoadJavaScript(
      uint64_t,
      std::vector<std::string>,
      fuchsia::mem::Buffer,
      AddBeforeLoadJavaScriptCallback callback) override {
    callback(fpromise::ok());
  }

  void RemoveBeforeLoadJavaScript(uint64_t) override {}

  void SetUrlRequestRewriteRules(
      std::vector<fuchsia::web::UrlRequestRewriteRule>,
      SetUrlRequestRewriteRulesCallback callback) override {
    pending_rewrite_rules_callback_ = std::move(callback);
  }

  void SetMediaSettings(fuchsia::web::FrameMediaSettings) override {}

  void ConfigureInputTypes(fuchsia::web::InputTypes,
                           fuchsia::web::AllowInputState) override {}

  void SetPermissionState(fuchsia::web::PermissionDescriptor,
                          std::string,
                          fuchsia::web::PermissionState) override {}

  void SetBlockMediaLoading(bool) override {}

  void PostMessage(std::string origin,
                   fuchsia::web::WebMessage message,
                   PostMessageCallback callback) override {
    if (origin == "cast-streaming:receiver") {
      ASSERT_TRUE(message.has_outgoing_transfer());
      ASSERT_EQ(message.outgoing_transfer().size(), 1u);
      ASSERT_TRUE(message.outgoing_transfer()[0].is_message_port());
      streaming_port_future_.SetValue(
          cast_api_bindings::MessagePortFuchsia::Create(std::move(
              (*message.mutable_outgoing_transfer())[0].message_port())));
    } else {
      ConnectorPostMessage posted;
      posted.origin = std::move(origin);
      if (message.has_data()) {
        posted.data =
            base::StringFromMemBuffer(message.data()).value_or(std::string());
      }
      if (message.has_outgoing_transfer() &&
          !message.outgoing_transfer().empty() &&
          message.outgoing_transfer()[0].is_message_port()) {
        posted.port = cast_api_bindings::MessagePortFuchsia::Create(std::move(
            (*message.mutable_outgoing_transfer())[0].message_port()));
      }
      connector_post_message_.SetValue(std::move(posted));
    }
    callback(fpromise::ok());
  }

 private:
  fuchsia::web::NavigationEventListenerPtr navigation_listener_;
  base::test::TestFuture<void> navigation_listener_bound_;
  base::test::TestFuture<std::unique_ptr<cast_api_bindings::MessagePort>>
      streaming_port_future_;
  base::test::TestFuture<ConnectorPostMessage> connector_post_message_;
  SetUrlRequestRewriteRulesCallback pending_rewrite_rules_callback_;
};

class FakeWebContext final : public fuchsia::web::testing::Context_TestBase {
 public:
  FakeWebContext() = default;
  ~FakeWebContext() override = default;

  FakeWebContext(const FakeWebContext&) = delete;
  FakeWebContext& operator=(const FakeWebContext&) = delete;

  FakeFrame& frame() { return frame_; }

  // fuchsia::web::testing::Context_TestBase implementation:
  void NotImplemented_(const std::string& name) override {
    ADD_FAILURE() << "Unexpected Context call: " << name;
  }

  void CreateFrameWithParams(
      fuchsia::web::CreateFrameParams,
      fidl::InterfaceRequest<fuchsia::web::Frame> frame) override {
    frame_binding_.Bind(std::move(frame));
  }

 private:
  FakeFrame frame_;
  fidl::Binding<fuchsia::web::Frame> frame_binding_{&frame_};
};

class FakeApplicationContext final
    : public fidl::testing::TestBase<chromium_cast::ApplicationContext> {
 public:
  FakeApplicationContext() = default;
  ~FakeApplicationContext() override = default;

  void NotImplemented_(const std::string&, ::fidl::CompleterBase&) override {}

  void SetApplicationController(
      SetApplicationControllerRequest& request,
      SetApplicationControllerCompleter::Sync&) override {
    application_controller_ = std::move(request.controller());
  }

 private:
  fidl::ClientEnd<chromium_cast::ApplicationController> application_controller_;
};

class CastComponentTest : public testing::Test {
 protected:
  CastComponentTest()
      : api_bindings_binding_(&bindings_manager_),
        context_binding_(&web_instance_services_, &fake_context_),
        runner_(base::BindLambdaForTesting(
                    [this](fuchsia::web::CreateContextParams,
                           fidl::InterfaceRequest<fuchsia::io::Directory>
                               services_request,
                           const base::CommandLine&) -> zx_status_t {
                      return web_instance_services_.Serve(
                          fuchsia_io::wire::kPermReadable,
                          fidl::ServerEnd<fuchsia_io::Directory>(
                              services_request.TakeChannel()));
                    }),
                base::BindRepeating(
                    []() { return WebContentRunner::WebInstanceConfig(); })) {}

  void StartComponent(std::string_view debug_name, const GURL& app_url) {
    ASSERT_TRUE(bindings_manager_.origin().empty());

    base::test::TestFuture<void> bindings_ready;
    auto api_bindings_client = std::make_unique<ApiBindingsClient>(
        api_bindings_binding_.NewBinding(), bindings_ready.GetCallback());
    ASSERT_TRUE(bindings_ready.Wait());
    ASSERT_TRUE(api_bindings_client->HasBindings());

    fuchsia::component::runner::ComponentStartInfo start_info;
    fidl::InterfaceHandle<fuchsia::io::Directory> svc_dir;
    svc_dir_request_ = svc_dir.NewRequest();
    auto& svc_entry = start_info.mutable_ns()->emplace_back();
    svc_entry.set_path(base::kServiceDirectoryPath);
    svc_entry.set_directory(std::move(svc_dir));

    CastComponent::Params params;
    params.startup_context =
        std::make_unique<base::StartupContext>(std::move(start_info));
    params.controller_request = component_controller_.NewRequest();
    rewrite_rules_request_ = params.url_rewrite_rules_provider.NewRequest();
    params.api_bindings_client = std::move(api_bindings_client);
    params.application_config =
        FakeApplicationConfigManager::CreateConfig(kTestAppId, app_url);

    auto app_context_endpoints =
        fidl::CreateEndpoints<chromium_cast::ApplicationContext>();
    ZX_CHECK(app_context_endpoints.is_ok(),
             app_context_endpoints.status_value());
    app_context_binding_.emplace(async_get_default_dispatcher(),
                                 std::move(app_context_endpoints->server),
                                 &fake_app_context_, [](fidl::UnbindInfo) {});
    params.application_context = std::move(app_context_endpoints->client);
    params.initial_url_rewrite_rules.emplace();
    params.media_settings.emplace();
    params.trace_flow_id = 0;
    ASSERT_TRUE(params.AreComplete());

    auto component = std::make_unique<CastComponent>(
        debug_name, &runner_, std::move(params), /*is_headless=*/true);
    component->StartComponent();
    runner_.RegisterComponent(std::move(component));

    fake_context_.frame().WaitForNavigationListener();
  }

  void KillComponent() {
    base::test::TestFuture<void> runner_empty;
    runner_.SetOnEmptyCallback(runner_empty.GetCallback());
    component_controller_->Kill();
    ASSERT_TRUE(runner_empty.Wait());
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
  chromecast::bindings::BindingsManagerFuchsia bindings_manager_;
  fidl::Binding<chromium::cast::ApiBindings> api_bindings_binding_;
  FakeWebContext fake_context_;
  vfs::PseudoDir web_instance_services_;
  base::ScopedServiceBinding<fuchsia::web::Context> context_binding_;
  fidl::InterfaceRequest<fuchsia::io::Directory> svc_dir_request_;
  fidl::InterfaceRequest<chromium::cast::UrlRequestRewriteRulesProvider>
      rewrite_rules_request_;
  FakeApplicationContext fake_app_context_;
  std::optional<fidl::ServerBinding<chromium_cast::ApplicationContext>>
      app_context_binding_;
  fuchsia::component::runner::ComponentControllerPtr component_controller_;
  WebContentRunner runner_;
};

TEST_F(CastComponentTest,
       CastStreamingPreservesTransportPortAndOriginAcrossLoad) {
  url::ScopedSchemeRegistryForTests scoped_scheme_registry;
  RegisterFuchsiaDirScheme();

  ScopedPortHandler transport_handler(bindings_manager_,
                                      kCastStreamingMessagePortName);
  StartComponent("cast_streaming_test", GURL("cast-streaming:receiver"));

  // Verify StartComponent() sets the Cast Streaming origin and connects both
  // ends of the cast_transport port without pre-seeding origin state.
  cast_api_bindings::TestMessagePortReceiver agent_receiver;
  std::unique_ptr<cast_api_bindings::MessagePort> agent_transport_port =
      transport_handler.RunUntilPortConnected();
  ASSERT_TRUE(agent_transport_port);
  agent_transport_port->SetReceiver(&agent_receiver);

  cast_api_bindings::TestMessagePortReceiver web_engine_receiver;
  std::unique_ptr<cast_api_bindings::MessagePort> web_engine_transport_port =
      fake_context_.frame().TakeStreamingPort();
  ASSERT_TRUE(web_engine_transport_port);
  web_engine_transport_port->SetReceiver(&web_engine_receiver);

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return bindings_manager_.origin() == kCastStreamingOrigin; }));

  // Simulate Cast Streaming receiver document loading and verify the skip in
  // CastComponent::MaybeConnectPortConnector() preserves the transport port and
  // tracked origin.
  fuchsia::web::NavigationState load_started;
  load_started.set_url(kCastStreamingReceiverUrl);
  load_started.set_is_main_document_loaded(false);
  fake_context_.frame().SendNavigationState(std::move(load_started));

  fuchsia::web::NavigationState load_completed;
  load_completed.set_is_main_document_loaded(true);
  fake_context_.frame().SendNavigationState(std::move(load_completed));

  EXPECT_EQ(fake_context_.frame().TakeConnectorPostMessage().origin,
            kCastStreamingOrigin);

  EXPECT_TRUE(agent_transport_port->PostMessage("still_alive"));
  ASSERT_TRUE(web_engine_receiver.RunUntilMessageCountEqual(1));
  EXPECT_EQ(web_engine_receiver.buffer()[0].first, "still_alive");
  EXPECT_EQ(bindings_manager_.origin(), kCastStreamingOrigin);

  KillComponent();
}

TEST_F(CastComponentTest, NonStreamingWaitsForHandshakeAckBeforeSettingOrigin) {
  constexpr char kAppUrl[] = "https://example.com/app.html";
  constexpr char kExpectedOrigin[] = "https://example.com";

  StartComponent("non_streaming_test", GURL(kAppUrl));

  fuchsia::web::NavigationState load_completed;
  load_completed.set_url(kAppUrl);
  load_completed.set_is_main_document_loaded(true);
  fake_context_.frame().SendNavigationState(std::move(load_completed));

  cast_api_bindings::TestMessagePortReceiver control_port_receiver;
  FakeFrame::ConnectorPostMessage handshake =
      fake_context_.frame().TakeConnectorPostMessage();
  EXPECT_EQ(handshake.origin, kExpectedOrigin);
  ASSERT_TRUE(handshake.port);
  handshake.port->SetReceiver(&control_port_receiver);

  // Drive a non-load navigation event so any eager SetOrigin() FIDL call from
  // MaybeConnectPortConnector() would already have been dispatched.
  fuchsia::web::NavigationState title_update;
  title_update.set_title("loaded");
  fake_context_.frame().SendNavigationState(std::move(title_update));
  EXPECT_TRUE(bindings_manager_.origin().empty());

  // Reply with the connector handshake ack carrying the handshake token and
  // verify CastComponent now updates the tracked origin.
  std::optional<base::DictValue> connect_dict =
      base::JSONReader::ReadDict(handshake.data, base::JSON_PARSE_RFC);
  ASSERT_TRUE(connect_dict.has_value());
  const std::string* token = connect_dict->FindString("token");
  ASSERT_TRUE(token);

  base::DictValue ack_payload;
  // Protocol port ID required by NamedMessagePortConnector.
  ack_payload.Set("portId", "cast.master.connected");  // nocheck
  ack_payload.Set("token", *token);
  std::optional<std::string> ack_json = base::WriteJson(ack_payload);
  ASSERT_TRUE(ack_json.has_value());
  ASSERT_TRUE(handshake.port->PostMessage(*ack_json));

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return bindings_manager_.origin() == kExpectedOrigin; }));

  KillComponent();
}

}  // namespace
