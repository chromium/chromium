// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/win/media_foundation_cdm_util.h"

#include <Mferror.h>

#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_com_initializer.h"
#include "media/base/win/mf_helpers.h"
#include "media/base/win/mf_mocks.h"
#include "media/cdm/win/media_foundation_cdm_module.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using Microsoft::WRL::ComPtr;
using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::StrictMock;

namespace media {

namespace {

const char kTestKeySystem[] = "com.example.test.key.system";
const char kTestContentType[] = "video/mp4;codecs=\"avc1.4d401e\"";
const CdmConfig kTestCdmConfig = {
    kTestKeySystem,  // key_system
    true,            // allow_distinctive_identifier
    true,            // allow_persistent_state
    true             // use_hw_secure_codecs
};
const base::UnguessableToken kTestOriginId = base::UnguessableToken::Create();
const base::FilePath kTestStorePath(FILE_PATH_LITERAL("C:\\test\\store\\path"));

}  // namespace

class IsMediaFoundationContentTypeSupportedTest : public testing::Test {
 public:
  IsMediaFoundationContentTypeSupportedTest()
      : mf_type_support_(MakeComPtr<MockMFExtendedDRMTypeSupport>()) {}

  ~IsMediaFoundationContentTypeSupportedTest() override = default;

 protected:
  ComPtr<MockMFExtendedDRMTypeSupport> mf_type_support_;
};

TEST_F(IsMediaFoundationContentTypeSupportedTest, Probably) {
  COM_EXPECT_CALL(mf_type_support_, IsTypeSupportedEx(_, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(MF_MEDIA_ENGINE_CANPLAY_PROBABLY),
                      Return(S_OK)));

  bool result = IsMediaFoundationContentTypeSupported(
      mf_type_support_, kTestKeySystem, kTestContentType);

  EXPECT_TRUE(result);
}

TEST_F(IsMediaFoundationContentTypeSupportedTest, NotSupported) {
  COM_EXPECT_CALL(mf_type_support_, IsTypeSupportedEx(_, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(MF_MEDIA_ENGINE_CANPLAY_NOT_SUPPORTED),
                      Return(S_OK)));

  bool result = IsMediaFoundationContentTypeSupported(
      mf_type_support_, kTestKeySystem, kTestContentType);

  EXPECT_FALSE(result);
}

TEST_F(IsMediaFoundationContentTypeSupportedTest, Maybe) {
  // Set up the mock to return MAYBE for each call,
  // simulating ongoing HDCP negotiation
  COM_EXPECT_CALL(mf_type_support_, IsTypeSupportedEx(_, _, _))
      .Times(5)
      .WillRepeatedly(
          DoAll(SetArgPointee<2>(MF_MEDIA_ENGINE_CANPLAY_MAYBE), Return(S_OK)));

  // The method should retry the maximum number of times and then return false
  bool result = IsMediaFoundationContentTypeSupported(
      mf_type_support_, kTestKeySystem, kTestContentType);

  // After only receiving MAYBE responses, the method should return false
  EXPECT_FALSE(result);
}

TEST_F(IsMediaFoundationContentTypeSupportedTest, MaybeAndThenProbably) {
  // Set up expectations in sequence
  {
    InSequence seq;
    // First call returns MAYBE
    COM_EXPECT_CALL(mf_type_support_, IsTypeSupportedEx(_, _, _))
        .WillOnce(DoAll(SetArgPointee<2>(MF_MEDIA_ENGINE_CANPLAY_MAYBE),
                        Return(S_OK)));

    // Second call returns PROBABLY
    COM_EXPECT_CALL(mf_type_support_, IsTypeSupportedEx(_, _, _))
        .WillOnce(DoAll(SetArgPointee<2>(MF_MEDIA_ENGINE_CANPLAY_PROBABLY),
                        Return(S_OK)));
  }

  bool result = IsMediaFoundationContentTypeSupported(
      mf_type_support_, kTestKeySystem, kTestContentType);

  EXPECT_TRUE(result);
}

TEST_F(IsMediaFoundationContentTypeSupportedTest, Failure) {
  COM_EXPECT_CALL(mf_type_support_, IsTypeSupportedEx(_, _, _))
      .WillOnce(Return(E_FAIL));

  bool result = IsMediaFoundationContentTypeSupported(
      mf_type_support_, kTestKeySystem, kTestContentType);

  EXPECT_FALSE(result);
}

class CreateMediaFoundationCdmTest : public testing::Test {
 public:
  CreateMediaFoundationCdmTest()
      : mf_cdm_factory_(MakeComPtr<MockMFCdmFactory>()),
        mf_cdm_access_(MakeComPtr<MockMFCdmAccess>()),
        mf_cdm_(MakeComPtr<MockMFCdm>()),
        mf_get_service_(MakeComPtr<MockMFGetService>()),
        mf_pmp_host_(MakeComPtr<MockMFPMPHost>()),
        mf_pmp_host_app_(MakeComPtr<MockMFPMPHostApp>()) {}

