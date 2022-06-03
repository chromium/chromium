// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/mediacodec/cpp/fidl.h>
#include <fuchsia/mem/cpp/fidl.h>
#include <lib/zx/vmo.h>
#include <zircon/rights.h>
#include <zircon/types.h>

#include <string>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/mem_buffer_util.h"
#include "base/fuchsia/process_context.h"
#include "base/strings/stringprintf.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "components/version_info/version_info.h"
#include "fuchsia/base/test/fit_adapter.h"
#include "fuchsia/base/test/frame_test_util.h"
#include "fuchsia/base/test/test_devtools_list_fetcher.h"
#include "fuchsia_web/webengine/web_engine_integration_test_base.h"
#include "media/base/media_switches.h"
#include "media/fuchsia/audio/fake_audio_consumer.h"
#include "media/fuchsia/audio/fake_audio_device_enumerator.h"
#include "media/fuchsia/camera/fake_fuchsia_camera.h"
#include "net/http/http_request_headers.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

constexpr char kValidUserAgentProduct[] = "TestProduct";
constexpr char kValidUserAgentVersion[] = "dev.12345";
constexpr char kValidUserAgentProductAndVersion[] = "TestProduct/dev.12345";
constexpr char kInvalidUserAgentProduct[] = "Test/Product";
constexpr char kInvalidUserAgentVersion[] = "dev/12345";

constexpr char kAutoplayVp9OpusUrl[] =
    "fuchsia-dir://testdata/play_video.html?codecs=vp9,opus&autoplay=1";
constexpr char kAutoplayVp9OpusToEndUrl[] =
    "fuchsia-dir://testdata/"
    "play_video.html?codecs=vp9,opus&autoplay=1&reportended=1";
constexpr char kLoadVp9OpusUrl[] =
    "fuchsia-dir://testdata/play_video.html?codecs=vp9,opus";

}  // namespace

// Starts a WebEngine instance before running the test.
class WebEngineIntegrationTest : public WebEngineIntegrationTestBase {
 protected:
  WebEngineIntegrationTest() = default;

  void SetUp() override {
    WebEngineIntegrationTestBase::SetUp();

    StartWebEngine(base::CommandLine(base::CommandLine::NO_PROGRAM));
  }

  void RunPermissionTest(bool grant);
};

class WebEngineIntegrationUserAgentTest : public WebEngineIntegrationTest {
 protected:
  GURL GetEchoUserAgentUrl() {
    static std::string echo_user_agent_header_path =
        std::string("/echoheader?") + net::HttpRequestHeaders::kUserAgent;
    return embedded_test_server_.GetURL(echo_user_agent_header_path);
  }

  // Returns the expected user agent string for the current Chrome version.
  static std::string GetExpectedUserAgentString() {
    // The default (base) user agent string without any client modifications.
    // TODO(crbug.com/1225812): Replace "X11; " appropriately and the version
    // with <majorVersion>.0.0.0.
    constexpr char kDefaultUserAgentStringWithVersionPlaceholder[] =
        "Mozilla/5.0 (Fuchsia) AppleWebKit/537.36 (KHTML, like Gecko) "
        "Chrome/%s Safari/537.36";

    std::string expected_ua =
        base::StringPrintf(kDefaultUserAgentStringWithVersionPlaceholder,
                           version_info::GetVersionNumber().c_str());

    // Ensure the field was actually populated.
    EXPECT_GT(expected_ua.length(),
              std::size(kDefaultUserAgentStringWithVersionPlaceholder));
    EXPECT_NE(expected_ua.find(version_info::GetVersionNumber()),
              std::string::npos);

    return expected_ua;
  }

  static std::string GetExpectedUserAgentStringWithProduct(
      const char* product) {
    return base::StringPrintf("%s %s", GetExpectedUserAgentString().c_str(),
                              product);
  }
};

// Although this does not need to be an integration test, all other user agent
// tests are here.
TEST_F(WebEngineIntegrationUserAgentTest, Default) {
  // Create a Context with no values specified.
  fuchsia::web::CreateContextParams create_params = TestContextParams();
  CreateContextAndFrameAndLoadUrl(std::move(create_params),
                                  GetEchoUserAgentUrl());

  // Query & verify that the header echoed into the document body is as
  // expected.
  std::string result =
      ExecuteJavaScriptWithStringResult("document.body.innerText;");
  EXPECT_EQ(result, GetExpectedUserAgentString());

  // Query & verify that the navigator.userAgent is as expected.
  result = ExecuteJavaScriptWithStringResult("navigator.userAgent;");
  EXPECT_EQ(result, GetExpectedUserAgentString());
}

