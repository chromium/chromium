// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/mediacodec/cpp/fidl.h>
#include <fuchsia/sys/cpp/fidl.h>
#include <fuchsia/web/cpp/fidl.h>
#include <lib/fdio/directory.h>
#include <lib/fidl/cpp/binding.h>
#include <lib/sys/cpp/component_context.h>
#include <zircon/processargs.h>

#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/fuchsia/file_utils.h"
#include "base/fuchsia/filtered_service_directory.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/fuchsia/scoped_service_binding.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/test/task_environment.h"
#include "fuchsia/base/context_provider_test_connector.h"
#include "fuchsia/base/fit_adapter.h"
#include "fuchsia/base/frame_test_util.h"
#include "fuchsia/base/result_receiver.h"
#include "fuchsia/base/test_devtools_list_fetcher.h"
#include "fuchsia/base/test_navigation_listener.h"
#include "media/base/media_switches.h"
#include "media/fuchsia/audio/fake_audio_consumer.h"
#include "media/fuchsia/camera/fake_fuchsia_camera.h"
#include "net/base/test_completion_callback.h"
#include "net/http/http_request_headers.h"
#include "net/socket/tcp_client_socket.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kValidUserAgentProduct[] = "TestProduct";
constexpr char kValidUserAgentVersion[] = "dev.12345";
constexpr char kValidUserAgentProductAndVersion[] = "TestProduct/dev.12345";
constexpr char kInvalidUserAgentProduct[] = "Test/Product";
constexpr char kInvalidUserAgentVersion[] = "dev/12345";

fuchsia::web::ContentDirectoryProvider CreateTestDataDirectoryProvider() {
  fuchsia::web::ContentDirectoryProvider provider;
  provider.set_name("testdata");
  base::FilePath pkg_path;
  CHECK(base::PathService::Get(base::DIR_ASSETS, &pkg_path));
  provider.set_directory(base::fuchsia::OpenDirectory(
      pkg_path.AppendASCII("fuchsia/engine/test/data")));
  return provider;
}

}  // namespace

class WebEngineIntegrationTestBase : public testing::Test {
 public:
  WebEngineIntegrationTestBase()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {}
  ~WebEngineIntegrationTestBase() override = default;

  void SetUp() override {
    embedded_test_server_.ServeFilesFromSourceDirectory(
        "fuchsia/engine/test/data");
    net::test_server::RegisterDefaultHandlers(&embedded_test_server_);
    ASSERT_TRUE(embedded_test_server_.Start());
  }

  void StartWebEngine(base::CommandLine command_line) {
    web_context_provider_ = cr_fuchsia::ConnectContextProvider(
        web_engine_controller_.NewRequest(), std::move(command_line));
    web_context_provider_.set_error_handler(
        [](zx_status_t status) { ADD_FAILURE(); });
  }

  fuchsia::web::CreateContextParams DefaultContextParams() const {
    fuchsia::web::CreateContextParams create_params;
    auto directory = base::fuchsia::OpenDirectory(
        base::FilePath(base::fuchsia::kServiceDirectoryPath));
    EXPECT_TRUE(directory.is_valid());
    create_params.set_service_directory(std::move(directory));
    return create_params;
  }

  fuchsia::web::CreateContextParams DefaultContextParamsWithTestData() const {
    fuchsia::web::CreateContextParams create_params = DefaultContextParams();

    fuchsia::web::ContentDirectoryProvider provider;
    provider.set_name("testdata");
    base::FilePath pkg_path;
    CHECK(base::PathService::Get(base::DIR_ASSETS, &pkg_path));
    provider.set_directory(base::fuchsia::OpenDirectory(
        pkg_path.AppendASCII("fuchsia/engine/test/data")));

    create_params.mutable_content_directories()->emplace_back(
        std::move(provider));

    return create_params;
  }

