// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(sandersd): Refactor to remove recomputed codec arrays, and generally
// shorten and improve coverage.
//   - http://crbug.com/417444
//   - http://crbug.com/457438
// TODO(sandersd): Add tests to cover codec vectors with empty items.
// http://crbug.com/417461

#include <string>
#include <vector>

#include "base/logging.h"
#include "media/base/audio_parameters.h"
#include "media/base/decrypt_config.h"
#include "media/base/eme_constants.h"
#include "media/base/key_systems.h"
#include "media/base/media.h"
#include "media/base/media_client.h"
#include "media/media_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/widevine/cdm/widevine_cdm_common.h"

namespace media {

namespace {

// These are the (fake) key systems that are registered for these tests.
// kUsesAes uses the AesDecryptor like Clear Key.
// kExternal uses an external CDM, such as library CDM or Android platform CDM.
const char kUsesAes[] = "x-org.example.usesaes";
const char kExternal[] = "x-com.example.external";

const char kClearKey[] = "org.w3.clearkey";
const char kExternalClearKey[] = "org.chromium.externalclearkey";

const char kAudioWebM[] = "audio/webm";
const char kVideoWebM[] = "video/webm";
const char kAudioFoo[] = "audio/foo";
const char kVideoFoo[] = "video/foo";

const char kRobustnessSupported[] = "supported";
const char kRobustnessSecureCodecsRequired[] = "secure-codecs-required";
const char kRobustnessNotSupported[] = "not-supported";

// Codecs only supported in FOO container. Pick some arbitrary bit fields as
// long as they are not in conflict with the real ones (static_asserted below).
// TODO(crbug.com/724362): Remove container type (FOO) from codec enums.
enum TestCodec : uint32_t {
  TEST_CODEC_FOO_AUDIO = 1 << 25,
  TEST_CODEC_FOO_AUDIO_ALL = TEST_CODEC_FOO_AUDIO,
  TEST_CODEC_FOO_VIDEO = 1 << 26,
  // Only supported by hardware secure codec in kExternal key system.
  TEST_CODEC_FOO_SECURE_VIDEO = 1 << 27,
  TEST_CODEC_FOO_VIDEO_ALL = TEST_CODEC_FOO_VIDEO | TEST_CODEC_FOO_SECURE_VIDEO,
  TEST_CODEC_FOO_ALL = TEST_CODEC_FOO_AUDIO_ALL | TEST_CODEC_FOO_VIDEO_ALL
};

static_assert((TEST_CODEC_FOO_ALL & EME_CODEC_ALL) == EME_CODEC_NONE,
              "test codec masks should only use invalid codec masks");

// Base class to provide default implementations.
class TestKeySystemPropertiesBase : public KeySystemProperties {
 public:
  bool IsSupportedInitDataType(EmeInitDataType init_data_type) const override {
    return init_data_type == EmeInitDataType::WEBM;
  }

  // Note: TEST_CODEC_FOO_SECURE_VIDEO is not supported by default.
  SupportedCodecs GetSupportedCodecs() const override {
    return EME_CODEC_WEBM_ALL | TEST_CODEC_FOO_AUDIO | TEST_CODEC_FOO_VIDEO;
  }

  EmeConfigRule GetRobustnessConfigRule(
      EmeMediaType media_type,
      const std::string& requested_robustness) const override {
    return requested_robustness.empty() ? EmeConfigRule::SUPPORTED
                                        : EmeConfigRule::NOT_SUPPORTED;
  }

  EmeSessionTypeSupport GetPersistentUsageRecordSessionSupport()
      const override {
    return EmeSessionTypeSupport::NOT_SUPPORTED;
  }
};

class AesKeySystemProperties : public TestKeySystemPropertiesBase {
 public:
  AesKeySystemProperties(const std::string& name) : name_(name) {}

  std::string GetKeySystemName() const override { return name_; }

  EmeConfigRule GetEncryptionSchemeConfigRule(
      EncryptionScheme encryption_scheme) const override {
    return (encryption_scheme == EncryptionScheme::kUnencrypted ||
            encryption_scheme == EncryptionScheme::kCenc)
               ? EmeConfigRule::SUPPORTED
               : EmeConfigRule::NOT_SUPPORTED;
  }

