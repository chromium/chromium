// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/key_systems.h"

#include <optional>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/notreached.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "media/base/audio_parameters.h"
#include "media/base/decrypt_config.h"
#include "media/base/eme_constants.h"
#include "media/base/key_systems_impl.h"
#include "media/base/media.h"
#include "media/base/media_client.h"
#include "media/cdm/clear_key_cdm_common.h"
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

const char kAudioWebM[] = "audio/webm";
const char kVideoWebM[] = "video/webm";
const char kAudioFoo[] = "audio/foo";
const char kVideoFoo[] = "video/foo";

const char kRobustnessSupported[] = "supported";
const char kRobustnessSecureCodecsRequired[] = "secure-codecs-required";
const char kRobustnessNotSupported[] = "not-supported";

// Codecs only supported in FOO container. Pick some arbitrary bit fields as
// long as they are not in conflict with the real ones (static_asserted below).
// TODO(crbug.com/40521627): Remove container type (FOO) from codec enums.
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
class TestKeySystemInfoBase : public KeySystemInfo {
 public:
  bool IsSupportedInitDataType(EmeInitDataType init_data_type) const override {
    return init_data_type == EmeInitDataType::WEBM;
  }

  // Note: TEST_CODEC_FOO_SECURE_VIDEO is not supported by default.
  SupportedCodecs GetSupportedCodecs() const override {
    return EME_CODEC_WEBM_ALL | TEST_CODEC_FOO_AUDIO | TEST_CODEC_FOO_VIDEO;
  }

  EmeConfig::Rule GetRobustnessConfigRule(
      const std::string& key_system,
      EmeMediaType media_type,
      const std::string& requested_robustness,
      const bool* /*hw_secure_requirement*/) const override {
    if (requested_robustness.empty()) {
      return EmeConfig::SupportedRule();
    }
    return EmeConfig::UnsupportedRule();
  }
};

class AesKeySystemInfo : public TestKeySystemInfoBase {
 public:
  explicit AesKeySystemInfo(const std::string& name) : name_(name) {}

  std::string GetBaseKeySystemName() const override { return name_; }

  EmeConfig::Rule GetEncryptionSchemeConfigRule(
      EncryptionScheme encryption_scheme) const override {
    if ((encryption_scheme == EncryptionScheme::kUnencrypted ||
         encryption_scheme == EncryptionScheme::kCenc)) {
      return EmeConfig::SupportedRule();
    }
    return EmeConfig::UnsupportedRule();
  }