  fuchsia::web::CreateContextParams
  ContextParamsWithFilteredServiceDirectory() {
    filtered_service_directory_ =
        std::make_unique<base::fuchsia::FilteredServiceDirectory>(
            base::ComponentContextForProcess()->svc().get());
    fidl::InterfaceHandle<fuchsia::io::Directory> svc_dir;
    filtered_service_directory_->ConnectClient(svc_dir.NewRequest());

    // Push all services from /svc to the service directory.
    base::FileEnumerator file_enum(base::FilePath("/svc"), false,
                                   base::FileEnumerator::FILES);
    for (auto file = file_enum.Next(); !file.empty(); file = file_enum.Next()) {
      filtered_service_directory_->AddService(file.BaseName().value().c_str());
    }

    fuchsia::web::CreateContextParams create_params;
    create_params.set_service_directory(std::move(svc_dir));
    return create_params;
  }

  // Returns CreateContextParams that has AUDIO feature enabled with an injected
  // FakeAudioConsumerService.
  fuchsia::web::CreateContextParams ContextParamsWithAudio() {
    // Use a FilteredServiceDirectory in order to inject a fake AudioConsumer
    // service.
    fuchsia::web::CreateContextParams create_params =
        ContextParamsWithFilteredServiceDirectory();
    create_params.set_features(fuchsia::web::ContextFeatureFlags::AUDIO);

    fake_audio_consumer_service_ =
        std::make_unique<media::FakeAudioConsumerService>(
            filtered_service_directory_->outgoing_directory()
                ->GetOrCreateDirectory("svc"));

    return create_params;
  }

  // Returns the same CreateContextParams as ContextParamsWithAudio() plus the
  // testdata content directory.
  fuchsia::web::CreateContextParams ContextParamsWithAudioAndTestData() {
    fuchsia::web::CreateContextParams create_params = ContextParamsWithAudio();
    create_params.mutable_content_directories()->push_back(
        CreateTestDataDirectoryProvider());
    return create_params;
  }

  // Populates |navigation_listener_| with a TestNavigationListener and adds it
  // to |frame|, enabling tests to monitor the state of the Frame.
  // May only be called once.
  void CreateNavigationListener(fuchsia::web::FramePtr* frame) {
    DCHECK(!navigation_listener_);
    navigation_listener_ =
        std::make_unique<cr_fuchsia::TestNavigationListener>();
    navigation_listener_binding_ =
        std::make_unique<fidl::Binding<fuchsia::web::NavigationEventListener>>(
            navigation_listener_.get());
    (*frame)->SetNavigationEventListener(
        navigation_listener_binding_->NewBinding());
  }

  // Populates |navigation_controller_| with a NavigationController for |frame|.
  // May only be called once.
  void AddNavigationControllerAndListenerToFrame(
      fuchsia::web::FramePtr* frame) {
    DCHECK(!navigation_controller_);

    (*frame)->GetNavigationController(navigation_controller_.NewRequest());
    navigation_controller_.set_error_handler(
        [](zx_status_t status) { ADD_FAILURE(); });

    CreateNavigationListener(frame);
  }

  // Populates |context_| with a Context with |params|.
  void CreateContext(fuchsia::web::CreateContextParams context_params) {
    DCHECK(!context_);
    web_context_provider_->Create(std::move(context_params),
                                  context_.NewRequest());
    context_.set_error_handler([](zx_status_t status) { ADD_FAILURE(); });
  }

  // Returns a new Frame created from |context_|.
  fuchsia::web::FramePtr CreateFrame() {
    DCHECK(context_);
    fuchsia::web::FramePtr frame;
    context_->CreateFrame(frame.NewRequest());
    frame.set_error_handler([](zx_status_t status) { ADD_FAILURE(); });
    return frame;
  }

  // Returns a new Frame with |frame_params| created from |context_|.
  fuchsia::web::FramePtr CreateFrameWithParams(
      fuchsia::web::CreateFrameParams frame_params) {
    DCHECK(context_);
    fuchsia::web::FramePtr frame;
    context_->CreateFrameWithParams(std::move(frame_params),
                                    frame.NewRequest());
    frame.set_error_handler([](zx_status_t status) { ADD_FAILURE(); });
    return frame;
  }

  // Populates |context_| with a Context with |context_params|, |frame_| with a
  // new Frame, |navigation_controller_| with a NavigationController request for
  // |frame_|, and navigation_listener_| with a TestNavigationListener that is
  // added to |frame|.
  void CreateContextAndFrame(fuchsia::web::CreateContextParams context_params) {
    ASSERT_FALSE(frame_);

    CreateContext(std::move(context_params));

    frame_ = CreateFrame();
    AddNavigationControllerAndListenerToFrame(&frame_);
  }