  EmeSessionTypeSupport GetPersistentLicenseSessionSupport() const override {
    return EmeSessionTypeSupport::NOT_SUPPORTED;
  }

  EmeFeatureSupport GetPersistentStateSupport() const override {
    return EmeFeatureSupport::NOT_SUPPORTED;
  }

  EmeFeatureSupport GetDistinctiveIdentifierSupport() const override {
    return EmeFeatureSupport::NOT_SUPPORTED;
  }

  bool UseAesDecryptor() const override { return true; }

 private:
  std::string name_;
};

class ExternalKeySystemProperties : public TestKeySystemPropertiesBase {
 public:
  std::string GetKeySystemName() const override { return kExternal; }

  // Pretend clear (unencrypted) and 'cenc' content are always supported. But
  // 'cbcs' is not supported by hardware secure codecs.
  EmeConfigRule GetEncryptionSchemeConfigRule(
      EncryptionScheme encryption_scheme) const override {
    switch (encryption_scheme) {
      case media::EncryptionScheme::kUnencrypted:
      case media::EncryptionScheme::kCenc:
        return media::EmeConfigRule::SUPPORTED;
      case media::EncryptionScheme::kCbcs:
        return media::EmeConfigRule::HW_SECURE_CODECS_NOT_ALLOWED;
    }
    NOTREACHED();
    return media::EmeConfigRule::NOT_SUPPORTED;
  }

  // We have hardware secure codec support for FOO_VIDEO and FOO_SECURE_VIDEO.
  SupportedCodecs GetSupportedHwSecureCodecs() const override {
    return TEST_CODEC_FOO_VIDEO | TEST_CODEC_FOO_SECURE_VIDEO;
  }

  EmeConfigRule GetRobustnessConfigRule(
      EmeMediaType media_type,
      const std::string& requested_robustness) const override {
    if (requested_robustness == kRobustnessSupported)
      return EmeConfigRule::SUPPORTED;
    else if (requested_robustness == kRobustnessSecureCodecsRequired)
      return EmeConfigRule::HW_SECURE_CODECS_REQUIRED;
    else if (requested_robustness == kRobustnessNotSupported)
      return EmeConfigRule::NOT_SUPPORTED;
    else
      NOTREACHED();
    return EmeConfigRule::NOT_SUPPORTED;
  }

  EmeSessionTypeSupport GetPersistentLicenseSessionSupport() const override {
    return EmeSessionTypeSupport::SUPPORTED;
  }

  EmeFeatureSupport GetPersistentStateSupport() const override {
    return EmeFeatureSupport::ALWAYS_ENABLED;
  }