TEST_F(WebEngineIntegrationUserAgentTest, ValidProductOnly) {
  // Create a Context with just an embedder product specified.
  fuchsia::web::CreateContextParams create_params = TestContextParams();
  create_params.set_user_agent_product(kValidUserAgentProduct);
  CreateContextAndFrameAndLoadUrl(std::move(create_params),
                                  GetEchoUserAgentUrl());

  std::string expected =
      GetExpectedUserAgentStringWithProduct(kValidUserAgentProduct);

  // Query & verify that the header echoed into the document body contains
  // the product tag.
  std::string result =
      ExecuteJavaScriptWithStringResult("document.body.innerText;");
  EXPECT_TRUE(result.find(kValidUserAgentProduct) != std::string::npos);
  EXPECT_EQ(result, expected);

  // Query & verify that the navigator.userAgent contains the product tag.
  result = ExecuteJavaScriptWithStringResult("navigator.userAgent;");
  EXPECT_TRUE(result.find(kValidUserAgentProduct) != std::string::npos);
  EXPECT_EQ(result, expected);
}

TEST_F(WebEngineIntegrationUserAgentTest, ValidProductAndVersion) {
  // Create a Context with both product and version specified.
  fuchsia::web::CreateContextParams create_params = TestContextParams();
  create_params.set_user_agent_product(kValidUserAgentProduct);
  create_params.set_user_agent_version(kValidUserAgentVersion);
  CreateContextAndFrameAndLoadUrl(std::move(create_params),
                                  GetEchoUserAgentUrl());

  std::string expected =
      GetExpectedUserAgentStringWithProduct(kValidUserAgentProductAndVersion);

  // Query & verify that the header echoed into the document body contains
  // both product & version.
  std::string result =
      ExecuteJavaScriptWithStringResult("document.body.innerText;");
  EXPECT_TRUE(result.find(kValidUserAgentProductAndVersion) !=
              std::string::npos);
  EXPECT_EQ(result, expected);

  // Query & verify that the navigator.userAgent contains product & version.
  result = ExecuteJavaScriptWithStringResult("navigator.userAgent;");
  EXPECT_TRUE(result.find(kValidUserAgentProductAndVersion) !=
              std::string::npos);
  EXPECT_EQ(result, expected);
}

TEST_F(WebEngineIntegrationUserAgentTest, InvalidProduct) {
  // Try to create a Context with an invalid embedder product tag.
  fuchsia::web::CreateContextParams create_params = TestContextParams();
  create_params.set_user_agent_product(kInvalidUserAgentProduct);
  CreateContextAndExpectError(std::move(create_params), ZX_ERR_INVALID_ARGS);
}

TEST_F(WebEngineIntegrationUserAgentTest, VersionOnly) {
  // Try to create a Context with an embedder version but no product.
  fuchsia::web::CreateContextParams create_params = TestContextParams();
  create_params.set_user_agent_version(kValidUserAgentVersion);
  CreateContextAndExpectError(std::move(create_params), ZX_ERR_INVALID_ARGS);
}

TEST_F(WebEngineIntegrationUserAgentTest, ValidProductAndInvalidVersion) {
  // Try to create a Context with valid product tag, but invalid version.
  fuchsia::web::CreateContextParams create_params = TestContextParams();
  create_params.set_user_agent_product(kValidUserAgentProduct);
  create_params.set_user_agent_version(kInvalidUserAgentVersion);
  CreateContextAndExpectError(std::move(create_params), ZX_ERR_INVALID_ARGS);
}