  ~CreateMediaFoundationCdmTest() override = default;

  void SetUpMockCdmFactoryExpectations(bool support_key_system) {
    COM_EXPECT_CALL(mf_cdm_factory_, IsTypeSupported(_, nullptr))
        .WillOnce(Return(support_key_system ? TRUE : FALSE));
  }

  void SetUpMockCdmAccessExpectations(bool success) {
    HRESULT hr = success ? S_OK : E_FAIL;
    COM_EXPECT_CALL(mf_cdm_factory_, CreateContentDecryptionModuleAccess(
                                         NotNull(), NotNull(), _, _))
        .WillOnce(DoAll(SetComPointee<3>(mf_cdm_access_.Get()), Return(hr)));

    if (success) {
      COM_EXPECT_CALL(mf_cdm_access_, CreateContentDecryptionModule(_, _))
          .WillOnce(DoAll(SetComPointee<1>(mf_cdm_.Get()), Return(S_OK)));
    }
  }

  void SetUpOsCdmExpectations(bool with_pmp_host) {
    MediaFoundationCdmModule::GetInstance()->SetIsOsCdmForTesting(true);
    {
      // Setup expectations to simulate OS CDM
      InSequence seq;

      COM_ON_CALL(mf_cdm_, QueryInterface(IID_IMFGetService, _))
          .WillByDefault(SetComPointeeAndReturnOk<1>(mf_get_service_.Get()));

      if (with_pmp_host) {
        COM_EXPECT_CALL(mf_get_service_, GetService(_, IID_IMFPMPHost, _))
            .WillOnce(
                DoAll(SetComPointee<2>(mf_pmp_host_.Get()), Return(S_OK)));

        COM_EXPECT_CALL(mf_cdm_, SetPMPHostApp(_)).WillOnce(Return(S_OK));
      } else {
        COM_EXPECT_CALL(mf_get_service_, GetService(_, IID_IMFPMPHost, _))
            .WillOnce(Return(E_NOINTERFACE));

        COM_EXPECT_CALL(mf_get_service_, GetService(_, IID_IMFPMPHostApp, _))
            .WillOnce(
                DoAll(SetComPointee<2>(mf_pmp_host_app_.Get()), Return(S_OK)));

        COM_EXPECT_CALL(mf_cdm_, SetPMPHostApp(_)).WillOnce(Return(S_OK));
      }
    }
  }

 protected:
  ComPtr<MockMFCdmFactory> mf_cdm_factory_;
  ComPtr<MockMFCdmAccess> mf_cdm_access_;
  ComPtr<MockMFCdm> mf_cdm_;
  ComPtr<MockMFGetService> mf_get_service_;
  ComPtr<MockMFPMPHost> mf_pmp_host_;
  ComPtr<MockMFPMPHostApp> mf_pmp_host_app_;