  EmeConfig::Rule GetPersistentLicenseSessionSupport() const override {
    return EmeConfig::UnsupportedRule();
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

class ExternalKeySystemInfo : public TestKeySystemInfoBase {
 public:
  std::string GetBaseKeySystemName() const override { return kExternal; }

  // Pretend clear (unencrypted) and 'cenc' content are always supported. But
  // 'cbcs' is not supported by hardware secure codecs.
  EmeConfig::Rule GetEncryptionSchemeConfigRule(
      EncryptionScheme encryption_scheme) const override {
    switch (encryption_scheme) {
      case EncryptionScheme::kUnencrypted:
      case EncryptionScheme::kCenc:
        return EmeConfig::SupportedRule();
      case EncryptionScheme::kCbcs:
        return EmeConfig{.hw_secure_codecs = EmeConfigRuleState::kNotAllowed};
    }
    NOTREACHED();
  }

  // We have hardware secure codec support for FOO_VIDEO and FOO_SECURE_VIDEO.
  SupportedCodecs GetSupportedHwSecureCodecs() const override {
    return TEST_CODEC_FOO_VIDEO | TEST_CODEC_FOO_SECURE_VIDEO;
  }

  EmeConfig::Rule GetRobustnessConfigRule(
      const std::string& key_system,
      EmeMediaType media_type,
      const std::string& requested_robustness,
      const bool* /*hw_secure_requirement*/) const override {
    if (requested_robustness == kRobustnessSupported) {
      return EmeConfig::SupportedRule();
    }
    if (requested_robustness == kRobustnessSecureCodecsRequired) {
      return EmeConfig{.hw_secure_codecs = EmeConfigRuleState::kRequired};
    }
    CHECK_EQ(requested_robustness, kRobustnessNotSupported);
    return EmeConfig::UnsupportedRule();
  }

  EmeConfig::Rule GetPersistentLicenseSessionSupport() const override {
    return EmeConfig::UnsupportedRule();
  }

  EmeFeatureSupport GetPersistentStateSupport() const override {
    return EmeFeatureSupport::ALWAYS_ENABLED;
  }

  EmeFeatureSupport GetDistinctiveIdentifierSupport() const override {
    return EmeFeatureSupport::ALWAYS_ENABLED;
  }
};

class TestMediaClient : public MediaClient {
 public:
  TestMediaClient();
  ~TestMediaClient() override;

  // MediaClient implementation.
  bool IsSupportedAudioType(const AudioType& type) final;
  bool IsSupportedVideoType(const VideoType& type) final;
  bool IsSupportedBitstreamAudioCodec(AudioCodec codec) final;
  ExternalMemoryAllocator* GetMediaAllocator() final;

  // Helper function to disable "kExternal" key system support so that we can
  // test the key system update case.
  void DisableExternalKeySystemSupport();

  std::optional<AudioRendererAlgorithmParameters>
  GetAudioRendererAlgorithmParameters(AudioParameters audio_parameters) final;

  std::unique_ptr<KeySystemSupportRegistration> GetSupportedKeySystems(
      GetSupportedKeySystemsCB cb);

 private:
  KeySystemInfos GetSupportedKeySystemsInternal();

  GetSupportedKeySystemsCB get_supported_key_systems_cb_;
  bool supports_external_key_system_ = true;
};

TestMediaClient::TestMediaClient() = default;
TestMediaClient::~TestMediaClient() = default;

std::unique_ptr<::media::KeySystemSupportRegistration>
TestMediaClient::GetSupportedKeySystems(GetSupportedKeySystemsCB cb) {
  // Save the callback for future updates.
  get_supported_key_systems_cb_ = cb;

  get_supported_key_systems_cb_.Run(GetSupportedKeySystemsInternal());
  return nullptr;
}

bool TestMediaClient::IsSupportedAudioType(const AudioType& type) {
  return true;
}

bool TestMediaClient::IsSupportedVideoType(const VideoType& type) {
  return true;
}

bool TestMediaClient::IsSupportedBitstreamAudioCodec(AudioCodec codec) {
  return false;
}

ExternalMemoryAllocator* TestMediaClient::GetMediaAllocator() {
  return nullptr;
}

void TestMediaClient::DisableExternalKeySystemSupport() {
  supports_external_key_system_ = false;
  get_supported_key_systems_cb_.Run(GetSupportedKeySystemsInternal());
}

std::optional<AudioRendererAlgorithmParameters>
TestMediaClient::GetAudioRendererAlgorithmParameters(
    AudioParameters audio_parameters) {
  return std::nullopt;
}

KeySystemInfos TestMediaClient::GetSupportedKeySystemsInternal() {
  KeySystemInfos key_systems;

  key_systems.emplace_back(std::make_unique<AesKeySystemInfo>(kUsesAes));

  if (supports_external_key_system_) {
    key_systems.emplace_back(std::make_unique<ExternalKeySystemInfo>());
  }

  return key_systems;
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

    key_systems_ = std::make_unique<KeySystemsImpl>(base::BindOnce(
        &KeySystemsTest::RegisterKeySystemsSupport, base::Unretained(this)));
  }

  void SetUp() override {
    AddContainerAndCodecMasksForTest();

    base::RunLoop run_loop;
    key_systems_->UpdateIfNeeded(run_loop.QuitClosure());
    run_loop.Run();
  }

  ~KeySystemsTest() override {
    // Clear the use of |test_media_client_|, which was set in SetUp().
    // NOTE: This does not clear any cached KeySystemInfo in the global
    // KeySystems instance.
    SetMediaClient(nullptr);
  }

  void UpdateClientKeySystems() {
    test_media_client_.DisableExternalKeySystemSupport();

    base::RunLoop run_loop;
    key_systems_->UpdateIfNeeded(run_loop.QuitClosure());
    run_loop.Run();
  }

  std::unique_ptr<KeySystemSupportRegistration> RegisterKeySystemsSupport(
      GetSupportedKeySystemsCB cb) {
    return test_media_client_.GetSupportedKeySystems(std::move(cb));
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

  const KeySystems* key_systems() const { return key_systems_.get(); }

  void ExpectEncryptionSchemeConfigRule(const std::string& key_system,
                                        EncryptionScheme encryption_scheme,
                                        EmeConfig::Rule expected_rule) {
    EXPECT_EQ(expected_rule, key_systems()->GetEncryptionSchemeConfigRule(
                                 key_system, encryption_scheme));
  }

  EmeConfig::Rule GetVideoContentTypeConfigRule(
      const std::string& mime_type,
      const std::vector<std::string>& codecs,
      const std::string& key_system) {
    return key_systems()->GetContentTypeConfigRule(
        key_system, EmeMediaType::VIDEO, mime_type, codecs);
  }

  bool IsSupportedKeySystemWithMediaMimeType(
      const std::string& mime_type,
      const std::vector<std::string>& codecs,
      const std::string& key_system) {
    return (GetVideoContentTypeConfigRule(mime_type, codecs, key_system)
                .has_value());
  }

  bool IsSupportedKeySystemWithAudioMimeType(
      const std::string& mime_type,
      const std::vector<std::string>& codecs,
      const std::string& key_system) {
    return (key_systems()
                ->GetContentTypeConfigRule(key_system, EmeMediaType::AUDIO,
                                           mime_type, codecs)
                .has_value());
  }

  bool IsSupportedKeySystem(const std::string& key_system) {
    return key_systems_->IsSupportedKeySystem(key_system);
  }

  EmeConfig::Rule GetRobustnessConfigRule(
      const std::string& requested_robustness) {
    return key_systems()->GetRobustnessConfigRule(
        kExternal, EmeMediaType::VIDEO, requested_robustness, nullptr);
  }

  // Adds test container and codec masks.
  // This function must be called after SetMediaClient() if a MediaClient will
  // be provided. More details: AddXxxMask() will create KeySystems if it hasn't
  // been created. During KeySystems's construction GetMediaClient() will be
  // used to add key systems. In test code, the MediaClient is set by
  // SetMediaClient(). Therefore, SetMediaClient() must be called before this
  // function to make sure MediaClient in effect when constructing KeySystems.
  void AddContainerAndCodecMasksForTest() {
    key_systems_->AddCodecMaskForTesting(EmeMediaType::AUDIO, "fooaudio",
                                         TEST_CODEC_FOO_AUDIO);
    key_systems_->AddCodecMaskForTesting(EmeMediaType::VIDEO, "foovideo",
                                         TEST_CODEC_FOO_VIDEO);
    key_systems_->AddCodecMaskForTesting(EmeMediaType::VIDEO, "securefoovideo",
                                         TEST_CODEC_FOO_SECURE_VIDEO);
    key_systems_->AddMimeTypeCodecMaskForTesting("audio/foo",
                                                 TEST_CODEC_FOO_AUDIO_ALL);
    key_systems_->AddMimeTypeCodecMaskForTesting("video/foo",
                                                 TEST_CODEC_FOO_VIDEO_ALL);
  }

 private:
  base::test::TaskEnvironment task_environment_;

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

  std::unique_ptr<KeySystemsImpl> key_systems_;
  TestMediaClient test_media_client_;
};

TEST_F(KeySystemsTest, EmptyKeySystem) {
  EXPECT_FALSE(IsSupportedKeySystem(std::string()));
  EXPECT_EQ("Unknown", GetKeySystemNameForUMA(std::string()));
}

// Clear Key is the only key system registered in content.
TEST_F(KeySystemsTest, ClearKey) {
  EXPECT_TRUE(IsSupportedKeySystem(kClearKeyKeySystem));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(kVideoWebM, no_codecs(),
                                                    kClearKeyKeySystem));

  EXPECT_EQ("ClearKey", GetKeySystemNameForUMA(kClearKeyKeySystem));
}

TEST_F(KeySystemsTest, ClearKeyWithInitDataType) {
  EXPECT_TRUE(key_systems()->IsSupportedKeySystem(kClearKeyKeySystem));
  EXPECT_TRUE(key_systems()->IsSupportedInitDataType(kClearKeyKeySystem,
                                                     EmeInitDataType::WEBM));
  EXPECT_TRUE(key_systems()->IsSupportedInitDataType(kClearKeyKeySystem,
                                                     EmeInitDataType::KEYIDS));

  // All other InitDataTypes are not supported.
  EXPECT_FALSE(key_systems()->IsSupportedInitDataType(
      kClearKeyKeySystem, EmeInitDataType::UNKNOWN));
}

// The key system is not registered and therefore is unrecognized.
TEST_F(KeySystemsTest, Basic_UnrecognizedKeySystem) {
  static const char* const kUnrecognized = "x-org.example.unrecognized";

  EXPECT_FALSE(IsSupportedKeySystem(kUnrecognized));

  EXPECT_EQ("Unknown", GetKeySystemNameForUMA(kUnrecognized));
  EXPECT_FALSE(key_systems()->CanUseAesDecryptor(kUnrecognized));
}

TEST_F(KeySystemsTest, Basic_UsesAesDecryptor) {
  EXPECT_TRUE(IsSupportedKeySystem(kUsesAes));
  EXPECT_TRUE(
      IsSupportedKeySystemWithMediaMimeType(kVideoWebM, no_codecs(), kUsesAes));

  // No UMA value for this test key system.
  EXPECT_EQ("Unknown", GetKeySystemNameForUMA(kUsesAes));

  EXPECT_TRUE(key_systems()->CanUseAesDecryptor(kUsesAes));
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
  auto supported = EmeConfig::SupportedRule();
  auto not_supported = EmeConfig::UnsupportedRule();
  ExpectEncryptionSchemeConfigRule(kUsesAes, EncryptionScheme::kUnencrypted,
                                   supported);
  ExpectEncryptionSchemeConfigRule(kUsesAes, EncryptionScheme::kCenc,
                                   supported);
  ExpectEncryptionSchemeConfigRule(kUsesAes, EncryptionScheme::kCbcs,
                                   not_supported);
}

//
// Non-AesDecryptor-based key system.
//

TEST_F(KeySystemsTest, Basic_ExternalDecryptor) {
  EXPECT_TRUE(IsSupportedKeySystem(kExternal));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(kVideoWebM, no_codecs(),
                                                    kExternal));

