// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/win/media_foundation_cdm.h"

#include <wchar.h>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "media/base/mock_filters.h"
#include "media/base/test_helpers.h"
#include "media/base/win/hresults.h"
#include "media/base/win/media_foundation_cdm_proxy.h"
#include "media/base/win/mf_helpers.h"
#include "media/base/win/mf_mocks.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::IsEmpty;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::SetArgReferee;
using ::testing::StartsWith;
using ::testing::StrEq;
using ::testing::StrictMock;
using ::testing::WithoutArgs;

namespace media {

namespace {

const char kSessionId[] = "session_id";
const double kExpirationMs = 123456789.0;
const auto kExpirationTime =
    base::Time::FromMillisecondsSinceUnixEpoch(kExpirationMs);
const char kTestUmaPrefix[] = "Media.EME.TestUmaPrefix.";

std::vector<uint8_t> StringToVector(const std::string& str) {
  return std::vector<uint8_t>(str.begin(), str.end());
}

// testing::InvokeArgument<N> does not work with base::OnceCallback. Use this
// gmock action template to invoke base::OnceCallback. `k` is the k-th argument
// and `T` is the callback's type.
ACTION_TEMPLATE(InvokeCallbackArgument,
                HAS_2_TEMPLATE_PARAMS(int, k, typename, T),
                AND_1_VALUE_PARAMS(p0)) {
  std::move(const_cast<T&>(std::get<k>(args))).Run(p0);
}

}  // namespace

using Microsoft::WRL::ComPtr;

class MediaFoundationCdmTest : public testing::Test {
 public:
  MediaFoundationCdmTest()
      : mf_cdm_(MakeComPtr<StrictMock<MockMFCdm>>()),
        mf_cdm_session_(MakeComPtr<StrictMock<MockMFCdmSession>>()),
        cdm_(base::MakeRefCounted<MediaFoundationCdm>(
            kTestUmaPrefix,
            base::BindRepeating(&MediaFoundationCdmTest::CreateMFCdm,
                                base::Unretained(this)),
            is_type_supported_cb_.Get(),
            store_client_token_cb_.Get(),
            cdm_event_cb_.Get(),
            base::BindRepeating(&MockCdmClient::OnSessionMessage,
                                base::Unretained(&cdm_client_)),
            base::BindRepeating(&MockCdmClient::OnSessionClosed,
                                base::Unretained(&cdm_client_)),
            base::BindRepeating(&MockCdmClient::OnSessionKeysChange,
                                base::Unretained(&cdm_client_)),
            base::BindRepeating(&MockCdmClient::OnSessionExpirationUpdate,
                                base::Unretained(&cdm_client_)))) {}

  ~MediaFoundationCdmTest() override = default;

  void CreateMFCdm(HRESULT& hresult,
                   Microsoft::WRL::ComPtr<IMFContentDecryptionModule>& mf_cdm) {
    if (can_initialize_) {
      hresult = S_OK;
      mf_cdm = mf_cdm_;
    } else {
      hresult = E_FAIL;
      mf_cdm.Reset();
    }
  }

  void Initialize() { ASSERT_SUCCESS(cdm_->Initialize()); }

  void InitializeAndExpectFailure() {
    can_initialize_ = false;
    EXPECT_CALL(cdm_event_cb_, Run(CdmEvent::kCdmError, E_FAIL));
    ASSERT_FAILED(cdm_->Initialize());
  }

  void SetGenerateRequestExpectations(
      ComPtr<MockMFCdmSession> mf_cdm_session,
      const char* session_id,
      IMFContentDecryptionModuleSessionCallbacks** mf_cdm_session_callbacks,
      bool expect_message = true) {
    std::vector<uint8_t> license_request = StringToVector("request");

    // Session ID to return. Will be released by |mf_cdm_session_|.
    std::wstring wide_session_id = base::UTF8ToWide(session_id);
    LPWSTR mf_session_id = nullptr;
    ASSERT_SUCCESS(
        CopyCoTaskMemWideString(wide_session_id.data(), &mf_session_id));

    COM_EXPECT_CALL(mf_cdm_session,
                    GenerateRequest(StrEq(L"webm"), NotNull(), _))
        .WillOnce(WithoutArgs([=] {  // Capture local variables by value.
          (*mf_cdm_session_callbacks)
              ->KeyMessage(MF_MEDIAKEYSESSION_MESSAGETYPE_LICENSE_REQUEST,
                           license_request.data(), license_request.size(),
                           nullptr);
          return S_OK;
        }));

    COM_EXPECT_CALL(mf_cdm_session, GetSessionId(_))
        .WillOnce(DoAll(SetArgPointee<0>(mf_session_id), Return(S_OK)));

    if (expect_message) {
      EXPECT_CALL(cdm_client_,
                  OnSessionMessage(session_id, CdmMessageType::LICENSE_REQUEST,
                                   license_request));
    }
  }

