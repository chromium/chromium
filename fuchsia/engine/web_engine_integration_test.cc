// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/mem/cpp/fidl.h>
#include <zircon/rights.h>
#include <zircon/types.h>

#include <lib/zx/vmo.h>

#include "fuchsia/base/fit_adapter.h"
#include "fuchsia/base/frame_test_util.h"
#include "fuchsia/base/mem_buffer_util.h"
#include "fuchsia/base/result_receiver.h"
#include "fuchsia/base/test_devtools_list_fetcher.h"
#include "fuchsia/engine/web_engine_integration_test_base.h"
#include "media/base/media_switches.h"
#include "media/fuchsia/audio/fake_audio_consumer.h"
#include "media/fuchsia/camera/fake_fuchsia_camera.h"
#include "net/base/test_completion_callback.h"
#include "net/http/http_request_headers.h"
#include "net/socket/tcp_client_socket.h"

namespace {

constexpr char kValidUserAgentProduct[] = "TestProduct";
constexpr char kValidUserAgentVersion[] = "dev.12345";
constexpr char kValidUserAgentProductAndVersion[] = "TestProduct/dev.12345";
constexpr char kInvalidUserAgentProduct[] = "Test/Product";
constexpr char kInvalidUserAgentVersion[] = "dev/12345";

}  // namespace

// Starts a WebEngine instance before running the test.
class WebEngineIntegrationTest : public WebEngineIntegrationTestBase {
 protected:
  WebEngineIntegrationTest() : WebEngineIntegrationTestBase() {}

  void SetUp() override {
    WebEngineIntegrationTestBase::SetUp();

    StartWebEngine(base::CommandLine(base::CommandLine::NO_PROGRAM));
  }

  void RunPermissionTest(bool grant);
};

class WebEngineIntegrationMediaTest : public WebEngineIntegrationTest {
 protected:
  WebEngineIntegrationMediaTest() : WebEngineIntegrationTest() {}

  // Returns a CreateContextParams that has AUDIO feature enabled with an
  // injected FakeAudioConsumerService.
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

  std::unique_ptr<media::FakeAudioConsumerService> fake_audio_consumer_service_;
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

TEST_F(WebEngineIntegrationTest, CreateFrameWithUnclonableFrameParamsFails) {
  CreateContext(DefaultContextParams());

  zx_rights_t kReadRightsWithoutDuplicate =
      ZX_RIGHT_TRANSFER | ZX_RIGHT_READ | ZX_RIGHT_MAP | ZX_RIGHT_GET_PROPERTY;

  // Create a buffer and remove the ability clone it by changing its rights to
  // not include ZX_RIGHT_DUPLICATE.
  auto buffer = cr_fuchsia::MemBufferFromString("some data", "some name");
  zx::vmo unclonable_readonly_vmo;
  EXPECT_EQ(ZX_OK, buffer.vmo.duplicate(kReadRightsWithoutDuplicate,
                                        &unclonable_readonly_vmo));
  buffer.vmo = std::move(unclonable_readonly_vmo);

  // Creation will fail, which will be reported in the error handler below.
  fuchsia::web::CreateFrameParams create_frame_params;
  create_frame_params.set_explicit_sites_filter_error_page(
      fuchsia::mem::Data::WithBuffer(std::move(buffer)));
  context_->CreateFrameWithParams(std::move(create_frame_params),
                                  frame_.NewRequest());

  base::RunLoop loop;
  frame_.set_error_handler(
      [quit_loop = loop.QuitClosure()](zx_status_t status) {
        EXPECT_EQ(status, ZX_ERR_INVALID_ARGS);
        quit_loop.Run();
      });
  loop.Run();

  EXPECT_FALSE(frame_);
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

TEST_F(WebEngineIntegrationMediaTest, PlayAudio) {
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
TEST_F(WebEngineIntegrationMediaTest, PlayAudio_NoFlag) {
  // Both FilteredServiceDirectory and test data are needed.
  fuchsia::web::CreateContextParams create_params =
      ContextParamsWithFilteredServiceDirectory();
  create_params.mutable_content_directories()->push_back(
      CreateTestDataDirectoryProvider());
  CreateContextAndFrame(std::move(create_params));

  bool is_requested = false;
  ASSERT_EQ(filtered_service_directory_->outgoing_directory()->AddPublicService(
                std::make_unique<vfs::Service>(
                    [&is_requested](zx::channel channel,
                                    async_dispatcher_t* dispatcher) {
                      is_requested = true;
                    }),
                fuchsia::media::SessionAudioConsumerFactory::Name_),
            ZX_OK);

  LoadUrlWithUserActivation("fuchsia-dir://testdata/play_audio.html");

  navigation_listener_->RunUntilTitleEquals("error");
  EXPECT_FALSE(is_requested);
}

TEST_F(WebEngineIntegrationMediaTest, PlayVideo) {
  CreateContextAndFrame(ContextParamsWithAudioAndTestData());

  LoadUrlWithUserActivation("fuchsia-dir://testdata/play_video.html?autoplay");

  navigation_listener_->RunUntilTitleEquals("ended");
}

void WebEngineIntegrationTest::RunPermissionTest(bool grant) {
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

TEST_F(WebEngineIntegrationMediaTest, MicrophoneAccess_WithPermission) {
  CreateContextAndFrame(ContextParamsWithAudio());

  GrantPermission(fuchsia::web::PermissionType::MICROPHONE,
                  embedded_test_server_.GetURL("/").GetOrigin().spec());

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      navigation_controller_.get(), fuchsia::web::LoadUrlParams(),
      embedded_test_server_.GetURL("/mic.html").spec()));

  navigation_listener_->RunUntilTitleEquals("ended");
}

TEST_F(WebEngineIntegrationMediaTest, MicrophoneAccess_WithoutPermission) {
  CreateContextAndFrame(ContextParamsWithAudio());

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      navigation_controller_.get(), fuchsia::web::LoadUrlParams(),
      embedded_test_server_.GetURL("/mic.html?NoPermission").spec()));

