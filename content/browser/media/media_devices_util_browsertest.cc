// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/bind_post_task.h"
#include "content/browser/media/media_devices_util.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/renderer_host/media/media_devices_manager.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/media_stream_ui_proxy.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/media_device_id.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/shell/browser/shell.h"
#include "media/audio/audio_device_description.h"
#include "media/base/media_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/mediastream/media_devices.mojom-shared.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-shared.h"
#include "url/origin.h"

namespace content {

using ::blink::mojom::MediaDeviceType;
using ::blink::mojom::MediaStreamType;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Optional;

namespace {

// Returns true if the `device_id` corresponds to the system default or
// communications device, false otherwise.
bool IsSpecialAudioDeviceId(MediaDeviceType device_type,
                            const std::string& device_id) {
  return (device_type == MediaDeviceType::kMediaAudioInput ||
          device_type == MediaDeviceType::kMediaAudioOutput) &&
         (media::AudioDeviceDescription::IsDefaultDevice(device_id) ||
          media::AudioDeviceDescription::IsCommunicationsDevice(device_id));
}

std::optional<MediaStreamType> ToMediaStreamType(MediaDeviceType device_type) {
  switch (device_type) {
    case MediaDeviceType::kMediaAudioInput:
      return MediaStreamType::DEVICE_AUDIO_CAPTURE;
    case MediaDeviceType::kMediaVideoInput:
      return MediaStreamType::DEVICE_VIDEO_CAPTURE;
    default:
      return std::nullopt;
  }
}

void VerifyHMACDeviceID(MediaDeviceType device_type,
                        const std::string& hmac_device_id,
                        const std::string& raw_device_id) {
  EXPECT_TRUE(IsValidDeviceId(hmac_device_id));
  if (IsSpecialAudioDeviceId(device_type, hmac_device_id)) {
    EXPECT_EQ(raw_device_id, hmac_device_id);
  } else {
    EXPECT_NE(raw_device_id, hmac_device_id);
  }
}

blink::StreamControls GetAudioStreamControls(std::string hmac_device_id) {
  blink::StreamControls stream_controls{/*request_audio=*/true,
                                        /*request_video=*/false};
  stream_controls.audio.device_ids = {hmac_device_id};
  return stream_controls;
}

blink::mojom::StreamSelectionInfoPtr NewSearchBySessionId(
    base::flat_map<std::string, base::UnguessableToken> session_id_map) {
  return blink::mojom::StreamSelectionInfo::NewSearchBySessionId(
      blink::mojom::SearchBySessionId::New(session_id_map));
}

}  // namespace

class MediaDevicesUtilBrowserTest : public ContentBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kUseFakeDeviceForMediaStream);
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    frame_id_ = shell()->web_contents()->GetPrimaryMainFrame()->GetGlobalId();
    device_enumeration_ = EnumerateDevices();
    ASSERT_EQ(device_enumeration_.size(),
              static_cast<size_t>(MediaDeviceType::kNumMediaDeviceTypes));
    for (const auto& typed_enumeration : device_enumeration_) {
      ASSERT_FALSE(typed_enumeration.empty());
    }
    origin_ = shell()
                  ->web_contents()
                  ->GetPrimaryMainFrame()
                  ->GetLastCommittedOrigin();
  }

  MediaDeviceEnumeration EnumerateDevices() const {
    MediaStreamManager* media_stream_manager =
        BrowserMainLoop::GetInstance()->media_stream_manager();
    MediaDeviceEnumeration device_enumeration;
    base::RunLoop run_loop;
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          MediaDevicesManager::BoolDeviceTypes types;
          types[static_cast<size_t>(MediaDeviceType::kMediaAudioInput)] = true;
          types[static_cast<size_t>(MediaDeviceType::kMediaAudioOutput)] = true;
          types[static_cast<size_t>(MediaDeviceType::kMediaVideoInput)] = true;
          base::test::TestFuture<const MediaDeviceEnumeration&> future;
          media_stream_manager->media_devices_manager()->EnumerateDevices(
              types, base::BindLambdaForTesting(
                         [&](const MediaDeviceEnumeration& enumeration) {
                           device_enumeration = enumeration;
                           run_loop.Quit();
                         }));
        }));
    run_loop.Run();
    return device_enumeration;
  }

  MediaDeviceSaltAndOrigin GetSaltAndOrigin() {
    base::test::TestFuture<const MediaDeviceSaltAndOrigin&> future;
    GetMediaDeviceSaltAndOrigin(frame_id_, future.GetCallback());
    return future.Get();
  }

  void GenerateStreams(
      GlobalRenderFrameHostId render_frame_host_id,
      const blink::StreamControls& controls,
      MediaDeviceSaltAndOrigin salt_and_origin,
      MediaStreamManager::GenerateStreamsCallback generate_stream_cb) {
    GetIOThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &MediaStreamManager::GenerateStreams,
            base::Unretained(
                BrowserMainLoop::GetInstance()->media_stream_manager()),
            render_frame_host_id, /*requester_id=*/0, /*page_request_id=*/0,
            controls, salt_and_origin,
            /*user_gesture=*/false,
            /*audio_stream_selection_info_ptr=*/
            NewSearchBySessionId({}),
            base::BindPostTaskToCurrentDefault(std::move(generate_stream_cb)),
            /*device_stopped_cb=*/base::DoNothing(),
            /*device_changed_cb=*/base::DoNothing(),
            /*device_request_state_change_cb*/ base::DoNothing(),
            /*device_capture_configuration_change_cb=*/base::DoNothing(),
            /*device_capture_handle_change_cb=*/base::DoNothing(),
            /*zoom_level_change_callback=*/base::DoNothing()));
  }

  GlobalRenderFrameHostId frame_id_;
  url::Origin origin_;
  MediaDeviceEnumeration device_enumeration_;
};