TEST_F(WebEngineIntegrationTest, CreateFrameWithUnclonableFrameParamsFails) {
  CreateContext(TestContextParams());

  zx_rights_t kReadRightsWithoutDuplicate =
      ZX_RIGHT_TRANSFER | ZX_RIGHT_READ | ZX_RIGHT_MAP | ZX_RIGHT_GET_PROPERTY;

  // Create a buffer and remove the ability clone it by changing its rights to
  // not include ZX_RIGHT_DUPLICATE.
  auto buffer = base::MemBufferFromString("some data", "some name");
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
  fuchsia::web::CreateContextParams create_params = TestContextParams();
  create_params.set_remote_debugging_port(0);

  // Create a Frame with remote debugging enabled.
  fuchsia::web::CreateFrameParams create_frame_params;
  create_frame_params.set_enable_remote_debugging(true);
  CreateContext(std::move(create_params));
  CreateFrameWithParams(std::move(create_frame_params));

  // Expect to receive a notification of the selected DevTools port.
  base::test::TestFuture<fuchsia::web::Context_GetRemoteDebuggingPort_Result>
      port_receiver;
  context_->GetRemoteDebuggingPort(
      cr_fuchsia::CallbackToFitFunction(port_receiver.GetCallback()));
  ASSERT_TRUE(port_receiver.Wait());

  ASSERT_TRUE(port_receiver.Get().is_response());
  uint16_t remote_debugging_port = port_receiver.Get().response().port;
  ASSERT_TRUE(remote_debugging_port != 0);

  // Navigate to a URL.
  GURL url = embedded_test_server_.GetURL("/defaultresponse");
  ASSERT_NO_FATAL_FAILURE(LoadUrlAndExpectResponse(url.spec()));
  navigation_listener()->RunUntilUrlEquals(url);

  base::Value devtools_list =
      cr_fuchsia::GetDevToolsListFromPort(remote_debugging_port);
  ASSERT_TRUE(devtools_list.is_list());
  EXPECT_EQ(devtools_list.GetListDeprecated().size(), 1u);

  base::Value* devtools_url =
      devtools_list.GetListDeprecated()[0].FindPath("url");
  ASSERT_TRUE(devtools_url->is_string());
  EXPECT_EQ(devtools_url->GetString(), url);

  // Create a second frame, without remote debugging enabled. The remote
  // debugging service should still report a single Frame is present.
  fuchsia::web::FramePtr web_frame2;
  web_frame2.set_error_handler([](zx_status_t) { ADD_FAILURE(); });
  context()->CreateFrame(web_frame2.NewRequest());

  devtools_list = cr_fuchsia::GetDevToolsListFromPort(remote_debugging_port);
  ASSERT_TRUE(devtools_list.is_list());
  EXPECT_EQ(devtools_list.GetListDeprecated().size(), 1u);

  devtools_url = devtools_list.GetListDeprecated()[0].FindPath("url");
  ASSERT_TRUE(devtools_url->is_string());
  EXPECT_EQ(devtools_url->GetString(), url);

  // Tear down the debuggable Frame. The remote debugging service should have
  // shut down.
  base::RunLoop controller_run_loop;
  auto navigation_controller = CreateNavigationController();
  navigation_controller.set_error_handler(
      [&controller_run_loop](zx_status_t) { controller_run_loop.Quit(); });
  frame_.Unbind();

  // Wait until the NavigationController shuts down to ensure WebEngine has
  // handled the Frame tear down.
  controller_run_loop.Run();

  devtools_list = cr_fuchsia::GetDevToolsListFromPort(remote_debugging_port);
  EXPECT_TRUE(devtools_list.is_none());
}

// Check that remote debugging requests for Frames in non-debuggable Contexts
// cause an error to be reported.
TEST_F(WebEngineIntegrationTest, RequestDebuggableFrameInNonDebuggableContext) {
  fuchsia::web::CreateFrameParams create_frame_params;
  create_frame_params.set_enable_remote_debugging(true);
  CreateContext(TestContextParams());
  CreateFrameWithParams(std::move(create_frame_params));

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

  CreateContextAndFrame(TestContextParamsWithTestData());

  // Navigate to test1.html and verify that the resource was correctly
  // downloaded and interpreted by inspecting the document title.
  ASSERT_NO_FATAL_FAILURE(LoadUrlAndExpectResponse(kUrl.spec()));
  navigation_listener()->RunUntilUrlAndTitleEquals(kUrl, kTitle);
}

// Configures the default filtered service directory with a fake AudioConsumer
// service for testing.
class WebEngineIntegrationMediaTest : public WebEngineIntegrationTest {
 protected:
  WebEngineIntegrationMediaTest()
      : fake_audio_consumer_service_(filtered_service_directory()
                                         .outgoing_directory()
                                         ->GetOrCreateDirectory("svc")) {
    auto* outgoing_directory =
        filtered_service_directory().outgoing_directory();

    // Publish fake AudioDeviceEnumerator.
    outgoing_directory
        ->RemovePublicService<fuchsia::media::AudioDeviceEnumerator>();
    fake_audio_device_enumerator_.emplace(
        outgoing_directory->GetOrCreateDirectory("svc"));

    // Intercept `fuchsia::media::Audio` connections in order to count them.
    outgoing_directory->RemovePublicService<fuchsia::media::Audio>();
    zx_status_t status = outgoing_directory->AddPublicService(
        fidl::InterfaceRequestHandler<fuchsia::media::Audio>(
            [this](auto request) {
              ++num_audio_connections_;
              base::ComponentContextForProcess()->svc()->Connect(
                  std::move(request));
            }));
    ZX_CHECK(status == ZX_OK, status) << "AddPublicService";
  }