  navigation_listener_->RunUntilTitleEquals("ended");
}

TEST_F(WebEngineIntegrationMediaTest, SetBlockMediaLoading_Blocked) {
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
TEST_F(WebEngineIntegrationMediaTest, SetBlockMediaLoading_AfterUnblock) {
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
TEST_F(WebEngineIntegrationMediaTest,
       SetBlockMediaLoading_SetBlockedAfterLoading) {
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
class MAYBE_VulkanWebEngineIntegrationTest
    : public WebEngineIntegrationMediaTest {};

TEST_F(MAYBE_VulkanWebEngineIntegrationTest,
       WebGLContextPresentWithVulkanFeature) {
  fuchsia::web::CreateContextParams create_params = DefaultContextParams();
  create_params.set_features(fuchsia::web::ContextFeatureFlags::VULKAN);
  CreateContextAndFrame(std::move(create_params));

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      navigation_controller_.get(), fuchsia::web::LoadUrlParams(),
      embedded_test_server_.GetURL("/webgl_presence.html").spec()));

  navigation_listener_->RunUntilLoaded();

  EXPECT_EQ(navigation_listener_->title(), "present");
}

// Does not start WebEngine automatically as one of the tests requires manually
// starting it.
class WebEngineIntegrationCameraTest : public WebEngineIntegrationTestBase {
 protected:
  WebEngineIntegrationCameraTest() : WebEngineIntegrationTestBase() {}

  void RunCameraTest(bool grant_permission);
};

void WebEngineIntegrationCameraTest::RunCameraTest(bool grant_permission) {
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
TEST_F(WebEngineIntegrationCameraTest, DISABLED_CameraAccess_WithPermission) {
  StartWebEngine(base::CommandLine(base::CommandLine::NO_PROGRAM));
  RunCameraTest(/*grant_permission=*/true);
}

TEST_F(WebEngineIntegrationCameraTest, CameraAccess_WithoutPermission) {
  StartWebEngine(base::CommandLine(base::CommandLine::NO_PROGRAM));
  RunCameraTest(/*grant_permission=*/false);
}

TEST_F(WebEngineIntegrationCameraTest, CameraNoVideoCaptureProcess) {
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
  ASSERT_EQ(filtered_service_directory_->outgoing_directory()->AddPublicService(
                std::make_unique<vfs::Service>(
                    [&is_requested](zx::channel channel,
                                    async_dispatcher_t* dispatcher) {
                      is_requested = true;
                    }),
                fuchsia::mediacodec::CodecFactory::Name_),
            ZX_OK);

  LoadUrlWithUserActivation("fuchsia-dir://testdata/play_video.html?autoplay");
  navigation_listener_->RunUntilTitleEquals("ended");

  EXPECT_TRUE(is_requested);
}

// Check that the CodecFactory service is not requested when
// HARDWARE_VIDEO_DECODER is not provided.
// The video should use software decoders and still play.
TEST_F(WebEngineIntegrationMediaTest, HardwareVideoDecoderFlag_NotProvided) {
  fuchsia::web::CreateContextParams create_params =
      ContextParamsWithAudioAndTestData();
  CreateContextAndFrame(std::move(create_params));

  bool is_requested = false;
  ASSERT_EQ(filtered_service_directory_->outgoing_directory()->AddPublicService(
                std::make_unique<vfs::Service>(
                    [&is_requested](zx::channel channel,
                                    async_dispatcher_t* dispatcher) {
                      is_requested = true;
                    }),
                fuchsia::mediacodec::CodecFactory::Name_),
            ZX_OK);

  LoadUrlWithUserActivation("fuchsia-dir://testdata/play_video.html?autoplay");

  navigation_listener_->RunUntilTitleEquals("ended");

  EXPECT_FALSE(is_requested);
}