// This test provides coverage for the utilities in
// content/public/media_device_id.h
IN_PROC_BROWSER_TEST_F(MediaDevicesUtilBrowserTest, TranslateDeviceIdAndBack) {
  const std::string salt = CreateRandomMediaDeviceIDSalt();
  EXPECT_FALSE(salt.empty());
  for (int i = 0; i < static_cast<int>(MediaDeviceType::kNumMediaDeviceTypes);
       ++i) {
    MediaDeviceType device_type = static_cast<MediaDeviceType>(i);
    for (const auto& device_info : device_enumeration_[i]) {
      testing::Message message;
      message << "Testing device_type = " << device_type
              << ", raw device ID = " << device_info.device_id;
      SCOPED_TRACE(message);
      std::string hmac_device_id =
          GetHMACForMediaDeviceID(salt, origin_, device_info.device_id);
      VerifyHMACDeviceID(device_type, hmac_device_id, device_info.device_id);

      std::optional<MediaStreamType> stream_type =
          ToMediaStreamType(device_type);
      EXPECT_EQ(stream_type.has_value(),
                device_type != MediaDeviceType::kMediaAudioOutput);
      if (!stream_type.has_value()) {
        continue;
      }
      base::test::TestFuture<const std::optional<std::string>&> future;
      GetMediaDeviceIDForHMAC(*stream_type, salt, origin_, hmac_device_id,
                              base::SequencedTaskRunner::GetCurrentDefault(),
                              future.GetCallback());
      std::optional<std::string> raw_device_id = future.Get();
      EXPECT_THAT(raw_device_id, Optional(device_info.device_id));
    }
  }
}

IN_PROC_BROWSER_TEST_F(MediaDevicesUtilBrowserTest,
                       GetMediaDeviceSaltAndOrigin) {
  MediaDeviceSaltAndOrigin salt_and_origin = GetSaltAndOrigin();
  EXPECT_FALSE(salt_and_origin.device_id_salt().empty());
  EXPECT_FALSE(salt_and_origin.group_id_salt().empty());
  EXPECT_NE(salt_and_origin.device_id_salt(), salt_and_origin.group_id_salt());
  EXPECT_EQ(
      salt_and_origin.origin(),
      shell()->web_contents()->GetPrimaryMainFrame()->GetLastCommittedOrigin());
}

