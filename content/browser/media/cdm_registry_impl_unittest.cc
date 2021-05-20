// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/cdm_registry_impl.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/base_paths.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/token.h"
#include "base/version.h"
#include "content/public/common/cdm_info.h"
#include "media/base/video_codecs.h"
#include "media/cdm/cdm_capability.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

using AudioCodec = media::AudioCodec;
using VideoCodec = media::VideoCodec;
using EncryptionScheme = media::EncryptionScheme;
using CdmSessionType = media::CdmSessionType;

const char kTestCdmName[] = "Test CDM";
const char kAlternateCdmName[] = "Alternate CDM";
const base::Token kTestCdmGuid{1234, 5678};
const char kTestPath[] = "/aa/bb";
const char kVersion1[] = "1.1.1.1";
const char kVersion2[] = "1.1.1.2";
const char kTestKeySystem[] = "com.example.somesystem";
const char kTestFileSystemId[] = "file_system_id";

// Helper function to compare a STL container to an initializer_list.
template <typename Container, typename T>
bool StlEquals(const Container a, std::initializer_list<T> b) {
  return a == Container(b);
}

#define EXPECT_STL_EQ(container, ...)                            \
  do {                                                           \
    EXPECT_THAT(container, ::testing::ElementsAre(__VA_ARGS__)); \
  } while (false)

#define EXPECT_AUDIO_CODECS(...) \
  EXPECT_STL_EQ(cdm.capability->audio_codecs, __VA_ARGS__)

#define EXPECT_VIDEO_CODECS(...) \
  EXPECT_STL_EQ(cdm.capability->video_codecs, __VA_ARGS__)

#define EXPECT_ENCRYPTION_SCHEMES(...) \
  EXPECT_STL_EQ(cdm.capability->encryption_schemes, __VA_ARGS__)

#define EXPECT_SESSION_TYPES(...) \
  EXPECT_STL_EQ(cdm.capability->session_types, __VA_ARGS__)

}  // namespace

// For simplicity and to make failures easier to diagnose, this test uses
// std::string instead of base::FilePath and std::vector<std::string>.
class CdmRegistryImplTest : public testing::Test {
 public:
  CdmRegistryImplTest() {}
  ~CdmRegistryImplTest() override {}

 protected:
  media::CdmCapability GetTestCdmCapability() {
    return media::CdmCapability(
        {media::kCodecVorbis}, {media::kCodecVP8, media::kCodecVP9},
        {EncryptionScheme::kCenc},
        {CdmSessionType::kTemporary, CdmSessionType::kPersistentLicense});
  }

  CdmInfo GetTestCdmInfo() {
    return CdmInfo(kTestKeySystem, CdmInfo::Robustness::kSoftwareSecure,
                   GetTestCdmCapability(),
                   /*supports_sub_key_systems=*/true, kTestCdmName,
                   kTestCdmGuid, base::Version(kVersion1),
                   base::FilePath::FromUTF8Unsafe(kTestPath),
                   kTestFileSystemId);
  }

  void Register(CdmInfo cdm_info) {
    cdm_registry_.RegisterCdm(std::move(cdm_info));
  }

  void RegisterForLazyInitialization() {
    // Register a CdmInfo without CdmCapability to allow lazy initialization.
    Register(CdmInfo(kTestKeySystem, CdmInfo::Robustness::kSoftwareSecure,
                     absl::nullopt));
    auto cdm_info = cdm_registry_.GetCdmInfo(
        kTestKeySystem, CdmInfo::Robustness::kSoftwareSecure);
    ASSERT_TRUE(cdm_info);
    ASSERT_FALSE(cdm_info->capability);
  }

  bool IsRegistered(const std::string& name, const std::string& version) {
    for (const auto& cdm : cdm_registry_.GetAllRegisteredCdmsForTesting()) {
      if (cdm.name == name && cdm.version.GetString() == version)
        return true;
    }
    return false;
  }