  EmeFeatureSupport GetDistinctiveIdentifierSupport() const override {
    return EmeFeatureSupport::ALWAYS_ENABLED;
  }
};

void ExpectEncryptionSchemeConfigRule(const std::string& key_system,
                                      EncryptionScheme encryption_scheme,
                                      EmeConfigRule expected_rule) {
  EXPECT_EQ(expected_rule,
            KeySystems::GetInstance()->GetEncryptionSchemeConfigRule(
                key_system, encryption_scheme));
}

EmeConfigRule GetVideoContentTypeConfigRule(
    const std::string& mime_type,
    const std::vector<std::string>& codecs,
    const std::string& key_system) {
  return KeySystems::GetInstance()->GetContentTypeConfigRule(
      key_system, EmeMediaType::VIDEO, mime_type, codecs);
}

// Adapt IsSupportedKeySystemWithMediaMimeType() to the new API,
// IsSupportedCodecCombination().
bool IsSupportedKeySystemWithMediaMimeType(
    const std::string& mime_type,
    const std::vector<std::string>& codecs,
    const std::string& key_system) {
  return (GetVideoContentTypeConfigRule(mime_type, codecs, key_system) !=
          EmeConfigRule::NOT_SUPPORTED);
}

bool IsSupportedKeySystemWithAudioMimeType(
    const std::string& mime_type,
    const std::vector<std::string>& codecs,
    const std::string& key_system) {
  return (KeySystems::GetInstance()->GetContentTypeConfigRule(
              key_system, EmeMediaType::AUDIO, mime_type, codecs) !=
          EmeConfigRule::NOT_SUPPORTED);
}

bool IsSupportedKeySystem(const std::string& key_system) {
  return KeySystems::GetInstance()->IsSupportedKeySystem(key_system);
}

EmeConfigRule GetRobustnessConfigRule(const std::string& requested_robustness) {
  return KeySystems::GetInstance()->GetRobustnessConfigRule(
      kExternal, EmeMediaType::VIDEO, requested_robustness);
}

// Adds test container and codec masks.
// This function must be called after SetMediaClient() if a MediaClient will be
// provided.
// More details: AddXxxMask() will create KeySystems if it hasn't been created.
// During KeySystems's construction GetMediaClient() will be used to add key
// systems. In test code, the MediaClient is set by SetMediaClient().
// Therefore, SetMediaClient() must be called before this function to make sure
// MediaClient in effect when constructing KeySystems.
void AddContainerAndCodecMasksForTest() {
  // Since KeySystems is a singleton. Make sure we only add test container and
  // codec masks once per process.
  static bool is_test_masks_added = false;

  if (is_test_masks_added)
    return;

  AddCodecMaskForTesting(EmeMediaType::AUDIO, "fooaudio", TEST_CODEC_FOO_AUDIO);
  AddCodecMaskForTesting(EmeMediaType::VIDEO, "foovideo", TEST_CODEC_FOO_VIDEO);
  AddCodecMaskForTesting(EmeMediaType::VIDEO, "securefoovideo",
                         TEST_CODEC_FOO_SECURE_VIDEO);
  AddMimeTypeCodecMaskForTesting("audio/foo", TEST_CODEC_FOO_AUDIO_ALL);
  AddMimeTypeCodecMaskForTesting("video/foo", TEST_CODEC_FOO_VIDEO_ALL);

  is_test_masks_added = true;
}

bool CanRunExternalKeySystemTests() {
#if defined(OS_ANDROID)
  if (HasPlatformDecoderSupport())
    return true;

  EXPECT_FALSE(IsSupportedKeySystem(kExternal));
  return false;
#else
  return true;
#endif
}

class TestMediaClient : public MediaClient {
 public:
  TestMediaClient();
  ~TestMediaClient() override;

  // MediaClient implementation.
  bool IsKeySystemsUpdateNeeded() final;
  void AddSupportedKeySystems(std::vector<std::unique_ptr<KeySystemProperties>>*
                                  key_systems_properties) override;
  bool IsSupportedAudioType(const media::AudioType& type) final;
  bool IsSupportedVideoType(const media::VideoType& type) final;
  bool IsSupportedBitstreamAudioCodec(AudioCodec codec) final;

  // Helper function to test the case where IsKeySystemsUpdateNeeded() is true
  // after AddSupportedKeySystems() is called.
  void SetKeySystemsUpdateNeeded();

  // Helper function to disable "kExternal" key system support so that we can
  // test the key system update case.
  void DisableExternalKeySystemSupport();

  base::Optional<::media::AudioRendererAlgorithmParameters>
  GetAudioRendererAlgorithmParameters(AudioParameters audio_parameters) final;