  EXPECT_FALSE(key_systems()->CanUseAesDecryptor(kExternal));
}

TEST_F(
    KeySystemsTest,
    IsSupportedKeySystemWithMediaMimeType_ExternalDecryptor_TypesContainer1) {
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
  auto supported = EmeConfig::SupportedRule();
  auto hw_secure_codecs_not_allowed =
      EmeConfig{.hw_secure_codecs = EmeConfigRuleState::kNotAllowed};
  ExpectEncryptionSchemeConfigRule(kExternal, EncryptionScheme::kUnencrypted,
                                   supported);
  ExpectEncryptionSchemeConfigRule(kExternal, EncryptionScheme::kCenc,
                                   supported);
  ExpectEncryptionSchemeConfigRule(kExternal, EncryptionScheme::kCbcs,
                                   hw_secure_codecs_not_allowed);
}

TEST_F(KeySystemsTest, KeySystemNameForUMA) {
  EXPECT_EQ("ClearKey", GetKeySystemNameForUMA(kClearKeyKeySystem));
  EXPECT_EQ("ClearKey", GetKeySystemNameForUMA(kClearKeyKeySystem, false));
  EXPECT_EQ("ClearKey", GetKeySystemNameForUMA(kClearKeyKeySystem, true));
  EXPECT_EQ("Widevine", GetKeySystemNameForUMA(kWidevineKeySystem));
  EXPECT_EQ("Widevine.SoftwareSecure",
            GetKeySystemNameForUMA(kWidevineKeySystem, false));
  EXPECT_EQ("Widevine.HardwareSecure",
            GetKeySystemNameForUMA(kWidevineKeySystem, true));
  EXPECT_EQ("Unknown", GetKeySystemNameForUMA("Foo"));
  EXPECT_EQ("Unknown", GetKeySystemNameForUMA("Foo", false));
  EXPECT_EQ("Unknown", GetKeySystemNameForUMA("Foo", true));

  // External Clear Key never has a UMA name.
  EXPECT_EQ("Unknown", GetKeySystemNameForUMA(kExternalClearKeyKeySystem));
  EXPECT_EQ("Unknown",
            GetKeySystemNameForUMA(kExternalClearKeyKeySystem, false));
  EXPECT_EQ("Unknown",
            GetKeySystemNameForUMA(kExternalClearKeyKeySystem, true));
}