  // Same as CreateContextAndFrame() but uses |frame_params| to create the
  // Frame.
  void CreateContextAndFrameWithParams(
      fuchsia::web::CreateContextParams context_params,
      fuchsia::web::CreateFrameParams frame_params) {
    ASSERT_FALSE(frame_);

    CreateContext(std::move(context_params));

    frame_ = CreateFrameWithParams(std::move(frame_params));
    AddNavigationControllerAndListenerToFrame(&frame_);
  }

  void CreateContextAndExpectError(fuchsia::web::CreateContextParams params,
                                   zx_status_t expected_error) {
    ASSERT_FALSE(context_);
    web_context_provider_->Create(std::move(params), context_.NewRequest());
    base::RunLoop run_loop;
    context_.set_error_handler([&run_loop, expected_error](zx_status_t status) {
      EXPECT_EQ(status, expected_error);
      run_loop.Quit();
    });
    run_loop.Run();
  }

  void CreateContextAndFrameAndLoadUrl(fuchsia::web::CreateContextParams params,
                                       const GURL& url) {
    CreateContextAndFrame(std::move(params));

    // Navigate the Frame to |url| and wait for it to complete loading.
    fuchsia::web::LoadUrlParams load_url_params;
    ASSERT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
        navigation_controller_.get(), std::move(load_url_params), url.spec()));

    // Wait for the URL to finish loading.
    navigation_listener_->RunUntilUrlEquals(url);
  }

  void LoadUrlWithUserActivation(base::StringPiece url) {
    EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
        navigation_controller_.get(),
        cr_fuchsia::CreateLoadUrlParamsWithUserActivation(), url));
  }

  void GrantPermission(fuchsia::web::PermissionType type,
                       const std::string& origin) {
    fuchsia::web::PermissionDescriptor permission;
    permission.set_type(type);
    frame_->SetPermissionState(std::move(permission), origin,
                               fuchsia::web::PermissionState::GRANTED);
  }

  std::string ExecuteJavaScriptWithStringResult(base::StringPiece script) {
    base::Optional<base::Value> value =
        cr_fuchsia::ExecuteJavaScript(frame_.get(), script);
    return value ? value->GetString() : std::string();
  }

  double ExecuteJavaScriptWithDoubleResult(base::StringPiece script) {
    base::Optional<base::Value> value =
        cr_fuchsia::ExecuteJavaScript(frame_.get(), script);
    return value ? value->GetDouble() : 0.0;
  }

  bool ExecuteJavaScriptWithBoolResult(base::StringPiece script) {
    base::Optional<base::Value> value =
        cr_fuchsia::ExecuteJavaScript(frame_.get(), script);
    return value ? value->GetBool() : false;
  }

  void RunCameraTest(bool grant_permission);

 protected:
  void RunPermissionTest(bool grant);

  const base::test::TaskEnvironment task_environment_;

  fidl::InterfaceHandle<fuchsia::sys::ComponentController>
      web_engine_controller_;
  fuchsia::web::ContextProviderPtr web_context_provider_;

  net::EmbeddedTestServer embedded_test_server_;

  fuchsia::web::ContextPtr context_;
  fuchsia::web::FramePtr frame_;
  fuchsia::web::NavigationControllerPtr navigation_controller_;

  std::unique_ptr<cr_fuchsia::TestNavigationListener> navigation_listener_;
  std::unique_ptr<fidl::Binding<fuchsia::web::NavigationEventListener>>
      navigation_listener_binding_;

  std::unique_ptr<base::fuchsia::FilteredServiceDirectory>
      filtered_service_directory_;

  std::unique_ptr<media::FakeAudioConsumerService> fake_audio_consumer_service_;

  DISALLOW_COPY_AND_ASSIGN(WebEngineIntegrationTestBase);
};

// Starts a WebEngine instance before running the test.
class WebEngineIntegrationTest : public WebEngineIntegrationTestBase {
 protected:
  WebEngineIntegrationTest() : WebEngineIntegrationTestBase() {}