  std::vector<std::string> GetVersions(const base::Token& guid) {
    std::vector<std::string> versions;
    for (const auto& cdm : cdm_registry_.GetAllRegisteredCdmsForTesting()) {
      if (cdm.guid == guid)
        versions.push_back(cdm.version.GetString());
    }
    return versions;
  }

 protected:
  CdmRegistryImpl cdm_registry_;
};

TEST_F(CdmRegistryImplTest, Register) {
  Register(GetTestCdmInfo());

  auto cdms = cdm_registry_.GetAllRegisteredCdmsForTesting();
  ASSERT_EQ(1u, cdms.size());
  CdmInfo cdm = cdms[0];
  EXPECT_EQ(kTestCdmName, cdm.name);
  EXPECT_EQ(kVersion1, cdm.version.GetString());
  EXPECT_EQ(kTestPath, cdm.path.MaybeAsASCII());
  EXPECT_EQ(kTestFileSystemId, cdm.file_system_id);
  EXPECT_AUDIO_CODECS(AudioCodec::kCodecVorbis);
  EXPECT_VIDEO_CODECS(VideoCodec::kCodecVP8, VideoCodec::kCodecVP9);
  EXPECT_ENCRYPTION_SCHEMES(EncryptionScheme::kCenc);
  EXPECT_SESSION_TYPES(CdmSessionType::kTemporary,
                       CdmSessionType::kPersistentLicense);
  EXPECT_EQ(kTestKeySystem, cdm.key_system);
  EXPECT_TRUE(cdm.supports_sub_key_systems);
  EXPECT_EQ(cdm.robustness, CdmInfo::Robustness::kSoftwareSecure);
}

TEST_F(CdmRegistryImplTest, ReRegister) {
  auto cdm_info = GetTestCdmInfo();
  Register(cdm_info);
  EXPECT_TRUE(IsRegistered(kTestCdmName, kVersion1));

  // Now register same key system with different values.
  cdm_info.supports_sub_key_systems = false;
  Register(cdm_info);

  EXPECT_TRUE(IsRegistered(kTestCdmName, kVersion1));
}

TEST_F(CdmRegistryImplTest, MultipleVersions) {
  auto cdm_info = GetTestCdmInfo();
  Register(cdm_info);
  cdm_info.version = base::Version(kVersion2);
  Register(cdm_info);

  EXPECT_TRUE(IsRegistered(kTestCdmName, kVersion1));
  EXPECT_TRUE(IsRegistered(kTestCdmName, kVersion2));
}

TEST_F(CdmRegistryImplTest, NewVersionInsertedLast) {
  auto cdm_info = GetTestCdmInfo();
  Register(cdm_info);
  cdm_info.version = base::Version(kVersion2);
  Register(cdm_info);

  const std::vector<std::string> versions = GetVersions(kTestCdmGuid);
  EXPECT_EQ(2u, versions.size());
  EXPECT_EQ(kVersion1, versions[0]);
  EXPECT_EQ(kVersion2, versions[1]);
}

TEST_F(CdmRegistryImplTest, DifferentNames) {
  auto cdm_info = GetTestCdmInfo();
  Register(cdm_info);
  cdm_info.name = kAlternateCdmName;
  Register(cdm_info);

  EXPECT_TRUE(IsRegistered(kTestCdmName, kVersion1));
  EXPECT_TRUE(IsRegistered(kAlternateCdmName, kVersion1));
}

TEST_F(CdmRegistryImplTest, SupportedEncryptionSchemes) {
  auto cdm_info = GetTestCdmInfo();
  cdm_info.capability->encryption_schemes = {EncryptionScheme::kCenc,
                                             EncryptionScheme::kCbcs};
  Register(cdm_info);

  std::vector<CdmInfo> cdms = cdm_registry_.GetAllRegisteredCdmsForTesting();
  ASSERT_EQ(1u, cdms.size());
  const CdmInfo& cdm = cdms[0];
  EXPECT_ENCRYPTION_SCHEMES(EncryptionScheme::kCenc, EncryptionScheme::kCbcs);
}

