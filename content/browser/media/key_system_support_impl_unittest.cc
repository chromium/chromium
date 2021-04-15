// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/key_system_support_impl.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/token.h"
#include "content/browser/media/cdm_registry_impl.h"
#include "content/public/browser/cdm_registry.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/common/cdm_info.h"
#include "content/public/common/webplugininfo.h"
#include "content/public/test/browser_task_environment.h"
#include "media/base/decrypt_config.h"
#include "media/base/media_switches.h"
#include "media/base/video_codecs.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

using VideoCodec = media::VideoCodec;
using EncryptionScheme = media::EncryptionScheme;
using CdmSessionType = media::CdmSessionType;
using Robustness = CdmInfo::Robustness;

const char kTestCdmName[] = "Test Content Decryption Module";
const base::Token kTestCdmGuid{1234, 5678};
const char kVersion[] = "1.1.1.1";
const char kTestPath[] = "/aa/bb";
const char kTestFileSystemId[] = "file_system_id";

// Helper function to compare a STL container to an initializer_list.
template <typename Container, typename T>
bool StlEquals(const Container a, std::initializer_list<T> b) {
  return a == Container(b);
}

#define EXPECT_STL_EQ(a, ...)                 \
  do {                                        \
    EXPECT_TRUE(StlEquals(a, {__VA_ARGS__})); \
  } while (false)

#define EXPECT_VIDEO_CODECS(...) \
  EXPECT_STL_EQ(capability_->video_codecs, __VA_ARGS__)

#define EXPECT_ENCRYPTION_SCHEMES(...) \
  EXPECT_STL_EQ(capability_->encryption_schemes, __VA_ARGS__)

#define EXPECT_SESSION_TYPES(...) \
  EXPECT_STL_EQ(capability_->session_types, __VA_ARGS__)

#define EXPECT_HW_SECURE_VIDEO_CODECS(...) \
  EXPECT_STL_EQ(capability_->hw_secure_video_codecs, __VA_ARGS__)

#define EXPECT_HW_SECURE_ENCRYPTION_SCHEMES(...) \
  EXPECT_STL_EQ(capability_->hw_secure_encryption_schemes, __VA_ARGS__)

}  // namespace

class KeySystemSupportTest : public testing::Test {
 protected:
  void SetUp() final {
    DVLOG(1) << __func__;
    // As `CdmRegistry::GetInstance()` is a static, explicitly reset
    // `CdmRegistry` so each test starts with a clean state.
    static_cast<CdmRegistryImpl*>(CdmRegistry::GetInstance())
        ->ResetForTesting();

    KeySystemSupportImpl::Create(
        key_system_support_.BindNewPipeAndPassReceiver());
  }

  CdmCapability TestCdmCapability() {
    return CdmCapability(
        {VideoCodec::kCodecVP8, VideoCodec::kCodecVP9},
        {EncryptionScheme::kCenc, EncryptionScheme::kCbcs},
        {CdmSessionType::kTemporary, CdmSessionType::kPersistentLicense});
  }

  // Registers |key_system| with |capability|. All other values for CdmInfo have
  // some default value as they're not returned by IsKeySystemSupported().
  void Register(const std::string& key_system,
                CdmCapability capability,
                Robustness robustness = Robustness::kSoftwareSecure) {
    DVLOG(1) << __func__;

    CdmRegistry::GetInstance()->RegisterCdm(
        CdmInfo(key_system, robustness, std::move(capability),
                /*supports_sub_key_systems=*/false, kTestCdmName, kTestCdmGuid,
                base::Version(kVersion),
                base::FilePath::FromUTF8Unsafe(kTestPath), kTestFileSystemId));
  }

  // Determines if |key_system| is registered. If it is, updates |codecs_|
  // and |persistent_|.
  bool IsSupported(const std::string& key_system) {
    DVLOG(1) << __func__;
    bool is_supported = false;
    key_system_support_->IsKeySystemSupported(key_system, &is_supported,
                                              &capability_);
    return is_supported;
  }

  mojo::Remote<media::mojom::KeySystemSupport> key_system_support_;
  base::test::ScopedFeatureList scoped_feature_list_;
  BrowserTaskEnvironment task_environment_;

  // Updated by IsSupported().
  media::mojom::KeySystemCapabilityPtr capability_;
};

TEST_F(KeySystemSupportTest, NoKeySystems) {
  EXPECT_FALSE(IsSupported("KeySystem"));
  EXPECT_FALSE(capability_);
}

TEST_F(KeySystemSupportTest, SoftwareSecureCapability) {
  Register("KeySystem", TestCdmCapability());

  EXPECT_TRUE(IsSupported("KeySystem"));
  EXPECT_VIDEO_CODECS(VideoCodec::kCodecVP8, VideoCodec::kCodecVP9);
  EXPECT_ENCRYPTION_SCHEMES(EncryptionScheme::kCenc, EncryptionScheme::kCbcs);
  EXPECT_SESSION_TYPES(CdmSessionType::kTemporary,
                       CdmSessionType::kPersistentLicense);
}

TEST_F(KeySystemSupportTest,
       HardwareSecureCapability_HardwareSecureDecryptionDisabled) {
  scoped_feature_list_.InitAndDisableFeature(media::kHardwareSecureDecryption);
  Register("KeySystem", TestCdmCapability(), Robustness::kHardwareSecure);

  EXPECT_FALSE(IsSupported("KeySystem"));
}

TEST_F(KeySystemSupportTest, HardwareSecureCapability) {
  scoped_feature_list_.InitAndEnableFeature(media::kHardwareSecureDecryption);
  Register("KeySystem", TestCdmCapability(), Robustness::kHardwareSecure);

  EXPECT_TRUE(IsSupported("KeySystem"));
  EXPECT_HW_SECURE_VIDEO_CODECS(VideoCodec::kCodecVP8, VideoCodec::kCodecVP9);
  EXPECT_HW_SECURE_ENCRYPTION_SCHEMES(EncryptionScheme::kCenc,
                                      EncryptionScheme::kCbcs);
  // TODO(xhwang): Support hardware secure session types.
  EXPECT_TRUE(capability_->session_types.empty());
}

TEST_F(KeySystemSupportTest, MultipleKeySystems) {
  Register("KeySystem1", TestCdmCapability());
  Register("KeySystem2", TestCdmCapability());

  EXPECT_TRUE(IsSupported("KeySystem1"));
  EXPECT_TRUE(IsSupported("KeySystem2"));
}

TEST_F(KeySystemSupportTest, MissingKeySystem) {
  Register("KeySystem", TestCdmCapability());

  EXPECT_FALSE(IsSupported("KeySystem1"));
  EXPECT_FALSE(capability_);
}

}  // namespace content