  void SetUp() override {
    WebEngineIntegrationTestBase::SetUp();

    StartWebEngine(base::CommandLine(base::CommandLine::NO_PROGRAM));
  }
};

class WebEngineIntegrationUserAgentTest : public WebEngineIntegrationTest {
 protected:
  GURL GetEchoUserAgentUrl() {
    static std::string echo_user_agent_header_path =
        std::string("/echoheader?") + net::HttpRequestHeaders::kUserAgent;
    return embedded_test_server_.GetURL(echo_user_agent_header_path);
  }
};

TEST_F(WebEngineIntegrationUserAgentTest, ValidProductOnly) {
  // Create a Context with just an embedder product specified.
  fuchsia::web::CreateContextParams create_params = DefaultContextParams();
  create_params.set_user_agent_product(kValidUserAgentProduct);
  CreateContextAndFrameAndLoadUrl(std::move(create_params),
                                  GetEchoUserAgentUrl());

  // Query & verify that the header echoed into the document body contains
  // the product tag.
  std::string result =
      ExecuteJavaScriptWithStringResult("document.body.innerText;");
  EXPECT_TRUE(result.find(kValidUserAgentProduct) != std::string::npos);

  // Query & verify that the navigator.userAgent contains the product tag.
  result = ExecuteJavaScriptWithStringResult("navigator.userAgent;");
  EXPECT_TRUE(result.find(kValidUserAgentProduct) != std::string::npos);
}

TEST_F(WebEngineIntegrationUserAgentTest, ValidProductAndVersion) {
  // Create a Context with both product and version specified.
  fuchsia::web::CreateContextParams create_params = DefaultContextParams();
  create_params.set_user_agent_product(kValidUserAgentProduct);
  create_params.set_user_agent_version(kValidUserAgentVersion);
  CreateContextAndFrameAndLoadUrl(std::move(create_params),
                                  GetEchoUserAgentUrl());

  // Query & verify that the header echoed into the document body contains
  // both product & version.
  std::string result =
      ExecuteJavaScriptWithStringResult("document.body.innerText;");
  EXPECT_TRUE(result.find(kValidUserAgentProductAndVersion) !=
              std::string::npos);

  // Query & verify that the navigator.userAgent contains product & version.
  result = ExecuteJavaScriptWithStringResult("navigator.userAgent;");
  EXPECT_TRUE(result.find(kValidUserAgentProductAndVersion) !=
              std::string::npos);
}

TEST_F(WebEngineIntegrationUserAgentTest, InvalidProduct) {
  // Try to create a Context with an invalid embedder product tag.
  fuchsia::web::CreateContextParams create_params = DefaultContextParams();
  create_params.set_user_agent_product(kInvalidUserAgentProduct);
  CreateContextAndExpectError(std::move(create_params), ZX_ERR_INVALID_ARGS);
}

TEST_F(WebEngineIntegrationUserAgentTest, VersionOnly) {
  // Try to create a Context with an embedder version but no product.
  fuchsia::web::CreateContextParams create_params = DefaultContextParams();
  create_params.set_user_agent_version(kValidUserAgentVersion);
  CreateContextAndExpectError(std::move(create_params), ZX_ERR_INVALID_ARGS);
}

TEST_F(WebEngineIntegrationUserAgentTest, ValidProductAndInvalidVersion) {
  // Try to create a Context with valid product tag, but invalid version.
  fuchsia::web::CreateContextParams create_params = DefaultContextParams();
  create_params.set_user_agent_product(kValidUserAgentProduct);
  create_params.set_user_agent_version(kInvalidUserAgentVersion);
  CreateContextAndExpectError(std::move(create_params), ZX_ERR_INVALID_ARGS);
}