 private:
  bool is_update_needed_;
  bool supports_external_key_system_;
};

TestMediaClient::TestMediaClient()
    : is_update_needed_(true), supports_external_key_system_(true) {}

TestMediaClient::~TestMediaClient() = default;

bool TestMediaClient::IsKeySystemsUpdateNeeded() {
  return is_update_needed_;
}

void TestMediaClient::AddSupportedKeySystems(
    std::vector<std::unique_ptr<KeySystemProperties>>* key_systems) {
  DCHECK(is_update_needed_);

  key_systems->emplace_back(new AesKeySystemProperties(kUsesAes));

  if (supports_external_key_system_)
    key_systems->emplace_back(new ExternalKeySystemProperties());

  is_update_needed_ = false;
}

bool TestMediaClient::IsSupportedAudioType(const media::AudioType& type) {
  return true;
}

bool TestMediaClient::IsSupportedVideoType(const media::VideoType& type) {
  return true;
}

bool TestMediaClient::IsSupportedBitstreamAudioCodec(AudioCodec codec) {
  return false;
}

void TestMediaClient::SetKeySystemsUpdateNeeded() {
  is_update_needed_ = true;
}

void TestMediaClient::DisableExternalKeySystemSupport() {
  supports_external_key_system_ = false;
}

base::Optional<::media::AudioRendererAlgorithmParameters>
TestMediaClient::GetAudioRendererAlgorithmParameters(
    AudioParameters audio_parameters) {
  return base::nullopt;
}

}  // namespace

class KeySystemsTest : public testing::Test {
 protected:
  KeySystemsTest() {
    vp8_codec_.push_back("vp8");

    vp80_codec_.push_back("vp8.0");

    vp9_codec_.push_back("vp9");

    vp90_codec_.push_back("vp9.0");

    vorbis_codec_.push_back("vorbis");

    vp8_and_vorbis_codecs_.push_back("vp8");
    vp8_and_vorbis_codecs_.push_back("vorbis");

    vp9_and_vorbis_codecs_.push_back("vp9");
    vp9_and_vorbis_codecs_.push_back("vorbis");

    foovideo_codec_.push_back("foovideo");

    securefoovideo_codec_.push_back("securefoovideo");

    // KeySystems only do strict codec string comparison. Extended codecs are
    // not supported. Note that in production KeySystemConfigSelector will strip
    // codec extension before calling into KeySystems.
    foovideo_extended_codec_.push_back("foovideo.4D400C");
    foovideo_dot_codec_.push_back("foovideo.");

    fooaudio_codec_.push_back("fooaudio");

    foovideo_and_fooaudio_codecs_.push_back("foovideo");
    foovideo_and_fooaudio_codecs_.push_back("fooaudio");

    unknown_codec_.push_back("unknown");

    mixed_codecs_.push_back("vorbis");
    mixed_codecs_.push_back("foovideo");

    SetMediaClient(&test_media_client_);
  }

  void SetUp() override { AddContainerAndCodecMasksForTest(); }

  ~KeySystemsTest() override {
    // Clear the use of |test_media_client_|, which was set in SetUp().
    // NOTE: This does not clear any cached KeySystemProperties in the global
    // KeySystems instance.
    SetMediaClient(nullptr);
  }

  void UpdateClientKeySystems() {
    test_media_client_.SetKeySystemsUpdateNeeded();
    test_media_client_.DisableExternalKeySystemSupport();
  }

  typedef std::vector<std::string> CodecVector;

  const CodecVector& no_codecs() const { return no_codecs_; }

  const CodecVector& vp8_codec() const { return vp8_codec_; }
  const CodecVector& vp80_codec() const { return vp80_codec_; }
  const CodecVector& vp9_codec() const { return vp9_codec_; }
  const CodecVector& vp90_codec() const { return vp90_codec_; }

  const CodecVector& vorbis_codec() const { return vorbis_codec_; }

  const CodecVector& vp8_and_vorbis_codecs() const {
    return vp8_and_vorbis_codecs_;
  }
  const CodecVector& vp9_and_vorbis_codecs() const {
    return vp9_and_vorbis_codecs_;
  }

  const CodecVector& foovideo_codec() const { return foovideo_codec_; }
  const CodecVector& securefoovideo_codec() const {
    return securefoovideo_codec_;
  }
  const CodecVector& foovideo_extended_codec() const {
    return foovideo_extended_codec_;
  }
  const CodecVector& foovideo_dot_codec() const { return foovideo_dot_codec_; }
  const CodecVector& fooaudio_codec() const { return fooaudio_codec_; }
  const CodecVector& foovideo_and_fooaudio_codecs() const {
    return foovideo_and_fooaudio_codecs_;
  }

  const CodecVector& unknown_codec() const { return unknown_codec_; }

  const CodecVector& mixed_codecs() const { return mixed_codecs_; }

 private:
  const CodecVector no_codecs_;
  CodecVector vp8_codec_;
  CodecVector vp80_codec_;
  CodecVector vp9_codec_;
  CodecVector vp90_codec_;
  CodecVector vorbis_codec_;
  CodecVector vp8_and_vorbis_codecs_;
  CodecVector vp9_and_vorbis_codecs_;

  CodecVector foovideo_codec_;
  CodecVector securefoovideo_codec_;
  CodecVector foovideo_extended_codec_;
  CodecVector foovideo_dot_codec_;
  CodecVector fooaudio_codec_;
  CodecVector foovideo_and_fooaudio_codecs_;

  CodecVector unknown_codec_;

  CodecVector mixed_codecs_;

