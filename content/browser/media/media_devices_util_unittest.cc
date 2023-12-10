// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_devices_util.h"

#include "media/audio/audio_device_description.h"
#include "media/capture/video/video_capture_device_descriptor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/mediastream/media_devices.h"

namespace content {

using ::blink::mojom::MediaDeviceType;
using ::blink::mojom::MediaStreamType;
using ::testing::Eq;
using ::testing::Optional;

namespace {

MediaDeviceSaltAndOrigin ExampleSaltAndOrigin() {
  return MediaDeviceSaltAndOrigin(
      "Device ID Salt", url::Origin::Create(GURL("https://example.com")),
      "Group ID Salt");
}

blink::WebMediaDeviceInfo ExampleWebMediaDeviceInfo() {
  return blink::WebMediaDeviceInfo(
      /*device_id=*/"example device id",
      /*label=*/"example label",
      /*group_id=*/"example_group_id",
      /*video_control_support=*/{.pan = true, .tilt = true, .zoom = true},
      blink::mojom::FacingMode::kEnvironment,
      media::CameraAvailability::kAvailable);
}

}  // namespace

TEST(MediaDevicesUtilTest, TranslateEmptyMediaDeviceInfoArray) {
  MediaDeviceSaltAndOrigin salt_and_origin = ExampleSaltAndOrigin();
  blink::WebMediaDeviceInfoArray web_media_device_infos =
      TranslateMediaDeviceInfoArray(/*has_permission=*/false, salt_and_origin,
                                    {});
  EXPECT_TRUE(web_media_device_infos.empty());
  web_media_device_infos = TranslateMediaDeviceInfoArray(
      /*has_permission=*/true, salt_and_origin, {});
  EXPECT_TRUE(web_media_device_infos.empty());
}

TEST(MediaDevicesUtilBrowserTest, TranslateMediaDeviceInfoWithPermission) {
  MediaDeviceSaltAndOrigin salt_and_origin = ExampleSaltAndOrigin();
  blink::WebMediaDeviceInfo original_info = ExampleWebMediaDeviceInfo();
  blink::WebMediaDeviceInfo translated_info = TranslateMediaDeviceInfo(
      /*has_permission=*/true, salt_and_origin, original_info);
  EXPECT_EQ(
      translated_info.device_id,
      GetHMACForRawMediaDeviceID(salt_and_origin, original_info.device_id));
  EXPECT_EQ(translated_info.label, original_info.label);
  EXPECT_EQ(translated_info.group_id,
            GetHMACForRawMediaDeviceID(salt_and_origin, original_info.group_id,
                                       /*use_group_salt=*/true));
  EXPECT_EQ(translated_info.video_facing, original_info.video_facing);
  EXPECT_EQ(translated_info.video_control_support.pan,
            original_info.video_control_support.pan);
  EXPECT_EQ(translated_info.video_control_support.tilt,
            original_info.video_control_support.tilt);
  EXPECT_EQ(translated_info.video_control_support.zoom,
            original_info.video_control_support.zoom);
  EXPECT_EQ(translated_info.availability, original_info.availability);
}

TEST(MediaDevicesUtilBrowserTest, TranslateMediaDeviceInfoWithoutPermission) {
  MediaDeviceSaltAndOrigin salt_and_origin = ExampleSaltAndOrigin();
  blink::WebMediaDeviceInfo original_info = ExampleWebMediaDeviceInfo();
  blink::WebMediaDeviceInfo translated_info = TranslateMediaDeviceInfo(
      /*has_permission=*/false, salt_and_origin, original_info);
  EXPECT_TRUE(translated_info.device_id.empty());
  EXPECT_TRUE(translated_info.label.empty());
  EXPECT_TRUE(translated_info.group_id.empty());
  EXPECT_EQ(translated_info.video_facing, blink::mojom::FacingMode::kNone);
  EXPECT_FALSE(translated_info.video_control_support.pan);
  EXPECT_FALSE(translated_info.video_control_support.tilt);
  EXPECT_FALSE(translated_info.video_control_support.zoom);
  EXPECT_FALSE(translated_info.availability.has_value());
}

TEST(MediaDevicesUtilTest, TranslateSpecialDeviceIDs) {
  MediaDeviceSaltAndOrigin salt_and_origin = ExampleSaltAndOrigin();
  const std::string raw_default_id(
      media::AudioDeviceDescription::kDefaultDeviceId);
  const std::string hashed_default_id =
      GetHMACForRawMediaDeviceID(salt_and_origin, raw_default_id);
  EXPECT_TRUE(DoesRawMediaDeviceIDMatchHMAC(salt_and_origin, hashed_default_id,
                                            raw_default_id));
  EXPECT_EQ(raw_default_id, hashed_default_id);

  const std::string raw_communications_id(
      media::AudioDeviceDescription::kCommunicationsDeviceId);
  const std::string hashed_communications_id =
      GetHMACForRawMediaDeviceID(salt_and_origin, raw_communications_id);
  EXPECT_TRUE(DoesRawMediaDeviceIDMatchHMAC(
      salt_and_origin, hashed_communications_id, raw_communications_id));
  EXPECT_EQ(raw_communications_id, hashed_communications_id);

  const std::string raw_empty_id;
  const std::string hashed_empty_id =
      GetHMACForRawMediaDeviceID(salt_and_origin, raw_empty_id);
  EXPECT_TRUE(DoesRawMediaDeviceIDMatchHMAC(salt_and_origin, hashed_empty_id,
                                            raw_empty_id));
  EXPECT_EQ(raw_empty_id, hashed_empty_id);
}

TEST(MediaDevicesUtilTest, TranslateNonSpecialDeviceID) {
  MediaDeviceSaltAndOrigin salt_and_origin = ExampleSaltAndOrigin();
  const std::string raw_other_id("other-unique-id");
  const std::string hashed_other_id =
      GetHMACForRawMediaDeviceID(salt_and_origin, raw_other_id);
  EXPECT_TRUE(DoesRawMediaDeviceIDMatchHMAC(salt_and_origin, hashed_other_id,
                                            raw_other_id));
  EXPECT_FALSE(DoesRawMediaDeviceIDMatchHMAC(
      salt_and_origin, hashed_other_id,
      media::AudioDeviceDescription::kDefaultDeviceId));
  EXPECT_FALSE(DoesRawMediaDeviceIDMatchHMAC(
      salt_and_origin, hashed_other_id,
      media::AudioDeviceDescription::kCommunicationsDeviceId));
  EXPECT_NE(raw_other_id, hashed_other_id);
  EXPECT_EQ(hashed_other_id.size(), 64U);
  for (const char& c : hashed_other_id) {
    EXPECT_TRUE(base::IsAsciiDigit(c) || (c >= 'a' && c <= 'f'));
  }
}

}  // namespace content