  void CreateSessionAndGenerateRequest() {
    std::vector<uint8_t> init_data = StringToVector("init_data");

    COM_EXPECT_CALL(mf_cdm_,
                    CreateSession(MF_MEDIAKEYSESSION_TYPE_TEMPORARY, _, _))
        .WillOnce(DoAll(SaveComPtr<1>(&mf_cdm_session_callbacks_),
                        SetComPointee<2>(mf_cdm_session_.Get()), Return(S_OK)));

    SetGenerateRequestExpectations(mf_cdm_session_, kSessionId,
                                   &mf_cdm_session_callbacks_);

    cdm_->CreateSessionAndGenerateRequest(
        CdmSessionType::kTemporary, EmeInitDataType::WEBM, init_data,
        std::make_unique<MockCdmSessionPromise>(/*expect_success=*/true,
                                                &session_id_));

    task_environment_.RunUntilIdle();
    EXPECT_EQ(session_id_, kSessionId);
  }

 protected:
  base::test::TaskEnvironment task_environment_;

  StrictMock<MockCdmClient> cdm_client_;
  StrictMock<base::MockCallback<MediaFoundationCdm::IsTypeSupportedCB>>
      is_type_supported_cb_;
  base::MockCallback<MediaFoundationCdm::StoreClientTokenCB>
      store_client_token_cb_;
  StrictMock<base::MockCallback<MediaFoundationCdm::CdmEventCB>> cdm_event_cb_;
  ComPtr<StrictMock<MockMFCdm>> mf_cdm_;
  ComPtr<StrictMock<MockMFCdmSession>> mf_cdm_session_;
  ComPtr<IMFContentDecryptionModuleSessionCallbacks> mf_cdm_session_callbacks_;
  scoped_refptr<MediaFoundationCdm> cdm_;
  bool can_initialize_ = true;
  std::string session_id_;
  scoped_refptr<MediaFoundationCdmProxy> mf_cdm_proxy_;
};

TEST_F(MediaFoundationCdmTest, SetServerCertificate) {
  Initialize();

  std::vector<uint8_t> certificate = StringToVector("certificate");
  COM_EXPECT_CALL(mf_cdm_,
                  SetServerCertificate(certificate.data(), certificate.size()))
      .WillOnce(Return(S_OK));

  cdm_->SetServerCertificate(
      certificate, std::make_unique<MockCdmPromise>(/*expect_success=*/true));
}

TEST_F(MediaFoundationCdmTest, SetServerCertificate_Failure) {
  Initialize();

  std::vector<uint8_t> certificate = StringToVector("certificate");
  COM_EXPECT_CALL(mf_cdm_,
                  SetServerCertificate(certificate.data(), certificate.size()))
      .WillOnce(Return(E_FAIL));

  cdm_->SetServerCertificate(
      certificate, std::make_unique<MockCdmPromise>(/*expect_success=*/false));
}

// HardwareContextReset during SetServerCertificate() will cause the the promise
// rejected.
TEST_F(MediaFoundationCdmTest, SetServerCertificate_HardwareContextReset) {
  Initialize();

  std::vector<uint8_t> certificate = StringToVector("certificate");
  COM_EXPECT_CALL(mf_cdm_,
                  SetServerCertificate(certificate.data(), certificate.size()))
      .WillOnce(Return(DRM_E_TEE_INVALID_HWDRM_STATE));

  cdm_->SetServerCertificate(
      certificate, std::make_unique<MockCdmPromise>(/*expect_success=*/false));
}

TEST_F(MediaFoundationCdmTest, GetStatusForPolicy_HdcpNone_KeyStatusUsable) {
  Initialize();
  CdmKeyInformation::KeyStatus key_status;
  cdm_->GetStatusForPolicy(HdcpVersion::kHdcpVersionNone,
                           std::make_unique<MockCdmKeyStatusPromise>(
                               /*expect_success=*/true, &key_status));
  EXPECT_EQ(CdmKeyInformation::KeyStatus::USABLE, key_status);
}

TEST_F(MediaFoundationCdmTest, GetStatusForPolicy_HdcpV1_1_KeyStatusUsable) {
  Initialize();
  EXPECT_CALL(is_type_supported_cb_,
              Run("video/mp4;codecs=\"avc1\";features=\"hdcp=1\"", _))
      .WillOnce(
          InvokeCallbackArgument<1,
                                 MediaFoundationCdm::IsTypeSupportedResultCB>(
              /*is_supported=*/true));

  CdmKeyInformation::KeyStatus key_status;
  cdm_->GetStatusForPolicy(HdcpVersion::kHdcpVersion1_1,
                           std::make_unique<MockCdmKeyStatusPromise>(
                               /*expect_success=*/true, &key_status));
  EXPECT_EQ(CdmKeyInformation::KeyStatus::USABLE, key_status);
}

TEST_F(MediaFoundationCdmTest,
       GetStatusForPolicy_HdcpV2_3_KeyStatusOutputRestricted) {
  Initialize();
  EXPECT_CALL(is_type_supported_cb_,
              Run("video/mp4;codecs=\"avc1\";features=\"hdcp=2\"", _))
      .WillOnce(
          InvokeCallbackArgument<1,
                                 MediaFoundationCdm::IsTypeSupportedResultCB>(
              /*is_supported=*/false));

  CdmKeyInformation::KeyStatus key_status;
  cdm_->GetStatusForPolicy(HdcpVersion::kHdcpVersion2_3,
                           std::make_unique<MockCdmKeyStatusPromise>(
                               /*expect_success=*/true, &key_status));
  EXPECT_EQ(CdmKeyInformation::KeyStatus::OUTPUT_RESTRICTED, key_status);
}

TEST_F(MediaFoundationCdmTest, CreateSessionAndGenerateRequest) {
  Initialize();
  CreateSessionAndGenerateRequest();
}

// Tests the case where two sessions are being created in parallel.
TEST_F(MediaFoundationCdmTest, CreateSessionAndGenerateRequest_Parallel) {
  Initialize();

  std::vector<uint8_t> init_data = StringToVector("init_data");
  const char kSessionId1[] = "session_id_1";
  const char kSessionId2[] = "session_id_2";

  auto mf_cdm_session_1 = MakeComPtr<MockMFCdmSession>();
  auto mf_cdm_session_2 = MakeComPtr<MockMFCdmSession>();
  ComPtr<IMFContentDecryptionModuleSessionCallbacks> mf_cdm_session_callbacks_1;
  ComPtr<IMFContentDecryptionModuleSessionCallbacks> mf_cdm_session_callbacks_2;

  COM_EXPECT_CALL(mf_cdm_,
                  CreateSession(MF_MEDIAKEYSESSION_TYPE_TEMPORARY, _, _))
      .WillOnce(DoAll(SaveComPtr<1>(&mf_cdm_session_callbacks_1),
                      SetComPointee<2>(mf_cdm_session_1.Get()), Return(S_OK)))
      .WillOnce(DoAll(SaveComPtr<1>(&mf_cdm_session_callbacks_2),
                      SetComPointee<2>(mf_cdm_session_2.Get()), Return(S_OK)));

  SetGenerateRequestExpectations(mf_cdm_session_1, kSessionId1,
                                 &mf_cdm_session_callbacks_1);
  SetGenerateRequestExpectations(mf_cdm_session_2, kSessionId2,
                                 &mf_cdm_session_callbacks_2);

  std::string session_id_1;
  std::string session_id_2;
  cdm_->CreateSessionAndGenerateRequest(
      CdmSessionType::kTemporary, EmeInitDataType::WEBM, init_data,
      std::make_unique<MockCdmSessionPromise>(/*expect_success=*/true,
                                              &session_id_1));
  cdm_->CreateSessionAndGenerateRequest(
      CdmSessionType::kTemporary, EmeInitDataType::WEBM, init_data,
      std::make_unique<MockCdmSessionPromise>(/*expect_success=*/true,
                                              &session_id_2));

  task_environment_.RunUntilIdle();
  EXPECT_EQ(session_id_1, kSessionId1);
  EXPECT_EQ(session_id_2, kSessionId2);
}

TEST_F(MediaFoundationCdmTest,
       CreateSessionAndGenerateRequest_AfterInitializeFailure) {
  InitializeAndExpectFailure();

  std::vector<uint8_t> init_data = StringToVector("init_data");
  cdm_->CreateSessionAndGenerateRequest(
      CdmSessionType::kTemporary, EmeInitDataType::WEBM, init_data,
      std::make_unique<MockCdmSessionPromise>(/*expect_success=*/false,
                                              &session_id_));

  task_environment_.RunUntilIdle();
  EXPECT_TRUE(session_id_.empty());
}

TEST_F(MediaFoundationCdmTest,
       CreateSessionAndGenerateRequest_CreateSessionFailure) {
  Initialize();

  COM_EXPECT_CALL(mf_cdm_,
                  CreateSession(MF_MEDIAKEYSESSION_TYPE_TEMPORARY, _, _))
      .WillOnce(Return(E_FAIL));

  std::vector<uint8_t> init_data = StringToVector("init_data");
  cdm_->CreateSessionAndGenerateRequest(
      CdmSessionType::kTemporary, EmeInitDataType::WEBM, init_data,
      std::make_unique<MockCdmSessionPromise>(/*expect_success=*/false,
                                              &session_id_));

  task_environment_.RunUntilIdle();
  EXPECT_TRUE(session_id_.empty());
}

// HardwareContextReset during SetServerCertificate() will cause the session
// closed and the promise resolved.
TEST_F(MediaFoundationCdmTest,
       CreateSessionAndGenerateRequest_CreateSessionHardwareContextReset) {
  Initialize();

  COM_EXPECT_CALL(mf_cdm_,
                  CreateSession(MF_MEDIAKEYSESSION_TYPE_TEMPORARY, _, _))
      .WillOnce(Return(DRM_E_TEE_INVALID_HWDRM_STATE));
  EXPECT_CALL(cdm_event_cb_, Run(CdmEvent::kHardwareContextReset,
                                 DRM_E_TEE_INVALID_HWDRM_STATE));
  EXPECT_CALL(cdm_client_,
              OnSessionClosed(StartsWith("DUMMY_"),
                              CdmSessionClosedReason::kHardwareContextReset));

  std::vector<uint8_t> init_data = StringToVector("init_data");
  cdm_->CreateSessionAndGenerateRequest(
      CdmSessionType::kTemporary, EmeInitDataType::WEBM, init_data,
      std::make_unique<MockCdmSessionPromise>(/*expect_success=*/true,
                                              &session_id_));

  task_environment_.RunUntilIdle();
  EXPECT_FALSE(session_id_.empty());
}

TEST_F(MediaFoundationCdmTest,
       CreateSessionAndGenerateRequest_GenerateRequestFailure) {
  Initialize();

  COM_EXPECT_CALL(mf_cdm_,
                  CreateSession(MF_MEDIAKEYSESSION_TYPE_TEMPORARY, _, _))
      .WillOnce(DoAll(SaveComPtr<1>(&mf_cdm_session_callbacks_),
                      SetComPointee<2>(mf_cdm_session_.Get()), Return(S_OK)));

  std::vector<uint8_t> init_data = StringToVector("init_data");
  COM_EXPECT_CALL(mf_cdm_session_,
                  GenerateRequest(StrEq(L"webm"), NotNull(), init_data.size()))
      .WillOnce(Return(E_FAIL));

  cdm_->CreateSessionAndGenerateRequest(
      CdmSessionType::kTemporary, EmeInitDataType::WEBM, init_data,
      std::make_unique<MockCdmSessionPromise>(/*expect_success=*/false,
                                              &session_id_));

  task_environment_.RunUntilIdle();
  EXPECT_TRUE(session_id_.empty());
}

TEST_F(MediaFoundationCdmTest,
       CreateSessionAndGenerateRequest_GenerateRequestHardwareContextReset) {
  Initialize();

  COM_EXPECT_CALL(mf_cdm_,
                  CreateSession(MF_MEDIAKEYSESSION_TYPE_TEMPORARY, _, _))
      .WillOnce(DoAll(SaveComPtr<1>(&mf_cdm_session_callbacks_),
                      SetComPointee<2>(mf_cdm_session_.Get()), Return(S_OK)));

  std::vector<uint8_t> init_data = StringToVector("init_data");
  COM_EXPECT_CALL(mf_cdm_session_,
                  GenerateRequest(StrEq(L"webm"), NotNull(), init_data.size()))
      .WillOnce(Return(DRM_E_TEE_INVALID_HWDRM_STATE));
  EXPECT_CALL(cdm_event_cb_, Run(CdmEvent::kHardwareContextReset,
                                 DRM_E_TEE_INVALID_HWDRM_STATE));
  EXPECT_CALL(cdm_client_,
              OnSessionClosed(StartsWith("DUMMY_"),
                              CdmSessionClosedReason::kHardwareContextReset));

  cdm_->CreateSessionAndGenerateRequest(
      CdmSessionType::kTemporary, EmeInitDataType::WEBM, init_data,
      std::make_unique<MockCdmSessionPromise>(/*expect_success=*/true,
                                              &session_id_));

  task_environment_.RunUntilIdle();
  EXPECT_FALSE(session_id_.empty());
}

// Duplicate session IDs cause session creation failure.
TEST_F(MediaFoundationCdmTest,
       CreateSessionAndGenerateRequest_DuplicateSessionId) {
  Initialize();

  auto mf_cdm_session_1 = MakeComPtr<MockMFCdmSession>();
  auto mf_cdm_session_2 = MakeComPtr<MockMFCdmSession>();
  ComPtr<IMFContentDecryptionModuleSessionCallbacks> mf_cdm_session_callbacks_1;
  ComPtr<IMFContentDecryptionModuleSessionCallbacks> mf_cdm_session_callbacks_2;

  COM_EXPECT_CALL(mf_cdm_,
                  CreateSession(MF_MEDIAKEYSESSION_TYPE_TEMPORARY, _, _))
      .WillOnce(DoAll(SaveComPtr<1>(&mf_cdm_session_callbacks_1),
                      SetComPointee<2>(mf_cdm_session_1.Get()), Return(S_OK)))
      .WillOnce(DoAll(SaveComPtr<1>(&mf_cdm_session_callbacks_2),
                      SetComPointee<2>(mf_cdm_session_2.Get()), Return(S_OK)));

  // In both sessions we return kSessionId. Session 1 succeeds. Session 2 fails
  // because of duplicate session ID.
  SetGenerateRequestExpectations(mf_cdm_session_1, kSessionId,
                                 &mf_cdm_session_callbacks_1);
  SetGenerateRequestExpectations(mf_cdm_session_2, kSessionId,
                                 &mf_cdm_session_callbacks_2,
                                 /*expect_message=*/false);
  std::string session_id_1;
  std::string session_id_2;
  std::vector<uint8_t> init_data = StringToVector("init_data");
  cdm_->CreateSessionAndGenerateRequest(
      CdmSessionType::kTemporary, EmeInitDataType::WEBM, init_data,
      std::make_unique<MockCdmSessionPromise>(/*expect_success=*/true,
                                              &session_id_1));
  cdm_->CreateSessionAndGenerateRequest(
      CdmSessionType::kTemporary, EmeInitDataType::WEBM, init_data,
      std::make_unique<MockCdmSessionPromise>(/*expect_success=*/false,
                                              &session_id_2));

  task_environment_.RunUntilIdle();
  EXPECT_EQ(session_id_1, kSessionId);
  EXPECT_TRUE(session_id_2.empty());
}

// LoadSession() is not implemented.
TEST_F(MediaFoundationCdmTest, LoadSession) {
  Initialize();

  cdm_->LoadSession(CdmSessionType::kPersistentLicense, kSessionId,
                    std::make_unique<MockCdmSessionPromise>(
                        /*expect_success=*/false, &session_id_));

  task_environment_.RunUntilIdle();
  EXPECT_TRUE(session_id_.empty());
}

TEST_F(MediaFoundationCdmTest, UpdateSession) {
  Initialize();
  CreateSessionAndGenerateRequest();

  std::vector<uint8_t> response = StringToVector("response");
  COM_EXPECT_CALL(mf_cdm_session_, Update(NotNull(), response.size()))
      .WillOnce(DoAll([&] { mf_cdm_session_callbacks_->KeyStatusChanged(); },
                      Return(S_OK)));
  COM_EXPECT_CALL(mf_cdm_session_, GetKeyStatuses(_, _)).WillOnce(Return(S_OK));
  COM_EXPECT_CALL(mf_cdm_session_, GetExpiration(_))
      .WillOnce(DoAll(SetArgPointee<0>(kExpirationMs), Return(S_OK)));
  EXPECT_CALL(cdm_client_, OnSessionKeysChangeCalled(_, true));
  EXPECT_CALL(cdm_client_, OnSessionExpirationUpdate(_, kExpirationTime));

  cdm_->UpdateSession(
      session_id_, response,
      std::make_unique<MockCdmPromise>(/*expect_success=*/true));
  task_environment_.RunUntilIdle();
}

TEST_F(MediaFoundationCdmTest, UpdateSession_InvalidSessionId) {
  Initialize();
  CreateSessionAndGenerateRequest();

  std::vector<uint8_t> response = StringToVector("response");
  cdm_->UpdateSession(
      "invalid_session_id", response,
      std::make_unique<MockCdmPromise>(/*expect_success=*/false));
  task_environment_.RunUntilIdle();
}

TEST_F(MediaFoundationCdmTest, UpdateSession_Failure) {
  Initialize();
  CreateSessionAndGenerateRequest();

  std::vector<uint8_t> response = StringToVector("response");
  COM_EXPECT_CALL(mf_cdm_session_, Update(NotNull(), response.size()))
      .WillOnce(Return(E_FAIL));

  cdm_->UpdateSession(
      session_id_, response,
      std::make_unique<MockCdmPromise>(/*expect_success=*/false));
  task_environment_.RunUntilIdle();
}

TEST_F(MediaFoundationCdmTest, UpdateSession_HardwareContextReset) {
  Initialize();
  CreateSessionAndGenerateRequest();

  std::vector<uint8_t> response = StringToVector("response");
  COM_EXPECT_CALL(mf_cdm_session_, Update(NotNull(), response.size()))
      .WillOnce(Return(DRM_E_TEE_INVALID_HWDRM_STATE));
  COM_EXPECT_CALL(mf_cdm_session_, Close()).WillOnce(Return(S_OK));
  EXPECT_CALL(cdm_client_,
              OnSessionClosed(kSessionId,
                              CdmSessionClosedReason::kHardwareContextReset));
  EXPECT_CALL(cdm_event_cb_, Run(CdmEvent::kHardwareContextReset,
                                 DRM_E_TEE_INVALID_HWDRM_STATE));

  cdm_->UpdateSession(
      session_id_, response,
      std::make_unique<MockCdmPromise>(/*expect_success=*/true));
  task_environment_.RunUntilIdle();
}

TEST_F(MediaFoundationCdmTest, CloseSession) {
  Initialize();
  CreateSessionAndGenerateRequest();

  COM_EXPECT_CALL(mf_cdm_session_, Close()).WillOnce(Return(S_OK));
  EXPECT_CALL(cdm_client_,
              OnSessionClosed(kSessionId, CdmSessionClosedReason::kClose));

  cdm_->CloseSession(session_id_,
                     std::make_unique<MockCdmPromise>(/*expect_success=*/true));
  task_environment_.RunUntilIdle();
}

TEST_F(MediaFoundationCdmTest, CloseSession_Failure) {
  Initialize();
  CreateSessionAndGenerateRequest();

  COM_EXPECT_CALL(mf_cdm_session_, Close()).WillOnce(Return(E_FAIL));

  cdm_->CloseSession(
      session_id_, std::make_unique<MockCdmPromise>(/*expect_success=*/false));
  task_environment_.RunUntilIdle();
}

// DRM_E_TEE_INVALID_HWDRM_STATE not handled for CloseSession yet.
TEST_F(MediaFoundationCdmTest, CloseSession_HardwareContextReset) {
  Initialize();
  CreateSessionAndGenerateRequest();

  COM_EXPECT_CALL(mf_cdm_session_, Close())
      .WillOnce(Return(DRM_E_TEE_INVALID_HWDRM_STATE));

  cdm_->CloseSession(
      session_id_, std::make_unique<MockCdmPromise>(/*expect_success=*/false));
  task_environment_.RunUntilIdle();
}

TEST_F(MediaFoundationCdmTest, RemoveSession) {
  Initialize();
  CreateSessionAndGenerateRequest();

  COM_EXPECT_CALL(mf_cdm_session_, Remove()).WillOnce(Return(S_OK));
  COM_EXPECT_CALL(mf_cdm_session_, GetExpiration(_))
      .WillOnce(DoAll(SetArgPointee<0>(kExpirationMs), Return(S_OK)));
  EXPECT_CALL(cdm_client_, OnSessionExpirationUpdate(_, kExpirationTime));

  cdm_->RemoveSession(
      session_id_, std::make_unique<MockCdmPromise>(/*expect_success=*/true));
  task_environment_.RunUntilIdle();
}

TEST_F(MediaFoundationCdmTest, RemoveSession_Failure) {
  Initialize();
  CreateSessionAndGenerateRequest();

  COM_EXPECT_CALL(mf_cdm_session_, Remove()).WillOnce(Return(E_FAIL));

  cdm_->RemoveSession(
      session_id_, std::make_unique<MockCdmPromise>(/*expect_success=*/false));
  task_environment_.RunUntilIdle();
}

TEST_F(MediaFoundationCdmTest, RemoveSession_HardwareContextReset) {
  Initialize();
  CreateSessionAndGenerateRequest();

  COM_EXPECT_CALL(mf_cdm_session_, Remove())
      .WillOnce(Return(DRM_E_TEE_INVALID_HWDRM_STATE));
  COM_EXPECT_CALL(mf_cdm_session_, Close()).WillOnce(Return(S_OK));
  EXPECT_CALL(cdm_client_,
              OnSessionClosed(kSessionId,
                              CdmSessionClosedReason::kHardwareContextReset));
  EXPECT_CALL(cdm_event_cb_, Run(CdmEvent::kHardwareContextReset,
                                 DRM_E_TEE_INVALID_HWDRM_STATE));

  cdm_->RemoveSession(
      session_id_, std::make_unique<MockCdmPromise>(/*expect_success=*/true));
  task_environment_.RunUntilIdle();
}

TEST_F(MediaFoundationCdmTest, HardwareContextReset) {
  Initialize();
  CreateSessionAndGenerateRequest();

  CdmContext* cdm_context = cdm_->GetCdmContext();
  mf_cdm_proxy_ = cdm_context->GetMediaFoundationCdmProxy();
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(mf_cdm_proxy_);

  COM_EXPECT_CALL(mf_cdm_session_, Close()).WillOnce(Return(S_OK));
  EXPECT_CALL(cdm_client_,
              OnSessionClosed(kSessionId,
                              CdmSessionClosedReason::kHardwareContextReset));
  EXPECT_CALL(cdm_event_cb_, Run(CdmEvent::kHardwareContextReset,
                                 DRM_E_TEE_INVALID_HWDRM_STATE));
  mf_cdm_proxy_->OnHardwareContextReset();

  // Create a new session and expect success.
  CreateSessionAndGenerateRequest();
}

TEST_F(MediaFoundationCdmTest, HardwareContextReset_InitializeFailure) {
  Initialize();
  CreateSessionAndGenerateRequest();

  CdmContext* cdm_context = cdm_->GetCdmContext();
  mf_cdm_proxy_ = cdm_context->GetMediaFoundationCdmProxy();
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(mf_cdm_proxy_);

  // Make the next `Initialize()` fail.
  can_initialize_ = false;
  EXPECT_CALL(cdm_event_cb_, Run(CdmEvent::kCdmError, E_FAIL));

  COM_EXPECT_CALL(mf_cdm_session_, Close()).WillOnce(Return(S_OK));
  EXPECT_CALL(cdm_client_,
              OnSessionClosed(kSessionId,
                              CdmSessionClosedReason::kHardwareContextReset));
  EXPECT_CALL(cdm_event_cb_, Run(CdmEvent::kHardwareContextReset,
                                 DRM_E_TEE_INVALID_HWDRM_STATE));
  mf_cdm_proxy_->OnHardwareContextReset();

  std::vector<uint8_t> init_data = StringToVector("init_data");
  cdm_->CreateSessionAndGenerateRequest(
      CdmSessionType::kTemporary, EmeInitDataType::WEBM, init_data,
      std::make_unique<MockCdmSessionPromise>(/*expect_success=*/false,
                                              &session_id_));
  task_environment_.RunUntilIdle();
}

}  // namespace media