  base::win::ScopedCOMInitializer com_initializer_;
};

TEST_F(CreateMediaFoundationCdmTest, Success) {
  SetUpMockCdmFactoryExpectations(true);
  SetUpMockCdmAccessExpectations(true);
  MediaFoundationCdmModule::GetInstance()->SetIsOsCdmForTesting(false);

  ComPtr<IMFContentDecryptionModule> result_cdm;
  HRESULT hr =
      CreateMediaFoundationCdm(mf_cdm_factory_, kTestCdmConfig, kTestOriginId,
                               std::nullopt, kTestStorePath, result_cdm);

  EXPECT_EQ(hr, S_OK);
  EXPECT_EQ(result_cdm.Get(), mf_cdm_.Get());
}

TEST_F(CreateMediaFoundationCdmTest, KeySystemNotSupported) {
  // Set up mock to indicate key system not supported
  SetUpMockCdmFactoryExpectations(false);

  ComPtr<IMFContentDecryptionModule> result_cdm;
  HRESULT hr =
      CreateMediaFoundationCdm(mf_cdm_factory_, kTestCdmConfig, kTestOriginId,
                               std::nullopt, kTestStorePath, result_cdm);

  EXPECT_EQ(hr, (HRESULT)MF_NOT_SUPPORTED_ERR);
  EXPECT_EQ(result_cdm.Get(), nullptr);
}

TEST_F(CreateMediaFoundationCdmTest, CreateAccessFailed) {
  // Set up mock to indicate key system supported but CDM access creation fails
  SetUpMockCdmFactoryExpectations(true);
  COM_EXPECT_CALL(mf_cdm_factory_,
                  CreateContentDecryptionModuleAccess(_, _, _, _))
      .WillOnce(Return(E_FAIL));

  ComPtr<IMFContentDecryptionModule> result_cdm;
  HRESULT hr =
      CreateMediaFoundationCdm(mf_cdm_factory_, kTestCdmConfig, kTestOriginId,
                               std::nullopt, kTestStorePath, result_cdm);

  EXPECT_EQ(hr, E_FAIL);
  EXPECT_EQ(result_cdm.Get(), nullptr);
}

TEST_F(CreateMediaFoundationCdmTest, CreateCdmFailed) {
  // Set up mock for successful key system support and CDM access creation
  SetUpMockCdmFactoryExpectations(true);

  COM_EXPECT_CALL(mf_cdm_factory_,
                  CreateContentDecryptionModuleAccess(_, _, _, _))
      .WillOnce(DoAll(SetComPointee<3>(mf_cdm_access_.Get()), Return(S_OK)));

  COM_EXPECT_CALL(mf_cdm_access_, CreateContentDecryptionModule(_, _))
      .WillOnce(Return(E_FAIL));

  ComPtr<IMFContentDecryptionModule> result_cdm;
  HRESULT hr =
      CreateMediaFoundationCdm(mf_cdm_factory_, kTestCdmConfig, kTestOriginId,
                               std::nullopt, kTestStorePath, result_cdm);

  EXPECT_EQ(hr, E_FAIL);
  EXPECT_EQ(result_cdm.Get(), nullptr);
}

TEST_F(CreateMediaFoundationCdmTest, OsCdmWithPmpHost) {
  // Set up mock for successful CDM creation
  SetUpMockCdmFactoryExpectations(true);
  SetUpMockCdmAccessExpectations(true);

  // Set up OS CDM with PMPHost expectations
  SetUpOsCdmExpectations(true);

  ComPtr<IMFContentDecryptionModule> result_cdm;
  HRESULT hr =
      CreateMediaFoundationCdm(mf_cdm_factory_, kTestCdmConfig, kTestOriginId,
                               std::nullopt, kTestStorePath, result_cdm);

  EXPECT_EQ(hr, S_OK);
  EXPECT_EQ(result_cdm.Get(), mf_cdm_.Get());
}

TEST_F(CreateMediaFoundationCdmTest, OsCdmWithPmpHostApp) {
  // Set up mock for successful CDM creation
  SetUpMockCdmFactoryExpectations(true);
  SetUpMockCdmAccessExpectations(true);

  // Set up OS CDM with PMPHostApp expectations
  SetUpOsCdmExpectations(false);

  ComPtr<IMFContentDecryptionModule> result_cdm;
  HRESULT hr =
      CreateMediaFoundationCdm(mf_cdm_factory_, kTestCdmConfig, kTestOriginId,
                               std::nullopt, kTestStorePath, result_cdm);

  EXPECT_EQ(hr, S_OK);
  EXPECT_EQ(result_cdm.Get(), mf_cdm_.Get());
}

TEST_F(CreateMediaFoundationCdmTest, WithClientToken) {
  // Set up mock for successful CDM creation
  SetUpMockCdmFactoryExpectations(true);
  SetUpMockCdmAccessExpectations(true);
  MediaFoundationCdmModule::GetInstance()->SetIsOsCdmForTesting(false);

  std::vector<uint8_t> client_token = {1, 2, 3, 4, 5};

  ComPtr<IMFContentDecryptionModule> result_cdm;
  HRESULT hr =
      CreateMediaFoundationCdm(mf_cdm_factory_, kTestCdmConfig, kTestOriginId,
                               client_token, kTestStorePath, result_cdm);

  EXPECT_EQ(hr, S_OK);
  EXPECT_EQ(result_cdm.Get(), mf_cdm_.Get());
}

}  // namespace media
