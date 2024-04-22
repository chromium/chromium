// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_internals.h"

#include <stddef.h>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/test/scoped_feature_list.h"
#include "base/types/cxx23_to_underlying.h"
#include "build/build_config.h"
#include "content/browser/media/session/media_session_impl.h"
#include "content/public/browser/media_session_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/test_web_contents.h"
#include "media/base/audio_parameters.h"
#include "media/base/channel_layout.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/media_session/public/cpp/features.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "ui/gfx/geometry/size.h"

namespace {
const int kTestComponentID = 0;
const char kTestDeviceID[] = "test-device-id";

using media_session::mojom::AudioFocusRequestStatePtr;

// This class encapsulates a MediaInternals reference. It also has some useful
// methods to receive a callback, deserialize its associated data and expect
// integer/string values.
class MediaInternalsTestBase {
 public:
  MediaInternalsTestBase() {
    scoped_feature_list_.InitAndEnableFeature(
        media_session::features::kMediaSessionService);
  }
  virtual ~MediaInternalsTestBase() = default;

 protected:
  // Extracts and deserializes the JSON update data; merges into |update_data_|.
  virtual void UpdateCallbackImpl(const std::u16string& update) {
    // Each update string looks like "<JavaScript Function Name>({<JSON>});"
    // or for video capabilities: "<JavaScript Function Name>([{<JSON>}]);".
    // In the second case we will be able to extract the dictionary if it is the
    // only member of the list.
    // To use the JSON reader we need to strip out the JS function name and ().
    std::string utf8_update = base::UTF16ToUTF8(update);
    const std::string::size_type first_brace = utf8_update.find('{');
    const std::string::size_type last_brace = utf8_update.rfind('}');
    std::optional<base::Value> output_value = base::JSONReader::Read(
        utf8_update.substr(first_brace, last_brace - first_brace + 1));
    ASSERT_TRUE(output_value);
    ASSERT_TRUE(output_value->is_dict());

    update_data_.Merge(std::move(*output_value).TakeDict());
  }

  void ExpectInt(const std::string& key, int expected_value) const {
    std::optional<int> actual_value = update_data_.FindInt(key);
    ASSERT_TRUE(actual_value);
    EXPECT_EQ(expected_value, *actual_value);
  }

  void ExpectString(const std::string& key,
                    const std::string& expected_value) const {
    const std::string* actual_value = update_data_.FindString(key);
    ASSERT_TRUE(actual_value);
    EXPECT_EQ(expected_value, *actual_value);
  }

  void ExpectStatus(const std::string& expected_value) const {
    ExpectString("status", expected_value);
  }

  void ExpectListOfStrings(const std::string& key,
                           const base::Value::List& expected_list) const {
    const base::Value::List* actual_list = update_data_.FindList(key);
    ASSERT_TRUE(actual_list);
    const size_t expected_size = expected_list.size();
    const size_t actual_size = actual_list->size();
    ASSERT_EQ(expected_size, actual_size);
    for (size_t i = 0; i < expected_size; ++i) {
      const std::string* expected_value = expected_list[i].GetIfString();
      const std::string* actual_value = (*actual_list)[i].GetIfString();
      ASSERT_TRUE(expected_value);
      ASSERT_TRUE(actual_value);
      EXPECT_EQ(*expected_value, *actual_value);
    }
  }

  base::Value::Dict update_data_;