TEST_F(KeySystemsTest, KeySystemsUpdate) {
  EXPECT_TRUE(IsSupportedKeySystem(kUsesAes));
  EXPECT_TRUE(
      IsSupportedKeySystemWithMediaMimeType(kVideoWebM, no_codecs(), kUsesAes));

  EXPECT_TRUE(IsSupportedKeySystem(kExternal));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(kVideoWebM, no_codecs(),
                                                    kExternal));

  UpdateClientKeySystems();

  EXPECT_TRUE(IsSupportedKeySystem(kUsesAes));
  EXPECT_TRUE(
      IsSupportedKeySystemWithMediaMimeType(kVideoWebM, no_codecs(), kUsesAes));
  EXPECT_FALSE(IsSupportedKeySystem(kExternal));
}

TEST_F(KeySystemsTest, GetContentTypeConfigRule) {
  auto supported = EmeConfig::SupportedRule();
  auto not_supported = EmeConfig::UnsupportedRule();
  auto hw_secure_codecs_required =
      EmeConfig{.hw_secure_codecs = EmeConfigRuleState::kRequired};
  EXPECT_EQ(supported, GetRobustnessConfigRule(kRobustnessSupported));
  EXPECT_EQ(not_supported, GetRobustnessConfigRule(kRobustnessNotSupported));
  EXPECT_TRUE(hw_secure_codecs_required ==
              GetRobustnessConfigRule(kRobustnessSecureCodecsRequired));
}

