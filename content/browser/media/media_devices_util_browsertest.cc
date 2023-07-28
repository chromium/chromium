// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_devices_util.h"

#include "base/feature_list.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/renderer_host/media/media_devices_manager.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/media_device_id.h"
#include "content/public/common/content_features.h"
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
using ::testing::Eq;
using ::testing::Optional;

namespace {

// Returns true if the `device_id` corresponds to the system default or
// communications device, false otherwise.
bool IsSpecialAudioDeviceId(MediaDeviceType device_type,
                            const std::string& device_id) {
  return (device_type == MediaDeviceType::MEDIA_AUDIO_INPUT ||
          device_type == MediaDeviceType::MEDIA_AUDIO_OUTPUT) &&
         (media::AudioDeviceDescription::IsDefaultDevice(device_id) ||
          media::AudioDeviceDescription::IsCommunicationsDevice(device_id));
}

absl::optional<MediaStreamType> ToMediaStreamType(MediaDeviceType device_type) {
  switch (device_type) {
    case MediaDeviceType::MEDIA_AUDIO_INPUT:
      return MediaStreamType::DEVICE_AUDIO_CAPTURE;
    case MediaDeviceType::MEDIA_VIDEO_INPUT:
      return MediaStreamType::DEVICE_VIDEO_CAPTURE;
    default:
      return absl::nullopt;
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

}  // namespace

class MediaDevicesUtilBrowserTest : public ContentBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kUseFakeDeviceForMediaStream);
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    frame_id_ = shell()->web_contents()->GetPrimaryMainFrame()->GetGlobalId();
    device_enumeration_ = EnumerateDevices();
    ASSERT_EQ(device_enumeration_.size(),
              static_cast<size_t>(MediaDeviceType::NUM_MEDIA_DEVICE_TYPES));
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
          types[static_cast<size_t>(MediaDeviceType::MEDIA_AUDIO_INPUT)] = true;
          types[static_cast<size_t>(MediaDeviceType::MEDIA_AUDIO_OUTPUT)] =
              true;
          types[static_cast<size_t>(MediaDeviceType::MEDIA_VIDEO_INPUT)] = true;
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

  bool ShouldHideDeviceIDs() {
    return base::FeatureList::IsEnabled(
        features::kEnumerateDevicesHideDeviceIDs);
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
  for (int i = 0; i < static_cast<int>(MediaDeviceType::NUM_MEDIA_DEVICE_TYPES);
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

      absl::optional<MediaStreamType> stream_type =
          ToMediaStreamType(device_type);
      EXPECT_EQ(stream_type.has_value(),
                device_type != MediaDeviceType::MEDIA_AUDIO_OUTPUT);
      if (!stream_type.has_value()) {
        continue;
      }
      base::test::TestFuture<const absl::optional<std::string>&> future;
      GetMediaDeviceIDForHMAC(*stream_type, salt, origin_, hmac_device_id,
                              base::SequencedTaskRunner::GetCurrentDefault(),
                              future.GetCallback());
      absl::optional<std::string> raw_device_id = future.Get();
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
    EXPECT_EQ(web_media_device_infos.size(),
              ShouldHideDeviceIDs() ? 1u : device_infos.size());
    for (const auto& device_info : web_media_device_infos) {
      EXPECT_EQ(device_info.device_id.empty(), ShouldHideDeviceIDs());
      EXPECT_TRUE(device_info.label.empty());
      EXPECT_EQ(device_info.group_id.empty(), ShouldHideDeviceIDs());
    }
  }
}

IN_PROC_BROWSER_TEST_F(MediaDevicesUtilBrowserTest, TranslationWithoutSalt) {
  for (int i = 0; i < static_cast<int>(MediaDeviceType::NUM_MEDIA_DEVICE_TYPES);
       ++i) {
    SCOPED_TRACE(base::StringPrintf("device_type %d", i));
    MediaDeviceType device_type = static_cast<MediaDeviceType>(i);
    for (const auto& device_info : device_enumeration_[i]) {
      base::test::TestFuture<const std::string&> future_hmac;
      GetHMACFromRawDeviceId(frame_id_, device_info.device_id,
                             future_hmac.GetCallback());
      std::string hmac_device_id = future_hmac.Get();
      VerifyHMACDeviceID(device_type, hmac_device_id, device_info.device_id);

      base::test::TestFuture<const absl::optional<std::string>&> future_raw;
      GetRawDeviceIdFromHMAC(frame_id_, hmac_device_id, device_type,
                             future_raw.GetCallback());
      absl::optional<std::string> raw_device_id = future_raw.Get();
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

  base::test::TestFuture<const absl::optional<std::string>&> future;
  GetIOThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&GetRawDeviceIDForMediaStreamHMAC,
                                MediaStreamType::DEVICE_AUDIO_CAPTURE,
                                salt_and_origin, existing_hmac_device_id,
                                base::SequencedTaskRunner::GetCurrentDefault(),
                                future.GetCallback()));
  absl::optional<std::string> raw_device_id = future.Get();
  ASSERT_TRUE(raw_device_id.has_value());
  EXPECT_EQ(*raw_device_id, existing_raw_device_id);
}

IN_PROC_BROWSER_TEST_F(MediaDevicesUtilBrowserTest, GetRawAudioOutputDeviceID) {
  const MediaDeviceSaltAndOrigin salt_and_origin = GetSaltAndOrigin();
  const std::string existing_raw_device_id =
      device_enumeration_[static_cast<size_t>(
          MediaDeviceType::MEDIA_AUDIO_OUTPUT)][0]
          .device_id;
  const std::string existing_hmac_device_id =
      GetHMACForRawMediaDeviceID(salt_and_origin, existing_raw_device_id);

  base::test::TestFuture<const absl::optional<std::string>&> future;
  GetIOThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&GetRawDeviceIDForMediaDeviceHMAC,
                                MediaDeviceType::MEDIA_AUDIO_OUTPUT,
                                salt_and_origin, existing_hmac_device_id,
                                base::SequencedTaskRunner::GetCurrentDefault(),
                                future.GetCallback()));
  absl::optional<std::string> raw_device_id = future.Get();
  ASSERT_TRUE(raw_device_id.has_value());
  EXPECT_EQ(*raw_device_id, existing_raw_device_id);
}

IN_PROC_BROWSER_TEST_F(MediaDevicesUtilBrowserTest,
                       GetRawDeviceIDForNonexistingHMAC) {
  base::test::TestFuture<const absl::optional<std::string>&> future;
  GetIOThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&GetRawDeviceIDForMediaDeviceHMAC,
                     MediaDeviceType::MEDIA_AUDIO_OUTPUT, GetSaltAndOrigin(),
                     "nonexisting_hmac_device_id",
                     base::SequencedTaskRunner::GetCurrentDefault(),
                     future.GetCallback()));
  absl::optional<std::string> raw_device_id = future.Get();
  EXPECT_FALSE(raw_device_id.has_value());
}

}  // namespace content