  TestMediaClient test_media_client_;
};

TEST_F(KeySystemsTest, EmptyKeySystem) {
  EXPECT_FALSE(IsSupportedKeySystem(std::string()));
  EXPECT_EQ("Unknown", GetKeySystemNameForUMA(std::string()));
}

// Clear Key is the only key system registered in content.
TEST_F(KeySystemsTest, ClearKey) {
  EXPECT_TRUE(IsSupportedKeySystem(kClearKey));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(kVideoWebM, no_codecs(),
                                                    kClearKey));

  EXPECT_EQ("ClearKey", GetKeySystemNameForUMA(kClearKey));
}

TEST_F(KeySystemsTest, ClearKeyWithInitDataType) {
  EXPECT_TRUE(IsSupportedKeySystem(kClearKey));
  EXPECT_TRUE(
      IsSupportedKeySystemWithInitDataType(kClearKey, EmeInitDataType::WEBM));
  EXPECT_TRUE(
      IsSupportedKeySystemWithInitDataType(kClearKey, EmeInitDataType::KEYIDS));

  // All other InitDataTypes are not supported.
  EXPECT_FALSE(IsSupportedKeySystemWithInitDataType(kClearKey,
                                                    EmeInitDataType::UNKNOWN));
}

// The key system is not registered and therefore is unrecognized.
TEST_F(KeySystemsTest, Basic_UnrecognizedKeySystem) {
  static const char* const kUnrecognized = "x-org.example.unrecognized";

  EXPECT_FALSE(IsSupportedKeySystem(kUnrecognized));

  EXPECT_EQ("Unknown", GetKeySystemNameForUMA(kUnrecognized));
  EXPECT_FALSE(CanUseAesDecryptor(kUnrecognized));
}

TEST_F(KeySystemsTest, Basic_UsesAesDecryptor) {
  EXPECT_TRUE(IsSupportedKeySystem(kUsesAes));
  EXPECT_TRUE(
      IsSupportedKeySystemWithMediaMimeType(kVideoWebM, no_codecs(), kUsesAes));

  // No UMA value for this test key system.
  EXPECT_EQ("Unknown", GetKeySystemNameForUMA(kUsesAes));

  EXPECT_TRUE(CanUseAesDecryptor(kUsesAes));
}

TEST_F(KeySystemsTest,
       IsSupportedKeySystemWithMediaMimeType_UsesAesDecryptor_TypesContainer1) {
  // Valid video types.
  EXPECT_TRUE(
      IsSupportedKeySystemWithMediaMimeType(kVideoWebM, vp8_codec(), kUsesAes));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(kVideoWebM, vp80_codec(),
                                                    kUsesAes));
  EXPECT_TRUE(
      IsSupportedKeySystemWithMediaMimeType(kVideoWebM, vp9_codec(), kUsesAes));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(kVideoWebM, vp90_codec(),
                                                    kUsesAes));

  // Audio in a video container.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, vp8_and_vorbis_codecs(), kUsesAes));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, vp9_and_vorbis_codecs(), kUsesAes));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(kVideoWebM, vorbis_codec(),
                                                     kUsesAes));

  // Non-Webm codecs.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, foovideo_codec(), kUsesAes));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, unknown_codec(), kUsesAes));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(kVideoWebM, mixed_codecs(),
                                                     kUsesAes));

  // Valid audio types.
  EXPECT_TRUE(
      IsSupportedKeySystemWithAudioMimeType(kAudioWebM, no_codecs(), kUsesAes));
  EXPECT_TRUE(IsSupportedKeySystemWithAudioMimeType(kAudioWebM, vorbis_codec(),
                                                    kUsesAes));

  // Non-audio codecs.
  EXPECT_FALSE(
      IsSupportedKeySystemWithMediaMimeType(kAudioWebM, vp8_codec(), kUsesAes));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kAudioWebM, vp8_and_vorbis_codecs(), kUsesAes));
  EXPECT_FALSE(
      IsSupportedKeySystemWithMediaMimeType(kAudioWebM, vp9_codec(), kUsesAes));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kAudioWebM, vp9_and_vorbis_codecs(), kUsesAes));

  // Non-Webm codec.
  EXPECT_FALSE(IsSupportedKeySystemWithAudioMimeType(
      kAudioWebM, fooaudio_codec(), kUsesAes));
}