TEST_F(CdmRegistryImplTest, GetCdmInfo_Success) {
  Register(GetTestCdmInfo());
  auto cdm_info = cdm_registry_.GetCdmInfo(
      kTestKeySystem, CdmInfo::Robustness::kSoftwareSecure);
  ASSERT_TRUE(cdm_info);

  const CdmInfo& cdm = *cdm_info;

  EXPECT_EQ(kTestCdmName, cdm.name);
  EXPECT_EQ(kVersion1, cdm.version.GetString());
  EXPECT_EQ(kTestPath, cdm.path.MaybeAsASCII());
  EXPECT_EQ(kTestFileSystemId, cdm.file_system_id);
  EXPECT_VIDEO_CODECS(VideoCodec::kCodecVP8, VideoCodec::kCodecVP9);
  EXPECT_ENCRYPTION_SCHEMES(EncryptionScheme::kCenc);
  EXPECT_SESSION_TYPES(CdmSessionType::kTemporary,
                       CdmSessionType::kPersistentLicense);
  EXPECT_EQ(kTestKeySystem, cdm.key_system);
  EXPECT_TRUE(cdm.supports_sub_key_systems);
  EXPECT_EQ(cdm.robustness, CdmInfo::Robustness::kSoftwareSecure);
}

TEST_F(CdmRegistryImplTest, GetCdmInfo_Fail) {
  Register(GetTestCdmInfo());
  auto cdm_info = cdm_registry_.GetCdmInfo(
      kTestKeySystem, CdmInfo::Robustness::kHardwareSecure);
  ASSERT_FALSE(cdm_info);
}

TEST_F(CdmRegistryImplTest, FinalizeCdmCapability_Success) {
  RegisterForLazyInitialization();
  EXPECT_TRUE(cdm_registry_.FinalizeCdmCapability(
      kTestKeySystem, CdmInfo::Robustness::kSoftwareSecure,
      GetTestCdmCapability()));
  auto cdm_info = cdm_registry_.GetCdmInfo(
      kTestKeySystem, CdmInfo::Robustness::kSoftwareSecure);
  EXPECT_TRUE(cdm_info && cdm_info->capability);
}

TEST_F(CdmRegistryImplTest, FinalizeCdmCapability_Unregistered) {
  RegisterForLazyInitialization();
  // Trying to finalize for `kHardwareSecure` which was not registered.
  EXPECT_FALSE(cdm_registry_.FinalizeCdmCapability(
      kTestKeySystem, CdmInfo::Robustness::kHardwareSecure,
      GetTestCdmCapability()));
  EXPECT_TRUE(cdm_registry_.GetCdmInfo(kTestKeySystem,
                                       CdmInfo::Robustness::kSoftwareSecure));
}

TEST_F(CdmRegistryImplTest, FinalizeCdmCapability_AlreadyFinalized) {
  Register(GetTestCdmInfo());
  EXPECT_FALSE(cdm_registry_.FinalizeCdmCapability(
      kTestKeySystem, CdmInfo::Robustness::kSoftwareSecure,
      GetTestCdmCapability()));
  auto cdm_info = cdm_registry_.GetCdmInfo(
      kTestKeySystem, CdmInfo::Robustness::kSoftwareSecure);
  EXPECT_TRUE(cdm_info && cdm_info->capability);
}

TEST_F(CdmRegistryImplTest, FinalizeCdmCapability_RemoveCdmInfo) {
  RegisterForLazyInitialization();
  EXPECT_FALSE(cdm_registry_.FinalizeCdmCapability(
      kTestKeySystem, CdmInfo::Robustness::kSoftwareSecure, absl::nullopt));
  EXPECT_FALSE(cdm_registry_.GetCdmInfo(kTestKeySystem,
                                        CdmInfo::Robustness::kSoftwareSecure));
}

}  // namespace content