IN_PROC_BROWSER_TEST_F(MediaDevicesUtilBrowserTest,
                       TranslateMediaDeviceInfoArrayWithPermission) {
  MediaDeviceSaltAndOrigin salt_and_origin = GetSaltAndOrigin();
  for (const auto& device_infos : device_enumeration_) {
    blink::WebMediaDeviceInfoArray web_media_device_infos =
        TranslateMediaDeviceInfoArray(/*has_permission=*/true, salt_and_origin,
                                      device_infos);
    EXPECT_EQ(web_media_device_infos.size(), device_infos.size());
    for (size_t j = 0; j < device_infos.size(); ++j) {
      EXPECT_EQ(web_media_device_infos[j].device_id,
                GetHMACForRawMediaDeviceID(salt_and_origin,
                                           device_infos[j].device_id));
      EXPECT_EQ(web_media_device_infos[j].label, device_infos[j].label);
      EXPECT_EQ(
          web_media_device_infos[j].group_id,
          GetHMACForRawMediaDeviceID(salt_and_origin, device_infos[j].group_id,
                                     /*use_group_salt=*/true));
    }
  }
}

IN_PROC_BROWSER_TEST_F(MediaDevicesUtilBrowserTest,
                       TranslateMediaDeviceInfoArrayWithoutPermission) {
  MediaDeviceSaltAndOrigin salt_and_origin = GetSaltAndOrigin();
  for (const auto& device_infos : device_enumeration_) {
    blink::WebMediaDeviceInfoArray web_media_device_infos =
        TranslateMediaDeviceInfoArray(/*has_permission=*/false, salt_and_origin,
                                      device_infos);
    EXPECT_EQ(web_media_device_infos.size(), 1u);
    for (const auto& device_info : web_media_device_infos) {
      EXPECT_TRUE(device_info.device_id.empty());
      EXPECT_TRUE(device_info.label.empty());
      EXPECT_TRUE(device_info.group_id.empty());
    }
  }
}

IN_PROC_BROWSER_TEST_F(MediaDevicesUtilBrowserTest, TranslationWithoutSalt) {
  for (int i = 0; i < static_cast<int>(MediaDeviceType::kNumMediaDeviceTypes);
       ++i) {
    SCOPED_TRACE(base::StringPrintf("device_type %d", i));
    MediaDeviceType device_type = static_cast<MediaDeviceType>(i);
    for (const auto& device_info : device_enumeration_[i]) {
      base::test::TestFuture<const std::string&> future_hmac;
      GetHMACFromRawDeviceId(frame_id_, device_info.device_id,
                             future_hmac.GetCallback());
      std::string hmac_device_id = future_hmac.Get();
      VerifyHMACDeviceID(device_type, hmac_device_id, device_info.device_id);

      base::test::TestFuture<const std::optional<std::string>&> future_raw;
      GetRawDeviceIdFromHMAC(frame_id_, hmac_device_id, device_type,
                             future_raw.GetCallback());
      std::optional<std::string> raw_device_id = future_raw.Get();
      EXPECT_THAT(raw_device_id, Optional(device_info.device_id));
    }
  }
}

IN_PROC_BROWSER_TEST_F(MediaDevicesUtilBrowserTest,
                       GetRawMediaDeviceIDForHMAC) {
  const MediaDeviceSaltAndOrigin salt_and_origin = GetSaltAndOrigin();
  const std::string existing_raw_device_id =
      device_enumeration_[0][0].device_id;
  const std::string existing_hmac_device_id =
      GetHMACForRawMediaDeviceID(salt_and_origin, existing_raw_device_id);

  base::test::TestFuture<const std::optional<std::string>&> future;
  GetIOThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&GetRawDeviceIDForMediaStreamHMAC,
                                MediaStreamType::DEVICE_AUDIO_CAPTURE,
                                salt_and_origin, existing_hmac_device_id,
                                base::SequencedTaskRunner::GetCurrentDefault(),
                                future.GetCallback()));
  std::optional<std::string> raw_device_id = future.Get();
  ASSERT_TRUE(raw_device_id.has_value());
  EXPECT_EQ(*raw_device_id, existing_raw_device_id);
}