TEST_F(KeySystemsTest, IsSupportedKeySystem_InvalidVariants) {
  // Case sensitive.
  EXPECT_FALSE(IsSupportedKeySystem("x-org.example.ClEaR"));

  // TLDs are not allowed.
  EXPECT_FALSE(IsSupportedKeySystem("org."));
  EXPECT_FALSE(IsSupportedKeySystem("com"));

  // Extra period.
  EXPECT_FALSE(IsSupportedKeySystem("x-org.example.clear."));

  // Prefix.
  EXPECT_FALSE(IsSupportedKeySystem("x-org.example."));
  EXPECT_FALSE(IsSupportedKeySystem("x-org.example"));

  // Incomplete.
  EXPECT_FALSE(IsSupportedKeySystem("x-org.example.clea"));

  // Extra character.
  EXPECT_FALSE(IsSupportedKeySystem("x-org.example.clearz"));

  // There are no child key systems for UsesAes.
  EXPECT_FALSE(IsSupportedKeySystem("x-org.example.clear.foo"));
}

TEST_F(KeySystemsTest, IsSupportedKeySystemWithMediaMimeType_NoType) {
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(std::string(), no_codecs(),
                                                     kUsesAes));

  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(std::string(), no_codecs(),
                                                     "x-org.example.foo"));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      std::string(), no_codecs(), "x-org.example.clear.foo"));
}

// Tests the second registered container type.
// TODO(ddorwin): Combined with TypesContainer1 in a future CL.
TEST_F(KeySystemsTest,
       IsSupportedKeySystemWithMediaMimeType_UsesAesDecryptor_TypesContainer2) {
  // Valid video types.
  EXPECT_TRUE(
      IsSupportedKeySystemWithMediaMimeType(kVideoFoo, no_codecs(), kUsesAes));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(kVideoFoo, foovideo_codec(),
                                                    kUsesAes));

  // Audio in a video container.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoFoo, foovideo_and_fooaudio_codecs(), kUsesAes));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoFoo, fooaudio_codec(), kUsesAes));

  // Extended codecs.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoFoo, foovideo_extended_codec(), kUsesAes));

  // Invalid codec format.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoFoo, foovideo_dot_codec(), kUsesAes));

  // Non-container2 codec.
  EXPECT_FALSE(
      IsSupportedKeySystemWithMediaMimeType(kVideoFoo, vp8_codec(), kUsesAes));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(kVideoFoo, unknown_codec(),
                                                     kUsesAes));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(kVideoFoo, mixed_codecs(),
                                                     kUsesAes));

  // Valid audio types.
  EXPECT_TRUE(
      IsSupportedKeySystemWithAudioMimeType(kAudioFoo, no_codecs(), kUsesAes));
  EXPECT_TRUE(IsSupportedKeySystemWithAudioMimeType(kAudioFoo, fooaudio_codec(),
                                                    kUsesAes));

  // Non-audio codecs.
  EXPECT_FALSE(IsSupportedKeySystemWithAudioMimeType(
      kAudioFoo, foovideo_codec(), kUsesAes));
  EXPECT_FALSE(IsSupportedKeySystemWithAudioMimeType(
      kAudioFoo, foovideo_and_fooaudio_codecs(), kUsesAes));

  // Non-container2 codec.
  EXPECT_FALSE(IsSupportedKeySystemWithAudioMimeType(kAudioFoo, vorbis_codec(),
                                                     kUsesAes));
}

TEST_F(KeySystemsTest,
       IsSupportedKeySystem_UsesAesDecryptor_EncryptionSchemes) {
  ExpectEncryptionSchemeConfigRule(kUsesAes, EncryptionScheme::kUnencrypted,
                                   EmeConfigRule::SUPPORTED);
  ExpectEncryptionSchemeConfigRule(kUsesAes, EncryptionScheme::kCenc,
                                   EmeConfigRule::SUPPORTED);
  ExpectEncryptionSchemeConfigRule(kUsesAes, EncryptionScheme::kCbcs,
                                   EmeConfigRule::NOT_SUPPORTED);
}

//
// Non-AesDecryptor-based key system.
//

TEST_F(KeySystemsTest, Basic_ExternalDecryptor) {
  if (!CanRunExternalKeySystemTests())
    return;

  EXPECT_TRUE(IsSupportedKeySystem(kExternal));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(kVideoWebM, no_codecs(),
                                                    kExternal));

  EXPECT_FALSE(CanUseAesDecryptor(kExternal));
}