// Check that if the CreateContextParams has |remote_debugging_port| set then:
// - DevTools becomes available when the first debuggable Frame is created.
// - DevTools closes when the last debuggable Frame is closed.
TEST_F(WebEngineIntegrationTest, RemoteDebuggingPort) {
  // Create a Context with remote debugging enabled via an ephemeral port.
  fuchsia::web::CreateContextParams create_params = DefaultContextParams();
  create_params.set_remote_debugging_port(0);

  // Create a Frame with remote debugging enabled.
  fuchsia::web::CreateFrameParams create_frame_params;
  create_frame_params.set_enable_remote_debugging(true);
  CreateContextAndFrameWithParams(std::move(create_params),
                                  std::move(create_frame_params));

  // Expect to receive a notification of the selected DevTools port.
  base::RunLoop run_loop;
  cr_fuchsia::ResultReceiver<
      fuchsia::web::Context_GetRemoteDebuggingPort_Result>
      port_receiver(run_loop.QuitClosure());
  context_->GetRemoteDebuggingPort(
      cr_fuchsia::CallbackToFitFunction(port_receiver.GetReceiveCallback()));
  run_loop.Run();

  ASSERT_TRUE(port_receiver->is_response());
  uint16_t remote_debugging_port = port_receiver->response().port;
  ASSERT_TRUE(remote_debugging_port != 0);

  // Navigate to a URL.
  GURL url = embedded_test_server_.GetURL("/defaultresponse");
  ASSERT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      navigation_controller_.get(), fuchsia::web::LoadUrlParams(), url.spec()));
  navigation_listener_->RunUntilUrlEquals(url);

  base::Value devtools_list =
      cr_fuchsia::GetDevToolsListFromPort(remote_debugging_port);
  ASSERT_TRUE(devtools_list.is_list());
  EXPECT_EQ(devtools_list.GetList().size(), 1u);

  base::Value* devtools_url = devtools_list.GetList()[0].FindPath("url");
  ASSERT_TRUE(devtools_url->is_string());
  EXPECT_EQ(devtools_url->GetString(), url);

  // Create a second frame, without remote debugging enabled. The remote
  // debugging service should still report a single Frame is present.
  fuchsia::web::FramePtr web_frame2 = CreateFrame();

  devtools_list = cr_fuchsia::GetDevToolsListFromPort(remote_debugging_port);
  ASSERT_TRUE(devtools_list.is_list());
  EXPECT_EQ(devtools_list.GetList().size(), 1u);

  devtools_url = devtools_list.GetList()[0].FindPath("url");
  ASSERT_TRUE(devtools_url->is_string());
  EXPECT_EQ(devtools_url->GetString(), url);

  // Tear down the debuggable Frame. The remote debugging service should have
  // shut down.
  base::RunLoop controller_run_loop;
  navigation_controller_.set_error_handler(
      [&controller_run_loop](zx_status_t) { controller_run_loop.Quit(); });
  frame_.Unbind();

  // Wait until the NavigationController shuts down to ensure WebEngine has
  // handled the Frame tear down.
  controller_run_loop.Run();

  // Verify that devtools server is shut down properly. WebEngine may shutdown
  // the socket after shutting down the Frame, so make several attempts to
  // connect until it fails. Don't try to read or write from/to the socket to
  // avoid fxb/49779.
  bool failed_to_connect = false;
  for (int i = 0; i < 10; ++i) {
    net::TestCompletionCallback connect_callback;
    net::TCPClientSocket connecting_socket(
        net::AddressList(net::IPEndPoint(net::IPAddress::IPv4Localhost(),
                                         remote_debugging_port)),
        nullptr, nullptr, nullptr, net::NetLogSource());
    int connect_result = connecting_socket.Connect(connect_callback.callback());
    connect_result = connect_callback.GetResult(connect_result);

    if (connect_result == net::OK) {
      // If Connect() succeeded then try again a bit later.
      base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(10));
      continue;
    }

    EXPECT_EQ(connect_result, net::ERR_CONNECTION_REFUSED);
    failed_to_connect = true;
    break;
  }

  EXPECT_TRUE(failed_to_connect);
}

// Check that remote debugging requests for Frames in non-debuggable Contexts
// cause an error to be reported.
TEST_F(WebEngineIntegrationTest, RequestDebuggableFrameInNonDebuggableContext) {
  fuchsia::web::CreateFrameParams create_frame_params;
  create_frame_params.set_enable_remote_debugging(true);
  CreateContextAndFrameWithParams(DefaultContextParams(),
                                  std::move(create_frame_params));

  base::RunLoop loop;
  frame_.set_error_handler(
      [quit_loop = loop.QuitClosure()](zx_status_t status) {
        EXPECT_EQ(status, ZX_ERR_INVALID_ARGS);
        quit_loop.Run();
      });
  loop.Run();
}