  content::MediaInternals* media_internals() const {
    return content::MediaInternals::GetInstance();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace

namespace content {

using media_session::mojom::AudioFocusType;

class MediaInternalsVideoCaptureDeviceTest : public testing::Test,
                                             public MediaInternalsTestBase {
 public:
  MediaInternalsVideoCaptureDeviceTest()
      : update_cb_(base::BindRepeating(
            &MediaInternalsVideoCaptureDeviceTest::UpdateCallbackImpl,
            base::Unretained(this))) {
    media_internals()->AddUpdateCallback(update_cb_);
  }

  ~MediaInternalsVideoCaptureDeviceTest() override {
    media_internals()->RemoveUpdateCallback(update_cb_);
  }

 protected:
  const content::BrowserTaskEnvironment task_environment_;
  MediaInternals::UpdateCallback update_cb_;
};

TEST_F(MediaInternalsVideoCaptureDeviceTest,
       VideoCaptureFormatStringIsInExpectedFormat) {
  // Since media internals will send video capture capabilities to JavaScript in
  // an expected format and there are no public methods for accessing the
  // resolutions, frame rates or pixel formats, this test checks that the format
  // has not changed. If the test fails because of the changed format, it should
  // be updated at the same time as the media internals JS files.
  const float kFrameRate = 30.0f;
  const gfx::Size kFrameSize(1280, 720);
  const media::VideoPixelFormat kPixelFormat = media::PIXEL_FORMAT_I420;
  const media::VideoCaptureFormat capture_format(kFrameSize, kFrameRate,
                                                 kPixelFormat);
  const std::string expected_string = base::StringPrintf(
      "(%s)@%.3ffps, pixel format: %s", kFrameSize.ToString().c_str(),
      kFrameRate, media::VideoPixelFormatToString(kPixelFormat).c_str());
  EXPECT_EQ(expected_string,
            media::VideoCaptureFormat::ToString(capture_format));
}

TEST_F(MediaInternalsVideoCaptureDeviceTest,
       NotifyVideoCaptureDeviceCapabilitiesEnumerated) {
  const int kWidth = 1280;
  const int kHeight = 720;
  const float kFrameRate = 30.0f;
  const media::VideoPixelFormat kPixelFormat = media::PIXEL_FORMAT_I420;
  const media::VideoCaptureFormat format_hd({kWidth, kHeight}, kFrameRate,
                                            kPixelFormat);
  media::VideoCaptureFormats formats{};
  formats.push_back(format_hd);
  media::VideoCaptureDeviceDescriptor descriptor;
  descriptor.device_id = "dummy";
  descriptor.set_display_name("dummy");
#if BUILDFLAG(IS_MAC)
  descriptor.capture_api = media::VideoCaptureApi::MACOSX_AVFOUNDATION;
#elif BUILDFLAG(IS_WIN)
  descriptor.capture_api = media::VideoCaptureApi::WIN_DIRECT_SHOW;
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  descriptor.device_id = "/dev/dummy";
  descriptor.capture_api = media::VideoCaptureApi::LINUX_V4L2_SINGLE_PLANE;
#elif BUILDFLAG(IS_ANDROID)
  descriptor.capture_api = media::VideoCaptureApi::ANDROID_API2_LEGACY;
#endif
  std::vector<std::tuple<media::VideoCaptureDeviceDescriptor,
                         media::VideoCaptureFormats>>
      descriptors_and_formats{};
  descriptors_and_formats.push_back(std::make_tuple(descriptor, formats));

  // When updating video capture capabilities, the update will serialize
  // a JSON array of objects to string. So here, the |UpdateCallbackImpl| will
  // deserialize the first object in the array. This means we have to have
  // exactly one device_info in the |descriptors_and_formats|.
  media_internals()->UpdateVideoCaptureDeviceCapabilities(
      descriptors_and_formats);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  ExpectString("id", "/dev/dummy");
#else
  ExpectString("id", "dummy");
#endif
  ExpectString("name", "dummy");
  base::Value::List expected_list;
  expected_list.Append(media::VideoCaptureFormat::ToString(format_hd));
  ExpectListOfStrings("formats", expected_list);
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  ExpectString("captureApi", "V4L2 SPLANE");
#elif BUILDFLAG(IS_WIN)
  ExpectString("captureApi", "Direct Show");
#elif BUILDFLAG(IS_MAC)
  ExpectString("captureApi", "AV Foundation");
#elif BUILDFLAG(IS_ANDROID)
  ExpectString("captureApi", "Camera API2 Legacy");
#endif
}

class MediaInternalsAudioLogTest
    : public MediaInternalsTestBase,
      public testing::TestWithParam<media::AudioLogFactory::AudioComponent> {
 public:
  MediaInternalsAudioLogTest()
      : update_cb_(
            base::BindRepeating(&MediaInternalsAudioLogTest::UpdateCallbackImpl,
                                base::Unretained(this))),
        test_params_(MakeAudioParams()),
        test_component_(GetParam()),
        audio_log_(media_internals()->CreateAudioLog(test_component_,
                                                     kTestComponentID)) {
    media_internals()->AddUpdateCallback(update_cb_);
  }

  virtual ~MediaInternalsAudioLogTest() {
    media_internals()->RemoveUpdateCallback(update_cb_);
  }

 protected:
  const content::BrowserTaskEnvironment task_environment_;
  MediaInternals::UpdateCallback update_cb_;
  const media::AudioParameters test_params_;
  const media::AudioLogFactory::AudioComponent test_component_;
  std::unique_ptr<media::AudioLog> audio_log_;

 private:
  static media::AudioParameters MakeAudioParams() {
    media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LINEAR,
                                  media::ChannelLayoutConfig::Mono(), 48000,
                                  128);
    params.set_effects(media::AudioParameters::ECHO_CANCELLER |
                       media::AudioParameters::DUCKING);
    return params;
  }
};

TEST_P(MediaInternalsAudioLogTest, AudioLogCreateStartStopErrorClose) {
  audio_log_->OnCreated(test_params_, kTestDeviceID);
  base::RunLoop().RunUntilIdle();

  ExpectString("channel_layout",
               media::ChannelLayoutToString(test_params_.channel_layout()));
  ExpectInt("sample_rate", test_params_.sample_rate());
  ExpectInt("frames_per_buffer", test_params_.frames_per_buffer());
  ExpectInt("channels", test_params_.channels());
  ExpectString("effects", "ECHO_CANCELLER | DUCKING");
  ExpectString("device_id", kTestDeviceID);
  ExpectInt("component_id", kTestComponentID);
  ExpectInt("component_type", base::to_underlying(test_component_));
  ExpectStatus("created");

  // Verify OnStarted().
  audio_log_->OnStarted();
  base::RunLoop().RunUntilIdle();
  ExpectStatus("started");

  // Verify OnStopped().
  audio_log_->OnStopped();
  base::RunLoop().RunUntilIdle();
  ExpectStatus("stopped");

  // Verify OnError().
  const char kErrorKey[] = "error_occurred";
  ASSERT_FALSE(update_data_.FindString(kErrorKey));
  audio_log_->OnError();
  base::RunLoop().RunUntilIdle();
  ExpectString(kErrorKey, "true");

  // Verify OnClosed().
  audio_log_->OnClosed();
  base::RunLoop().RunUntilIdle();
  ExpectStatus("closed");
}

TEST_P(MediaInternalsAudioLogTest, AudioLogCreateClose) {
  audio_log_->OnCreated(test_params_, kTestDeviceID);
  base::RunLoop().RunUntilIdle();
  ExpectStatus("created");

  audio_log_->OnClosed();
  base::RunLoop().RunUntilIdle();
  ExpectStatus("closed");
}

INSTANTIATE_TEST_SUITE_P(
    MediaInternalsAudioLogTest,
    MediaInternalsAudioLogTest,
    testing::Values(
        media::AudioLogFactory::AudioComponent::kAudioInputController,
        media::AudioLogFactory::AudioComponent::kAudioOuputController,
        media::AudioLogFactory::AudioComponent::kAudioOutputStream));

// TODO(crbug.com/40589017): AudioFocusManager is not available on
// Android.
#if !BUILDFLAG(IS_ANDROID)

namespace {

// Test page titles.
const char16_t kTestTitle1[] = u"Test Title 1";
const char16_t kTestTitle2[] = u"Test Title 2";

}  // namespace

class MediaInternalsAudioFocusTest : public RenderViewHostTestHarness,
                                     public MediaInternalsTestBase {
 public:
  MediaInternalsAudioFocusTest() = default;
  ~MediaInternalsAudioFocusTest() override = default;

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    update_cb_ =
        base::BindRepeating(&MediaInternalsAudioFocusTest::UpdateCallbackImpl,
                            base::Unretained(this));
    run_loop_ = std::make_unique<base::RunLoop>();

    GetMediaSessionService().BindAudioFocusManager(
        audio_focus_.BindNewPipeAndPassReceiver());

    content::MediaInternals::GetInstance()->AddUpdateCallback(update_cb_);
  }

  void TearDown() override {
    content::MediaInternals::GetInstance()->RemoveUpdateCallback(update_cb_);
    RenderViewHostTestHarness::TearDown();
  }

 protected:
  void UpdateCallbackImpl(const std::u16string& update) override {
    base::AutoLock auto_lock(lock_);
    MediaInternalsTestBase::UpdateCallbackImpl(update);
    call_count_++;

    if (call_count_ == wanted_call_count_)
      run_loop_->Quit();
  }

  base::Value GetSessionsFromValueAndReset() {
    base::AutoLock auto_lock(lock_);

    const base::Value::List* session_list = update_data_.FindList("sessions");
    EXPECT_TRUE(session_list);
    base::Value session(session_list->Clone());

    update_data_.clear();
    run_loop_ = std::make_unique<base::RunLoop>();
    call_count_ = 0;

    return session;
  }

  void Reset() {
    base::AutoLock auto_lock(lock_);

    update_data_.clear();
    run_loop_ = std::make_unique<base::RunLoop>();
    call_count_ = 0;
  }

  void RemoveAllPlayersForTest(MediaSessionImpl* session) {
    session->RemoveAllPlayersForTest();
  }

  void WaitForCallbackCount(int count) {
    wanted_call_count_ = count;

    {
      base::AutoLock auto_lock(lock_);
      if (!update_data_.empty() && call_count_ == wanted_call_count_)
        return;
    }

    run_loop_->Run();
  }

  std::string GetRequestIdForTopFocusRequest() {
    std::string result;

    audio_focus_->GetFocusRequests(base::BindOnce(
        [](std::string* out, std::vector<AudioFocusRequestStatePtr> requests) {
          DCHECK(!requests.empty());
          *out = requests.back()->request_id.value().ToString();
        },
        &result));

    audio_focus_.FlushForTesting();
    return result;
  }

  MediaInternals::UpdateCallback update_cb_;

 private:
  int call_count_ = 0;
  int wanted_call_count_ = 0;

  base::Lock lock_;
  std::unique_ptr<base::RunLoop> run_loop_;

  mojo::Remote<media_session::mojom::AudioFocusManager> audio_focus_;
};

TEST_F(MediaInternalsAudioFocusTest, AudioFocusStateIsUpdated) {
  // Create a test media session and request audio focus.
  std::unique_ptr<WebContents> web_contents1 = CreateTestWebContents();
  static_cast<TestWebContents*>(web_contents1.get())->SetTitle(kTestTitle1);
  MediaSessionImpl* media_session1 = MediaSessionImpl::Get(web_contents1.get());
  media_session1->RequestSystemAudioFocus(AudioFocusType::kGain);
  WaitForCallbackCount(1);

  // Get the |request_id| for the top session.
  const std::string request_id1 = GetRequestIdForTopFocusRequest();

  // Check JSON is what we expect.
  {
    base::Value found_sessions = GetSessionsFromValueAndReset();
    EXPECT_EQ(1u, found_sessions.GetList().size());

    const base::Value::Dict& session = found_sessions.GetList()[0].GetDict();
    EXPECT_EQ(request_id1, *session.FindString("id"));
    EXPECT_NE(session.FindString("name"), nullptr);
    EXPECT_NE(session.FindString("owner"), nullptr);
    EXPECT_NE(session.FindString("state"), nullptr);
  }

  // Create another media session.
  std::unique_ptr<WebContents> web_contents2 = CreateTestWebContents();
  static_cast<TestWebContents*>(web_contents2.get())->SetTitle(kTestTitle2);
  MediaSessionImpl* media_session2 = MediaSessionImpl::Get(web_contents2.get());
  media_session2->RequestSystemAudioFocus(
      AudioFocusType::kGainTransientMayDuck);
  WaitForCallbackCount(2);

  // Get the |request_id| for the top session.
  std::string request_id2 = GetRequestIdForTopFocusRequest();
  DCHECK_NE(request_id1, request_id2);

  // Check JSON is what we expect.
  {
    base::Value found_sessions = GetSessionsFromValueAndReset();
    EXPECT_EQ(2u, found_sessions.GetList().size());

    const base::Value::Dict& session1 = found_sessions.GetList()[0].GetDict();
    EXPECT_EQ(request_id2, *session1.FindString("id"));
    EXPECT_NE(session1.FindString("name"), nullptr);
    EXPECT_NE(session1.FindString("owner"), nullptr);
    EXPECT_NE(session1.FindString("state"), nullptr);

    const base::Value::Dict& session2 = found_sessions.GetList()[1].GetDict();
    EXPECT_EQ(request_id1, *session2.FindString("id"));
    EXPECT_NE(session2.FindString("name"), nullptr);
    EXPECT_NE(session2.FindString("owner"), nullptr);
    EXPECT_NE(session2.FindString("state"), nullptr);
  }

  // Abandon audio focus.
  RemoveAllPlayersForTest(media_session2);
  WaitForCallbackCount(1);

  // Check JSON is what we expect.
  {
    base::Value found_sessions = GetSessionsFromValueAndReset();
    EXPECT_EQ(1u, found_sessions.GetList().size());

    const base::Value::Dict& session = found_sessions.GetList()[0].GetDict();
    EXPECT_EQ(request_id1, *session.FindString("id"));
    EXPECT_NE(session.FindString("name"), nullptr);
    EXPECT_NE(session.FindString("owner"), nullptr);
    EXPECT_NE(session.FindString("state"), nullptr);
  }

  // Abandon audio focus.
  RemoveAllPlayersForTest(media_session1);

  // TODO(crbug.com/40606983): This should wait on a more precise
  // condition than RunLoop idling, but it's not clear exactly what that should
  // be.
  base::RunLoop().RunUntilIdle();

  // Check JSON is what we expect.
  {
    base::Value found_sessions = GetSessionsFromValueAndReset();
    EXPECT_EQ(0u, found_sessions.GetList().size());
  }
}

#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace content