TEST_F(
    KeySystemsTest,
    IsSupportedKeySystemWithMediaMimeType_ExternalDecryptor_TypesContainer1) {
  if (!CanRunExternalKeySystemTests())
    return;

  // Valid video types.
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(kVideoWebM, no_codecs(),
                                                    kExternal));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(kVideoWebM, vp8_codec(),
                                                    kExternal));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(kVideoWebM, vp80_codec(),
                                                    kExternal));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(kVideoWebM, vp9_codec(),
                                                    kExternal));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(kVideoWebM, vp90_codec(),
                                                    kExternal));

  // Audio in a video container.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, vp8_and_vorbis_codecs(), kExternal));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, vp9_and_vorbis_codecs(), kExternal));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(kVideoWebM, vorbis_codec(),
                                                     kExternal));

  // Non-Webm codecs.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, foovideo_codec(), kExternal));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, unknown_codec(), kExternal));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(kVideoWebM, mixed_codecs(),
                                                     kExternal));

  // Valid audio types.
  EXPECT_TRUE(IsSupportedKeySystemWithAudioMimeType(kAudioWebM, no_codecs(),
                                                    kExternal));
  EXPECT_TRUE(IsSupportedKeySystemWithAudioMimeType(kAudioWebM, vorbis_codec(),
                                                    kExternal));

  // Non-audio codecs.
  EXPECT_FALSE(IsSupportedKeySystemWithAudioMimeType(kAudioWebM, vp8_codec(),
                                                     kExternal));
  EXPECT_FALSE(IsSupportedKeySystemWithAudioMimeType(
      kAudioWebM, vp8_and_vorbis_codecs(), kExternal));
  EXPECT_FALSE(IsSupportedKeySystemWithAudioMimeType(kAudioWebM, vp9_codec(),
                                                     kExternal));
  EXPECT_FALSE(IsSupportedKeySystemWithAudioMimeType(
      kAudioWebM, vp9_and_vorbis_codecs(), kExternal));

  // Non-Webm codec.
  EXPECT_FALSE(IsSupportedKeySystemWithAudioMimeType(
      kAudioWebM, fooaudio_codec(), kExternal));
}

TEST_F(
    KeySystemsTest,
    IsSupportedKeySystemWithMediaMimeType_ExternalDecryptor_TypesContainer2) {
  if (!CanRunExternalKeySystemTests())
    return;

  // Valid video types.
  EXPECT_TRUE(
      IsSupportedKeySystemWithMediaMimeType(kVideoFoo, no_codecs(), kExternal));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(kVideoFoo, foovideo_codec(),
                                                    kExternal));

  // Audio in a video container.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoFoo, foovideo_and_fooaudio_codecs(), kExternal));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoFoo, fooaudio_codec(), kExternal));

  // Extended codecs.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoFoo, foovideo_extended_codec(), kExternal));

  // Invalid codec format.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoFoo, foovideo_dot_codec(), kExternal));

  // Non-container2 codecs.
  EXPECT_FALSE(
      IsSupportedKeySystemWithMediaMimeType(kVideoFoo, vp8_codec(), kExternal));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(kVideoFoo, unknown_codec(),
                                                     kExternal));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(kVideoFoo, mixed_codecs(),
                                                     kExternal));

  // Valid audio types.
  EXPECT_TRUE(
      IsSupportedKeySystemWithAudioMimeType(kAudioFoo, no_codecs(), kExternal));
  EXPECT_TRUE(IsSupportedKeySystemWithAudioMimeType(kAudioFoo, fooaudio_codec(),
                                                    kExternal));

  // Non-audio codecs.
  EXPECT_FALSE(IsSupportedKeySystemWithAudioMimeType(
      kAudioFoo, foovideo_codec(), kExternal));
  EXPECT_FALSE(IsSupportedKeySystemWithAudioMimeType(
      kAudioFoo, foovideo_and_fooaudio_codecs(), kExternal));

  // Non-container2 codec.
  EXPECT_FALSE(IsSupportedKeySystemWithAudioMimeType(kAudioFoo, vorbis_codec(),
                                                     kExternal));
}

