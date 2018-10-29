// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/android/build_info.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "media/base/android/media_drm_bridge.h"
#include "media/base/provision_fetcher.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/widevine/cdm/widevine_cdm_common.h"

namespace media {

#define EXPECT_TRUE_IF_WIDEVINE_AVAILABLE(a)                              \
  do {                                                                    \
    if (!MediaDrmBridge::IsKeySystemSupported(kWidevineKeySystem)) {      \
      VLOG(0) << "Widevine not supported on device.";                     \
      EXPECT_FALSE(a);                                                    \
    } else {                                                              \
      EXPECT_TRUE(a);                                                     \
    }                                                                     \
  } while (0)

const char kAudioMp4[] = "audio/mp4";
const char kVideoMp4[] = "video/mp4";
const char kAudioWebM[] = "audio/webm";
const char kVideoWebM[] = "video/webm";
const char kInvalidKeySystem[] = "invalid.keysystem";
const MediaDrmBridge::SecurityLevel kDefault =
    MediaDrmBridge::SECURITY_LEVEL_DEFAULT;
const MediaDrmBridge::SecurityLevel kL1 = MediaDrmBridge::SECURITY_LEVEL_1;
const MediaDrmBridge::SecurityLevel kL3 = MediaDrmBridge::SECURITY_LEVEL_3;
const char kTestOrigin[] = "http://www.example.com";

// Helper functions to avoid typing "MediaDrmBridge::" in tests.

static bool IsKeySystemSupportedWithType(
    const std::string& key_system,
    const std::string& container_mime_type) {
  return MediaDrmBridge::IsKeySystemSupportedWithType(key_system,
                                                      container_mime_type);
}

namespace {
// Mock ProvisionFetcher.
class MockProvisionFetcher : public ProvisionFetcher {
 public:
  static std::unique_ptr<ProvisionFetcher> Create() {
    return base::WrapUnique(new MockProvisionFetcher);
  }

  // ProvisionFetcher implementation.
  void Retrieve(const std::string& default_url,
                const std::string& request_data,
                const ResponseCB& response_cb) override {}
};
}  // namespace (anonymous)

TEST(MediaDrmBridgeTest, IsKeySystemSupported_Widevine) {
  // TODO(xhwang): Enable when b/13564917 is fixed.
  // EXPECT_TRUE_IF_AVAILABLE(
  //     IsKeySystemSupportedWithType(kWidevineKeySystem, kAudioMp4));
  EXPECT_TRUE_IF_WIDEVINE_AVAILABLE(
      IsKeySystemSupportedWithType(kWidevineKeySystem, kVideoMp4));

  if (base::android::BuildInfo::GetInstance()->sdk_int() <=
      base::android::SDK_VERSION_KITKAT) {
    EXPECT_FALSE(IsKeySystemSupportedWithType(kWidevineKeySystem, kAudioWebM));
    EXPECT_FALSE(IsKeySystemSupportedWithType(kWidevineKeySystem, kVideoWebM));
  } else {
    EXPECT_TRUE_IF_WIDEVINE_AVAILABLE(
        IsKeySystemSupportedWithType(kWidevineKeySystem, kAudioWebM));
    EXPECT_TRUE_IF_WIDEVINE_AVAILABLE(
        IsKeySystemSupportedWithType(kWidevineKeySystem, kVideoWebM));
  }

  EXPECT_FALSE(IsKeySystemSupportedWithType(kWidevineKeySystem, "unknown"));
  EXPECT_FALSE(IsKeySystemSupportedWithType(kWidevineKeySystem, "video/avi"));
  EXPECT_FALSE(IsKeySystemSupportedWithType(kWidevineKeySystem, "audio/mp3"));
}

// Invalid key system is NOT supported regardless whether MediaDrm is available.
TEST(MediaDrmBridgeTest, IsKeySystemSupported_InvalidKeySystem) {
  EXPECT_FALSE(MediaDrmBridge::IsKeySystemSupported(kInvalidKeySystem));
  EXPECT_FALSE(IsKeySystemSupportedWithType(kInvalidKeySystem, kAudioMp4));
  EXPECT_FALSE(IsKeySystemSupportedWithType(kInvalidKeySystem, kVideoMp4));
  EXPECT_FALSE(IsKeySystemSupportedWithType(kInvalidKeySystem, kAudioWebM));
  EXPECT_FALSE(IsKeySystemSupportedWithType(kInvalidKeySystem, kVideoWebM));
  EXPECT_FALSE(IsKeySystemSupportedWithType(kInvalidKeySystem, "unknown"));
  EXPECT_FALSE(IsKeySystemSupportedWithType(kInvalidKeySystem, "video/avi"));
  EXPECT_FALSE(IsKeySystemSupportedWithType(kInvalidKeySystem, "audio/mp3"));
}

TEST(MediaDrmBridgeTest, CreateWithoutSessionSupport_Widevine) {
  base::MessageLoop message_loop_;
  EXPECT_TRUE_IF_WIDEVINE_AVAILABLE(MediaDrmBridge::CreateWithoutSessionSupport(
      kWidevineKeySystem, kTestOrigin, kDefault,
      base::Bind(&MockProvisionFetcher::Create)));
}

// Invalid key system is NOT supported regardless whether MediaDrm is available.
TEST(MediaDrmBridgeTest, CreateWithoutSessionSupport_InvalidKeySystem) {
  base::MessageLoop message_loop_;
  EXPECT_FALSE(MediaDrmBridge::CreateWithoutSessionSupport(
      kInvalidKeySystem, kTestOrigin, kDefault,
      base::Bind(&MockProvisionFetcher::Create)));
}

TEST(MediaDrmBridgeTest, CreateWithSecurityLevel_Widevine) {
  base::MessageLoop message_loop_;

  // We test "L3" fully. But for "L1" we don't check the result as it depends on
  // whether the test device supports "L1".
  EXPECT_TRUE_IF_WIDEVINE_AVAILABLE(MediaDrmBridge::CreateWithoutSessionSupport(
      kWidevineKeySystem, kTestOrigin, kL3,
      base::Bind(&MockProvisionFetcher::Create)));

  MediaDrmBridge::CreateWithoutSessionSupport(
      kWidevineKeySystem, kTestOrigin, kL1,
      base::Bind(&MockProvisionFetcher::Create));
}

}  // namespace media