  // Returns a CreateContextParams that has AUDIO feature, and the "testdata"
  // content directory provider configured.
  fuchsia::web::CreateContextParams ContextParamsWithAudioAndTestData() {
    fuchsia::web::CreateContextParams create_params =
        TestContextParamsWithTestData();
    *create_params.mutable_features() |=
        fuchsia::web::ContextFeatureFlags::AUDIO;
    return create_params;
  }

  media::FakeAudioConsumerService fake_audio_consumer_service_;
  absl::optional<media::FakeAudioDeviceEnumerator>
      fake_audio_device_enumerator_;

  size_t num_audio_connections_ = 0;
};

TEST_F(WebEngineIntegrationMediaTest, PlayAudioToAudioRenderer) {
  CreateContextAndFrame(ContextParamsWithAudioAndTestData());

  ASSERT_NO_FATAL_FAILURE(LoadUrlAndExpectResponse(
      "fuchsia-dir://testdata/play_audio.html",
      cr_fuchsia::CreateLoadUrlParamsWithUserActivation()));

  navigation_listener()->RunUntilTitleEquals("ended");

  EXPECT_EQ(num_audio_connections_, 1U);
  EXPECT_EQ(fake_audio_consumer_service_.num_instances(), 0U);
}

TEST_F(WebEngineIntegrationMediaTest, PlayAudioToAudioConsumer) {
  CreateContextAndFrame(ContextParamsWithAudioAndTestData());

  // Send `FrameMediaSettings` with `audio_consumer_session_id`. This enables
  // `AudioConsumer`.
  static const uint16_t kTestMediaSessionId = 43;
  fuchsia::web::FrameMediaSettings media_settings;
  media_settings.set_audio_consumer_session_id(kTestMediaSessionId);
  frame_->SetMediaSettings(std::move(media_settings));

  ASSERT_NO_FATAL_FAILURE(LoadUrlAndExpectResponse(
      "fuchsia-dir://testdata/play_audio.html",
      cr_fuchsia::CreateLoadUrlParamsWithUserActivation()));

  navigation_listener()->RunUntilTitleEquals("ended");

  EXPECT_EQ(num_audio_connections_, 0U);
  ASSERT_EQ(fake_audio_consumer_service_.num_instances(), 1U);

  auto pos = fake_audio_consumer_service_.instance(0)->GetMediaPosition();
  EXPECT_GT(pos, base::Seconds(2.0));
  EXPECT_LT(pos, base::Seconds(2.5));

  EXPECT_EQ(fake_audio_consumer_service_.instance(0)->session_id(),
            kTestMediaSessionId);
  EXPECT_EQ(fake_audio_consumer_service_.instance(0)->volume(), 1.0);
  EXPECT_FALSE(fake_audio_consumer_service_.instance(0)->is_muted());
}

// Check that audio cannot play when the AUDIO ContextFeatureFlag is not
// provided.
TEST_F(WebEngineIntegrationMediaTest, PlayAudio_NoFlag) {
  // Both FilteredServiceDirectory and test data are needed.
  fuchsia::web::CreateContextParams create_params =
      TestContextParamsWithTestData();
  CreateContextAndFrame(std::move(create_params));

  ASSERT_NO_FATAL_FAILURE(LoadUrlAndExpectResponse(
      "fuchsia-dir://testdata/play_audio.html",
      cr_fuchsia::CreateLoadUrlParamsWithUserActivation()));

  // The file is still expected to play to the end.
  navigation_listener()->RunUntilTitleEquals("ended");

  EXPECT_EQ(fake_audio_consumer_service_.num_instances(), 0U);
  EXPECT_EQ(num_audio_connections_, 0U);
}