// Navigates to a resource served under the "testdata" ContentDirectory.
TEST_F(WebEngineIntegrationTest, ContentDirectoryProvider) {
  const GURL kUrl("fuchsia-dir://testdata/title1.html");
  constexpr char kTitle[] = "title 1";

  CreateContextAndFrame(DefaultContextParamsWithTestData());

  // Navigate to test1.html and verify that the resource was correctly
  // downloaded and interpreted by inspecting the document title.
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      navigation_controller_.get(), fuchsia::web::LoadUrlParams(),
      kUrl.spec()));
  navigation_listener_->RunUntilUrlAndTitleEquals(kUrl, kTitle);
}

TEST_F(WebEngineIntegrationTest, PlayAudio) {
  CreateContextAndFrame(ContextParamsWithAudioAndTestData());

  static uint16_t kTestMediaSessionId = 43;
  frame_->SetMediaSessionId(kTestMediaSessionId);

  LoadUrlWithUserActivation("fuchsia-dir://testdata/play_audio.html");

  navigation_listener_->RunUntilTitleEquals("ended");

  ASSERT_EQ(fake_audio_consumer_service_->num_instances(), 1U);

  auto pos = fake_audio_consumer_service_->instance(0)->GetMediaPosition();
  EXPECT_GT(pos, base::TimeDelta::FromSecondsD(2.0));
  EXPECT_LT(pos, base::TimeDelta::FromSecondsD(2.5));

  EXPECT_EQ(fake_audio_consumer_service_->instance(0)->session_id(),
            kTestMediaSessionId);
  EXPECT_EQ(fake_audio_consumer_service_->instance(0)->volume(), 1.0);
  EXPECT_FALSE(fake_audio_consumer_service_->instance(0)->is_muted());
}

// Check that audio cannot play when the AUDIO ContextFeatureFlag is not
// provided.
TEST_F(WebEngineIntegrationTest, PlayAudio_NoFlag) {
  // Both FilteredServiceDirectory and test data are needed.
  fuchsia::web::CreateContextParams create_params =
      ContextParamsWithFilteredServiceDirectory();
  create_params.mutable_content_directories()->push_back(
      CreateTestDataDirectoryProvider());
  CreateContextAndFrame(std::move(create_params));

  bool is_requested = false;
  filtered_service_directory_->outgoing_directory()->AddPublicService(
      std::make_unique<vfs::Service>(
          [&is_requested](zx::channel channel, async_dispatcher_t* dispatcher) {
            is_requested = true;
          }),
      fuchsia::media::SessionAudioConsumerFactory::Name_);

  LoadUrlWithUserActivation("fuchsia-dir://testdata/play_audio.html");

  navigation_listener_->RunUntilTitleEquals("error");
  EXPECT_FALSE(is_requested);
}

TEST_F(WebEngineIntegrationTest, PlayVideo) {
  CreateContextAndFrame(ContextParamsWithAudioAndTestData());

  LoadUrlWithUserActivation("fuchsia-dir://testdata/play_video.html?autoplay");

  navigation_listener_->RunUntilTitleEquals("ended");
}

void WebEngineIntegrationTestBase::RunPermissionTest(bool grant) {
  CreateContextAndFrame(DefaultContextParamsWithTestData());

  if (grant) {
    GrantPermission(fuchsia::web::PermissionType::MICROPHONE,
                    "fuchsia-dir://testdata/");
  }

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      navigation_controller_.get(), fuchsia::web::LoadUrlParams(),
      "fuchsia-dir://testdata/check_mic_permission.html"));

  navigation_listener_->RunUntilTitleEquals(grant ? "granted" : "denied");
}

TEST_F(WebEngineIntegrationTest, PermissionDenied) {
  RunPermissionTest(false);
}

TEST_F(WebEngineIntegrationTest, PermissionGranted) {
  RunPermissionTest(true);
}