TEST_F(KeySystemsTest,
       IsSupportedKeySystem_ExternalDecryptor_EncryptionSchemes) {
  if (!CanRunExternalKeySystemTests())
    return;

  ExpectEncryptionSchemeConfigRule(kExternal, EncryptionScheme::kUnencrypted,
                                   EmeConfigRule::SUPPORTED);
  ExpectEncryptionSchemeConfigRule(kExternal, EncryptionScheme::kCenc,
                                   EmeConfigRule::SUPPORTED);
  ExpectEncryptionSchemeConfigRule(kExternal, EncryptionScheme::kCbcs,
                                   EmeConfigRule::HW_SECURE_CODECS_NOT_ALLOWED);
}

TEST_F(KeySystemsTest, KeySystemNameForUMA) {
  EXPECT_EQ("ClearKey", GetKeySystemNameForUMA(kClearKey));
  EXPECT_EQ("Widevine", GetKeySystemNameForUMA(kWidevineKeySystem));
  EXPECT_EQ("Unknown", GetKeySystemNameForUMA("Foo"));

  // External Clear Key never has a UMA name.
  if (CanRunExternalKeySystemTests())
    EXPECT_EQ("Unknown", GetKeySystemNameForUMA(kExternalClearKey));
}

TEST_F(KeySystemsTest, KeySystemsUpdate) {
  EXPECT_TRUE(IsSupportedKeySystem(kUsesAes));
  EXPECT_TRUE(
      IsSupportedKeySystemWithMediaMimeType(kVideoWebM, no_codecs(), kUsesAes));

  if (CanRunExternalKeySystemTests()) {
    EXPECT_TRUE(IsSupportedKeySystem(kExternal));
    EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(kVideoWebM, no_codecs(),
                                                      kExternal));
  }

  UpdateClientKeySystems();

  EXPECT_TRUE(IsSupportedKeySystem(kUsesAes));
  EXPECT_TRUE(
      IsSupportedKeySystemWithMediaMimeType(kVideoWebM, no_codecs(), kUsesAes));
  if (CanRunExternalKeySystemTests())
    EXPECT_FALSE(IsSupportedKeySystem(kExternal));
}

TEST_F(KeySystemsTest, GetContentTypeConfigRule) {
  if (!CanRunExternalKeySystemTests())
    return;

  EXPECT_EQ(EmeConfigRule::SUPPORTED,
            GetRobustnessConfigRule(kRobustnessSupported));
  EXPECT_EQ(EmeConfigRule::NOT_SUPPORTED,
            GetRobustnessConfigRule(kRobustnessNotSupported));
  EXPECT_EQ(EmeConfigRule::HW_SECURE_CODECS_REQUIRED,
            GetRobustnessConfigRule(kRobustnessSecureCodecsRequired));
}

TEST_F(KeySystemsTest, HardwareSecureCodecs) {
  if (!CanRunExternalKeySystemTests())
    return;

  EXPECT_EQ(EmeConfigRule::HW_SECURE_CODECS_NOT_ALLOWED,
            GetVideoContentTypeConfigRule(kVideoWebM, vp8_codec(), kUsesAes));
  EXPECT_EQ(
      EmeConfigRule::HW_SECURE_CODECS_NOT_ALLOWED,
      GetVideoContentTypeConfigRule(kVideoFoo, foovideo_codec(), kUsesAes));
  EXPECT_EQ(EmeConfigRule::NOT_SUPPORTED,
            GetVideoContentTypeConfigRule(kVideoFoo, securefoovideo_codec(),
                                          kUsesAes));

  EXPECT_EQ(EmeConfigRule::HW_SECURE_CODECS_NOT_ALLOWED,
            GetVideoContentTypeConfigRule(kVideoWebM, vp8_codec(), kExternal));
  EXPECT_EQ(
      EmeConfigRule::SUPPORTED,
      GetVideoContentTypeConfigRule(kVideoFoo, foovideo_codec(), kExternal));

  // Codec that is supported by hardware secure codec but not otherwise is
  // treated as NOT_SUPPORTED instead of HW_SECURE_CODECS_REQUIRED. See
  // KeySystemsImpl::GetContentTypeConfigRule() for details.
  EXPECT_EQ(EmeConfigRule::NOT_SUPPORTED,
            GetVideoContentTypeConfigRule(kVideoFoo, securefoovideo_codec(),
                                          kExternal));
}

}  // namespace media