TEST_F(WebEngineIntegrationMediaTest, PlayVideo) {
  CreateContextAndFrame(ContextParamsWithAudioAndTestData());

  ASSERT_NO_FATAL_FAILURE(LoadUrlAndExpectResponse(
      kAutoplayVp9OpusToEndUrl,
      cr_fuchsia::CreateLoadUrlParamsWithUserActivation()));

  navigation_listener()->RunUntilTitleEquals("ended");

  // Audio should be sent to AudioRenderer (created though `fuchsia.web.Audio`).
  EXPECT_EQ(num_audio_connections_, 1U);
  EXPECT_EQ(fake_audio_consumer_service_.num_instances(), 0U);
}

void WebEngineIntegrationTest::RunPermissionTest(bool grant) {
  CreateContextAndFrame(TestContextParamsWithTestData());

  if (grant) {
    GrantPermission(fuchsia::web::PermissionType::MICROPHONE,
                    "fuchsia-dir://testdata/");
  }

  ASSERT_NO_FATAL_FAILURE(LoadUrlAndExpectResponse(
      "fuchsia-dir://testdata/check_mic_permission.html"));

  navigation_listener()->RunUntilTitleEquals(grant ? "granted" : "denied");
}

TEST_F(WebEngineIntegrationTest, PermissionDenied) {
  RunPermissionTest(false);
}

TEST_F(WebEngineIntegrationTest, PermissionGranted) {
  RunPermissionTest(true);
}

// TODO(crbug.com/1299352): Flaky.
TEST_F(WebEngineIntegrationMediaTest,
       DISABLED_MicrophoneAccess_WithPermission) {
  CreateContextAndFrame(ContextParamsWithAudioAndTestData());

  GrantPermission(
      fuchsia::web::PermissionType::MICROPHONE,
      embedded_test_server_.GetURL("/").DeprecatedGetOriginAsURL().spec());

  ASSERT_NO_FATAL_FAILURE(LoadUrlAndExpectResponse(
      embedded_test_server_.GetURL("/mic.html").spec()));

  navigation_listener()->RunUntilTitleEquals("ended");
}

TEST_F(WebEngineIntegrationMediaTest, MicrophoneAccess_WithoutPermission) {
  CreateContextAndFrame(ContextParamsWithAudioAndTestData());

  ASSERT_NO_FATAL_FAILURE(LoadUrlAndExpectResponse(
      embedded_test_server_.GetURL("/mic.html?NoPermission").spec()));

  navigation_listener()->RunUntilTitleEquals("ended-NotFoundError");
}

TEST_F(WebEngineIntegrationMediaTest, SetBlockMediaLoading_Blocked) {
  CreateContextAndFrame(ContextParamsWithAudioAndTestData());

  frame_->SetBlockMediaLoading(true);

  ASSERT_NO_FATAL_FAILURE(LoadUrlAndExpectResponse(
      kAutoplayVp9OpusUrl,
      cr_fuchsia::CreateLoadUrlParamsWithUserActivation()));

  // Check different indicators that media has not loaded and is not playing.
  navigation_listener()->RunUntilTitleEquals("stalled");
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

  ASSERT_NO_FATAL_FAILURE(LoadUrlAndExpectResponse(
      kAutoplayVp9OpusUrl,
      cr_fuchsia::CreateLoadUrlParamsWithUserActivation()));

  // Check that media loading has been blocked.
  navigation_listener()->RunUntilTitleEquals("stalled");

  // Unblock media from loading and see if media loads and plays, since
  // autoplay=true.
  frame_->SetBlockMediaLoading(false);
  navigation_listener()->RunUntilTitleEquals("playing");
  EXPECT_TRUE(ExecuteJavaScriptWithBoolResult("isMetadataLoaded"));
}

// Check that when autoplay=false and media loading was blocked after the
// element has started loading that media will play when play() is called.
TEST_F(WebEngineIntegrationMediaTest,
       SetBlockMediaLoading_SetBlockedAfterLoading) {
  CreateContextAndFrame(ContextParamsWithAudioAndTestData());

  ASSERT_NO_FATAL_FAILURE(LoadUrlAndExpectResponse(
      kLoadVp9OpusUrl, cr_fuchsia::CreateLoadUrlParamsWithUserActivation()));

  navigation_listener()->RunUntilTitleEquals("loaded");
  frame_->SetBlockMediaLoading(true);
  cr_fuchsia::ExecuteJavaScript(frame_.get(), "bear.play()");
  navigation_listener()->RunUntilTitleEquals("playing");
}