TEST_F(WebEngineIntegrationTest, MicrophoneAccess_WithPermission) {
  CreateContextAndFrame(ContextParamsWithAudio());

  GrantPermission(fuchsia::web::PermissionType::MICROPHONE,
                  embedded_test_server_.GetURL("/").GetOrigin().spec());

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      navigation_controller_.get(), fuchsia::web::LoadUrlParams(),
      embedded_test_server_.GetURL("/mic.html").spec()));

  navigation_listener_->RunUntilTitleEquals("ended");
}

TEST_F(WebEngineIntegrationTest, MicrophoneAccess_WithoutPermission) {
  CreateContextAndFrame(ContextParamsWithAudio());

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      navigation_controller_.get(), fuchsia::web::LoadUrlParams(),
      embedded_test_server_.GetURL("/mic.html?NoPermission").spec()));

  navigation_listener_->RunUntilTitleEquals("ended");
}

TEST_F(WebEngineIntegrationTest, SetBlockMediaLoading_Blocked) {
  CreateContextAndFrame(ContextParamsWithAudioAndTestData());

  frame_->SetBlockMediaLoading(true);

  LoadUrlWithUserActivation("fuchsia-dir://testdata/play_video.html?autoplay");

  // Check different indicators that media has not loaded and is not playing.
  navigation_listener_->RunUntilTitleEquals("stalled");
  EXPECT_EQ(0 /*HAVE_NOTHING*/,
            ExecuteJavaScriptWithDoubleResult("bear.readyState"));
  EXPECT_EQ(0.0, ExecuteJavaScriptWithDoubleResult("bear.currentTime"));
  EXPECT_FALSE(ExecuteJavaScriptWithBoolResult("isMetadataLoaded"));
}

// Initially, set media blocking to be true. When media is unblocked, check that
// it begins playing, since autoplay=true.
TEST_F(WebEngineIntegrationTest, SetBlockMediaLoading_AfterUnblock) {
  CreateContextAndFrame(ContextParamsWithAudioAndTestData());

  frame_->SetBlockMediaLoading(true);

  LoadUrlWithUserActivation("fuchsia-dir://testdata/play_video.html?autoplay");

  // Check that media loading has been blocked.
  navigation_listener_->RunUntilTitleEquals("stalled");

  // Unblock media from loading and see if media loads and plays, since
  // autoplay=true.
  frame_->SetBlockMediaLoading(false);
  navigation_listener_->RunUntilTitleEquals("playing");
  EXPECT_TRUE(ExecuteJavaScriptWithBoolResult("isMetadataLoaded"));
}

// Check that when autoplay=false and media loading was blocked after the
// element has started loading that media will play when play() is called.
TEST_F(WebEngineIntegrationTest, SetBlockMediaLoading_SetBlockedAfterLoading) {
  CreateContextAndFrame(ContextParamsWithAudioAndTestData());

  LoadUrlWithUserActivation("fuchsia-dir://testdata/play_video.html");

  navigation_listener_->RunUntilTitleEquals("loaded");
  frame_->SetBlockMediaLoading(true);
  cr_fuchsia::ExecuteJavaScript(frame_.get(), "bear.play()");
  navigation_listener_->RunUntilTitleEquals("playing");
}

TEST_F(WebEngineIntegrationTest, WebGLContextAbsentWithoutVulkanFeature) {
  CreateContextAndFrame(DefaultContextParams());

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      navigation_controller_.get(), fuchsia::web::LoadUrlParams(),
      embedded_test_server_.GetURL("/webgl_presence.html").spec()));

  navigation_listener_->RunUntilLoaded();

  EXPECT_EQ(navigation_listener_->title(), "absent");
}

#if defined(ARCH_CPU_ARM_FAMILY)
// TODO(crbug.com/1058247): Support Vulkan in tests on ARM64.
#define MAYBE_VulkanWebEngineIntegrationTest \
  DISABLED_VulkanWebEngineIntegrationTest
#else
#define MAYBE_VulkanWebEngineIntegrationTest VulkanWebEngineIntegrationTest
#endif
class MAYBE_VulkanWebEngineIntegrationTest : public WebEngineIntegrationTest {};