IN_PROC_BROWSER_TEST_F(MediaDevicesUtilBrowserTest, GetRawAudioOutputDeviceID) {
  const MediaDeviceSaltAndOrigin salt_and_origin = GetSaltAndOrigin();
  const std::string existing_raw_device_id =
      device_enumeration_[static_cast<size_t>(
          MediaDeviceType::kMediaAudioOutput)][0]
          .device_id;
  const std::string existing_hmac_device_id =
      GetHMACForRawMediaDeviceID(salt_and_origin, existing_raw_device_id);

  base::test::TestFuture<const std::optional<std::string>&> future;
  GetIOThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&GetRawDeviceIDForMediaDeviceHMAC,
                                MediaDeviceType::kMediaAudioOutput,
                                salt_and_origin, existing_hmac_device_id,
                                base::SequencedTaskRunner::GetCurrentDefault(),
                                future.GetCallback()));
  std::optional<std::string> raw_device_id = future.Get();
  ASSERT_TRUE(raw_device_id.has_value());
  EXPECT_EQ(*raw_device_id, existing_raw_device_id);
}

IN_PROC_BROWSER_TEST_F(MediaDevicesUtilBrowserTest,
                       GetRawDeviceIDForNonexistingHMAC) {
  base::test::TestFuture<const std::optional<std::string>&> future;
  GetIOThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&GetRawDeviceIDForMediaDeviceHMAC,
                     MediaDeviceType::kMediaAudioOutput, GetSaltAndOrigin(),
                     "nonexisting_hmac_device_id",
                     base::SequencedTaskRunner::GetCurrentDefault(),
                     future.GetCallback()));
  std::optional<std::string> raw_device_id = future.Get();
  EXPECT_FALSE(raw_device_id.has_value());
}

IN_PROC_BROWSER_TEST_F(MediaDevicesUtilBrowserTest,
                       GetMediaCaptureRawDeviceIdsOpenedForWebContents) {
  base::test::TestFuture<void> use_fake_ui_factory_for_tests_future;
  GetIOThreadTaskRunner()->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(
          &MediaStreamManager::UseFakeUIFactoryForTests,
          base::Unretained(
              BrowserMainLoop::GetInstance()->media_stream_manager()),
          base::BindRepeating([]() {
            return std::make_unique<FakeMediaStreamUIProxy>(
                /* tests_use_fake_render_frame_hosts=*/true);
          }),
          /*use_for_gum_desktop_capture=*/false,
          /*captured_tab_id=*/std::nullopt),
      use_fake_ui_factory_for_tests_future.GetCallback());
  ASSERT_TRUE(use_fake_ui_factory_for_tests_future.Wait());

  auto audio_devices = device_enumeration_[static_cast<size_t>(
      MediaDeviceType::kMediaAudioInput)];
  const auto kDevice1RawId = audio_devices[0].device_id;
  const auto kDevice1HMACId =
      GetHMACForRawMediaDeviceID(GetSaltAndOrigin(), kDevice1RawId);

  const auto kDevice2RawId = audio_devices[1].device_id;
  const auto kDevice2HMACId =
      GetHMACForRawMediaDeviceID(GetSaltAndOrigin(), kDevice2RawId);

  base::test::TestFuture<blink::mojom::MediaStreamRequestResult,
                         const std::string&, blink::mojom::StreamDevicesSetPtr,
                         bool>
      streams_generated_future;

  GenerateStreams(frame_id_, GetAudioStreamControls(kDevice2HMACId),
                  GetSaltAndOrigin(), streams_generated_future.GetCallback());

  ASSERT_EQ(std::get<0>(streams_generated_future.Take()),
            blink::mojom::MediaStreamRequestResult::OK)
      << "GenerateStreams() call failed";

  // Open device 1 on another render frame id. Device 1 shouldn't be
  // included in the response from `GetRawDeviceIdsOpenedForFrame()`.
  GenerateStreams({42, 38}, GetAudioStreamControls(kDevice1HMACId),
                  GetSaltAndOrigin(), streams_generated_future.GetCallback());

  ASSERT_EQ(std::get<0>(streams_generated_future.Take()),
            blink::mojom::MediaStreamRequestResult::OK)
      << "GenerateStreams() call failed";

  base::test::TestFuture<std::vector<std::string>>
      get_raw_device_ids_opened_for_web_contents_future;
  shell()->web_contents()->GetMediaCaptureRawDeviceIdsOpened(
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
      get_raw_device_ids_opened_for_web_contents_future.GetCallback());
  EXPECT_THAT(get_raw_device_ids_opened_for_web_contents_future.Get(),
              ElementsAre(kDevice2RawId));
}

}  // namespace content