TEST_F(WebEngineIntegrationTest, WebGLContextAbsentWithoutVulkanFeature) {
  CreateContextAndFrame(TestContextParams());

  ASSERT_NO_FATAL_FAILURE(LoadUrlAndExpectResponse(
      embedded_test_server_.GetURL("/webgl_presence.html").spec()));

  navigation_listener()->RunUntilLoaded();

  EXPECT_EQ(navigation_listener()->title(), "absent");
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
  fuchsia::web::CreateContextParams create_params = TestContextParams();
  *create_params.mutable_features() |=
      fuchsia::web::ContextFeatureFlags::VULKAN;
  CreateContextAndFrame(std::move(create_params));

  ASSERT_NO_FATAL_FAILURE(LoadUrlAndExpectResponse(
      embedded_test_server_.GetURL("/webgl_presence.html").spec()));

  navigation_listener()->RunUntilLoaded();

  EXPECT_EQ(navigation_listener()->title(), "present");
}

// Does not start WebEngine automatically as one of the tests requires manually
// starting it.
class WebEngineIntegrationCameraTest : public WebEngineIntegrationTestBase {
 protected:
  WebEngineIntegrationCameraTest() = default;

  void RunCameraTest(bool grant_permission);
};

void WebEngineIntegrationCameraTest::RunCameraTest(bool grant_permission) {
  fuchsia::web::CreateContextParams create_params = TestContextParams();

  media::FakeCameraDeviceWatcher fake_camera_device_watcher(
      filtered_service_directory().outgoing_directory());

  CreateContextAndFrame(std::move(create_params));

  if (grant_permission) {
    GrantPermission(
        fuchsia::web::PermissionType::CAMERA,
        embedded_test_server_.GetURL("/").DeprecatedGetOriginAsURL().spec());
  }

  const char* url =
      grant_permission ? "/camera.html" : "/camera.html?NoPermission";
  ASSERT_NO_FATAL_FAILURE(
      LoadUrlAndExpectResponse(embedded_test_server_.GetURL(url).spec()));

  navigation_listener()->RunUntilTitleEquals("ended");
}

TEST_F(WebEngineIntegrationCameraTest, CameraAccess_WithPermission) {
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
  // Check that the CodecFactory service is requested.
  base::RunLoop codec_connected_run_loop;
  auto* outgoing_directory = filtered_service_directory().outgoing_directory();
  outgoing_directory->RemovePublicService<fuchsia::mediacodec::CodecFactory>();
  zx_status_t status = outgoing_directory->AddPublicService(
      fidl::InterfaceRequestHandler<fuchsia::mediacodec::CodecFactory>(
          [&codec_connected_run_loop](auto request) {
            codec_connected_run_loop.Quit();
          }));
  ZX_CHECK(status == ZX_OK, status) << "AddPublicService";

  // The VULKAN flag is required for hardware video decoders to be available.
  fuchsia::web::CreateContextParams create_params =
      ContextParamsWithAudioAndTestData();
  *create_params.mutable_features() |=
      fuchsia::web::ContextFeatureFlags::VULKAN |
      fuchsia::web::ContextFeatureFlags::HARDWARE_VIDEO_DECODER;
  CreateContextAndFrame(std::move(create_params));

  ASSERT_NO_FATAL_FAILURE(LoadUrlAndExpectResponse(
      kAutoplayVp9OpusToEndUrl,
      cr_fuchsia::CreateLoadUrlParamsWithUserActivation()));
  codec_connected_run_loop.Run();
}

// Check that the CodecFactory service is not requested when
// HARDWARE_VIDEO_DECODER is not provided.
// The video should use software decoders and still play.
TEST_F(WebEngineIntegrationMediaTest, HardwareVideoDecoderFlag_NotProvided) {
  bool is_requested = false;
  auto* outgoing_directory = filtered_service_directory().outgoing_directory();
  outgoing_directory->RemovePublicService<fuchsia::mediacodec::CodecFactory>();
  zx_status_t status = outgoing_directory->AddPublicService(
      fidl::InterfaceRequestHandler<fuchsia::mediacodec::CodecFactory>(
          [&is_requested](auto request) { is_requested = true; }));
  ZX_CHECK(status == ZX_OK, status) << "AddPublicService";

  fuchsia::web::CreateContextParams create_params =
      ContextParamsWithAudioAndTestData();
  CreateContextAndFrame(std::move(create_params));

  ASSERT_NO_FATAL_FAILURE(LoadUrlAndExpectResponse(
      kAutoplayVp9OpusToEndUrl,
      cr_fuchsia::CreateLoadUrlParamsWithUserActivation()));

  navigation_listener()->RunUntilTitleEquals("ended");

  EXPECT_FALSE(is_requested);
}