// TODO(crbug.com/1104563): Flakily times-out.
TEST_F(MAYBE_VulkanWebEngineIntegrationTest,
       DISABLED_WebGLContextPresentWithVulkanFeature) {
  fuchsia::web::CreateContextParams create_params = DefaultContextParams();
  create_params.set_features(fuchsia::web::ContextFeatureFlags::VULKAN);
  CreateContextAndFrame(std::move(create_params));

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      navigation_controller_.get(), fuchsia::web::LoadUrlParams(),
      embedded_test_server_.GetURL("/webgl_presence.html").spec()));

  navigation_listener_->RunUntilLoaded();

  EXPECT_EQ(navigation_listener_->title(), "present");
}

void WebEngineIntegrationTestBase::RunCameraTest(bool grant_permission) {
  fuchsia::web::CreateContextParams create_params =
      ContextParamsWithFilteredServiceDirectory();

  media::FakeCameraDeviceWatcher fake_camera_device_watcher(
      filtered_service_directory_->outgoing_directory());

  CreateContextAndFrame(std::move(create_params));

  if (grant_permission) {
    GrantPermission(fuchsia::web::PermissionType::CAMERA,
                    embedded_test_server_.GetURL("/").GetOrigin().spec());
  }

  const char* url =
      grant_permission ? "/camera.html" : "/camera.html?NoPermission";
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      navigation_controller_.get(), fuchsia::web::LoadUrlParams(),
      embedded_test_server_.GetURL(url).spec()));

  navigation_listener_->RunUntilTitleEquals("ended");
}

// TODO(crbug.com/1104562): Flakily times-out.
TEST_F(WebEngineIntegrationTest, DISABLED_CameraAccess_WithPermission) {
  RunCameraTest(/*grant_permission=*/true);
}

TEST_F(WebEngineIntegrationTest, CameraAccess_WithoutPermission) {
  RunCameraTest(/*grant_permission=*/false);
}

TEST_F(WebEngineIntegrationTestBase, CameraNoVideoCaptureProcess) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII("disable-features", "MojoVideoCapture");
  StartWebEngine(std::move(command_line));
  RunCameraTest(/*grant_permission=*/true);
}

// Check that when the ContextFeatureFlag HARDWARE_VIDEO_DECODER is
// provided that the CodecFactory service is connected to.
TEST_F(MAYBE_VulkanWebEngineIntegrationTest,
       HardwareVideoDecoderFlag_Provided) {
  fuchsia::web::CreateContextParams create_params =
      ContextParamsWithAudioAndTestData();

  // The VULKAN flag is required for hardware video decoders to be available.
  create_params.set_features(
      fuchsia::web::ContextFeatureFlags::VULKAN |
      fuchsia::web::ContextFeatureFlags::HARDWARE_VIDEO_DECODER |
      fuchsia::web::ContextFeatureFlags::AUDIO);
  CreateContextAndFrame(std::move(create_params));

  // Check that the CodecFactory service is requested.
  bool is_requested = false;
  filtered_service_directory_->outgoing_directory()->AddPublicService(
      std::make_unique<vfs::Service>(
          [&is_requested](zx::channel channel, async_dispatcher_t* dispatcher) {
            is_requested = true;
          }),
      fuchsia::mediacodec::CodecFactory::Name_);

  LoadUrlWithUserActivation("fuchsia-dir://testdata/play_video.html?autoplay");
  navigation_listener_->RunUntilTitleEquals("ended");

  EXPECT_TRUE(is_requested);
}

// Check that the CodecFactory service is not requested when
// HARDWARE_VIDEO_DECODER is not provided.
// The video should use software decoders and still play.
TEST_F(WebEngineIntegrationTest, HardwareVideoDecoderFlag_NotProvided) {
  fuchsia::web::CreateContextParams create_params =
      ContextParamsWithAudioAndTestData();
  CreateContextAndFrame(std::move(create_params));

  bool is_requested = false;
  filtered_service_directory_->outgoing_directory()->AddPublicService(
      std::make_unique<vfs::Service>(
          [&is_requested](zx::channel channel, async_dispatcher_t* dispatcher) {
            is_requested = true;
          }),
      fuchsia::mediacodec::CodecFactory::Name_);

  LoadUrlWithUserActivation("fuchsia-dir://testdata/play_video.html?autoplay");

  navigation_listener_->RunUntilTitleEquals("ended");

  EXPECT_FALSE(is_requested);
}