TEST_F(KeySystemsTest, HardwareSecureCodecs) {
  auto supported = EmeConfig::SupportedRule();
  auto not_supported = EmeConfig::UnsupportedRule();
  auto hw_secure_codecs_required =
      EmeConfig{.hw_secure_codecs = EmeConfigRuleState::kRequired};
  auto hw_secure_codecs_not_allowed =
      EmeConfig{.hw_secure_codecs = EmeConfigRuleState::kNotAllowed};

  EXPECT_EQ(hw_secure_codecs_not_allowed,
            GetVideoContentTypeConfigRule(kVideoWebM, vp8_codec(), kUsesAes));
  EXPECT_EQ(
      hw_secure_codecs_not_allowed,
      GetVideoContentTypeConfigRule(kVideoFoo, foovideo_codec(), kUsesAes));
  EXPECT_EQ(not_supported, GetVideoContentTypeConfigRule(
                               kVideoFoo, securefoovideo_codec(), kUsesAes));

  EXPECT_EQ(hw_secure_codecs_not_allowed,
            GetVideoContentTypeConfigRule(kVideoWebM, vp8_codec(), kExternal));
  EXPECT_EQ(supported, GetVideoContentTypeConfigRule(
                           kVideoFoo, foovideo_codec(), kExternal));

  EXPECT_EQ(hw_secure_codecs_required,
            GetVideoContentTypeConfigRule(kVideoFoo, securefoovideo_codec(),
                                          kExternal));
}

}  // namespace media
