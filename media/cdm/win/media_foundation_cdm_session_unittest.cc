// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/win/media_foundation_cdm_session.h"

#include <wchar.h>

#include "base/functional/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "media/base/mock_filters.h"
#include "media/base/test_helpers.h"
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
using ::testing::StrictMock;
using ::testing::WithoutArgs;

namespace media {

namespace {

const double kExpirationMs = 123456789.0;
const auto kExpirationTime =
    base::Time::FromMillisecondsSinceUnixEpoch(kExpirationMs);
const char kTestUmaPrefix[] = "Media.EME.TestUmaPrefix.";

std::vector<uint8_t> StringToVector(const std::string& str) {
  return std::vector<uint8_t>(str.begin(), str.end());
}

}  // namespace

using Microsoft::WRL::ComPtr;

class MediaFoundationCdmSessionTest : public testing::Test {
 public:
  MediaFoundationCdmSessionTest()
      : mf_cdm_(MakeComPtr<MockMFCdm>()),
        mf_cdm_session_(MakeComPtr<MockMFCdmSession>()),
        cdm_session_(
            kTestUmaPrefix,
            base::BindRepeating(&MockCdmClient::OnSessionMessage,
                                base::Unretained(&cdm_client_)),
            base::BindRepeating(&MockCdmClient::OnSessionKeysChange,
                                base::Unretained(&cdm_client_)),
            base::BindRepeating(&MockCdmClient::OnSessionExpirationUpdate,
                                base::Unretained(&cdm_client_))) {}

  ~MediaFoundationCdmSessionTest() override = default;

  void Initialize() {
    COM_EXPECT_CALL(mf_cdm_,
                    CreateSession(MF_MEDIAKEYSESSION_TYPE_TEMPORARY, _, _))
        .WillOnce(DoAll(SaveComPtr<1>(&mf_cdm_session_callbacks_),
                        SetComPointee<2>(mf_cdm_session_.Get()), Return(S_OK)));
    ASSERT_SUCCESS(
        cdm_session_.Initialize(mf_cdm_.Get(), CdmSessionType::kTemporary));
  }

  void GenerateRequest() {
    std::vector<uint8_t> init_data = StringToVector("init_data");
    std::vector<uint8_t> license_request = StringToVector("request");
    base::MockCallback<MediaFoundationCdmSession::SessionIdCB> session_id_cb;

    // Session ID to return. Will be released by |mf_cdm_session_|.
    LPWSTR session_id = nullptr;
    ASSERT_SUCCESS(CopyCoTaskMemWideString(L"session_id", &session_id));

    {
      // Use InSequence here because the order of events matter. |session_id_cb|
      // must be called before OnSessionMessage().
      InSequence seq;
      COM_EXPECT_CALL(mf_cdm_session_, GenerateRequest(_, _, init_data.size()))
          .WillOnce(WithoutArgs([&] {
            mf_cdm_session_callbacks_->KeyMessage(
                MF_MEDIAKEYSESSION_MESSAGETYPE_LICENSE_REQUEST,
                license_request.data(), license_request.size(), nullptr);
            return S_OK;
          }));
      COM_EXPECT_CALL(mf_cdm_session_, GetSessionId(_))
          .WillOnce(DoAll(SetArgPointee<0>(session_id), Return(S_OK)));
      EXPECT_CALL(session_id_cb, Run(_)).WillOnce(Return(true));
      EXPECT_CALL(cdm_client_,
                  OnSessionMessage(_, CdmMessageType::LICENSE_REQUEST,
                                   license_request));
    }

    EXPECT_SUCCESS(cdm_session_.GenerateRequest(
        EmeInitDataType::WEBM, init_data, session_id_cb.Get()));
    task_environment_.RunUntilIdle();
  }

 protected:
  base::test::TaskEnvironment task_environment_;

  StrictMock<MockCdmClient> cdm_client_;
  ComPtr<MockMFCdm> mf_cdm_;
  ComPtr<MockMFCdmSession> mf_cdm_session_;
  MediaFoundationCdmSession cdm_session_;
  ComPtr<IMFContentDecryptionModuleSessionCallbacks> mf_cdm_session_callbacks_;
};

TEST_F(MediaFoundationCdmSessionTest, Initialize) {
  Initialize();
}

TEST_F(MediaFoundationCdmSessionTest, Initialize_Failure) {
  COM_EXPECT_CALL(mf_cdm_,
                  CreateSession(MF_MEDIAKEYSESSION_TYPE_TEMPORARY, _, _))
      .WillOnce(DoAll(SaveComPtr<1>(&mf_cdm_session_callbacks_),
                      SetComPointee<2>(mf_cdm_session_.Get()), Return(E_FAIL)));
  EXPECT_FAILED(
      cdm_session_.Initialize(mf_cdm_.Get(), CdmSessionType::kTemporary));
}

TEST_F(MediaFoundationCdmSessionTest, GenerateRequest) {
  Initialize();
  GenerateRequest();
}

TEST_F(MediaFoundationCdmSessionTest, GenerateRequest_Failure) {
  Initialize();
  std::vector<uint8_t> init_data = StringToVector("init_data");
  base::MockCallback<MediaFoundationCdmSession::SessionIdCB> session_id_cb;

  COM_EXPECT_CALL(mf_cdm_session_, GenerateRequest(_, _, init_data.size()))
      .WillOnce(Return(E_FAIL));
  EXPECT_FAILED(cdm_session_.GenerateRequest(EmeInitDataType::WEBM, init_data,
                                             session_id_cb.Get()));
  task_environment_.RunUntilIdle();
}

TEST_F(MediaFoundationCdmSessionTest, GetSessionId_Failure) {
  Initialize();
  std::vector<uint8_t> init_data = StringToVector("init_data");
  std::vector<uint8_t> license_request = StringToVector("request");
  base::MockCallback<MediaFoundationCdmSession::SessionIdCB> session_id_cb;

  COM_EXPECT_CALL(mf_cdm_session_, GenerateRequest(_, _, init_data.size()))
      .WillOnce(WithoutArgs([&] {
        mf_cdm_session_callbacks_->KeyMessage(
            MF_MEDIAKEYSESSION_MESSAGETYPE_LICENSE_REQUEST,
            license_request.data(), license_request.size(), nullptr);
        return S_OK;
      }));
  COM_EXPECT_CALL(mf_cdm_session_, GetSessionId(_)).WillOnce(Return(E_FAIL));
  EXPECT_CALL(session_id_cb, Run(IsEmpty()));
  // OnSessionMessage() will not be called.

  EXPECT_SUCCESS(cdm_session_.GenerateRequest(EmeInitDataType::WEBM, init_data,
                                              session_id_cb.Get()));
  task_environment_.RunUntilIdle();
}

TEST_F(MediaFoundationCdmSessionTest, GetSessionId_Empty) {
  Initialize();
  std::vector<uint8_t> init_data = StringToVector("init_data");
  std::vector<uint8_t> license_request = StringToVector("request");
  base::MockCallback<MediaFoundationCdmSession::SessionIdCB> session_id_cb;

  // Session ID to return. Will be released by |mf_cdm_session_|.
  LPWSTR empty_session_id = nullptr;
  ASSERT_SUCCESS(CopyCoTaskMemWideString(L"", &empty_session_id));

  COM_EXPECT_CALL(mf_cdm_session_, GenerateRequest(_, _, init_data.size()))
      .WillOnce(WithoutArgs([&] {
        mf_cdm_session_callbacks_->KeyMessage(
            MF_MEDIAKEYSESSION_MESSAGETYPE_LICENSE_REQUEST,
            license_request.data(), license_request.size(), nullptr);
        return S_OK;
      }));
  COM_EXPECT_CALL(mf_cdm_session_, GetSessionId(_))
      .WillOnce(DoAll(SetArgPointee<0>(empty_session_id), Return(S_OK)));
  EXPECT_CALL(session_id_cb, Run(IsEmpty()));
  // OnSessionMessage() will not be called since session ID is empty.

  EXPECT_SUCCESS(cdm_session_.GenerateRequest(EmeInitDataType::WEBM, init_data,
                                              session_id_cb.Get()));
  task_environment_.RunUntilIdle();
}

TEST_F(MediaFoundationCdmSessionTest, Update) {
  Initialize();
  GenerateRequest();

  std::vector<uint8_t> response = StringToVector("response");
  COM_EXPECT_CALL(mf_cdm_session_, Update(NotNull(), response.size()))
      .WillOnce(DoAll([&] { mf_cdm_session_callbacks_->KeyStatusChanged(); },
                      Return(S_OK)));
  COM_EXPECT_CALL(mf_cdm_session_, GetKeyStatuses(_, _)).WillOnce(Return(S_OK));
  COM_EXPECT_CALL(mf_cdm_session_, GetExpiration(_))
      .WillOnce(DoAll(SetArgPointee<0>(kExpirationMs), Return(S_OK)));
  EXPECT_CALL(cdm_client_, OnSessionKeysChangeCalled(_, true));
  EXPECT_CALL(cdm_client_, OnSessionExpirationUpdate(_, kExpirationTime));

  EXPECT_SUCCESS(cdm_session_.Update(response));
  task_environment_.RunUntilIdle();
}

TEST_F(MediaFoundationCdmSessionTest, Update_Failure) {
  Initialize();
  GenerateRequest();

  std::vector<uint8_t> response = StringToVector("response");
  COM_EXPECT_CALL(mf_cdm_session_, Update(NotNull(), response.size()))
      .WillOnce(Return(E_FAIL));
  EXPECT_FAILED(cdm_session_.Update(response));
}

TEST_F(MediaFoundationCdmSessionTest, Close) {
  Initialize();
  GenerateRequest();

  COM_EXPECT_CALL(mf_cdm_session_, Close()).WillOnce(Return(S_OK));
  EXPECT_SUCCESS(cdm_session_.Close());
}

TEST_F(MediaFoundationCdmSessionTest, Close_Failure) {
  Initialize();
  GenerateRequest();

  COM_EXPECT_CALL(mf_cdm_session_, Close()).WillOnce(Return(E_FAIL));
  EXPECT_FAILED(cdm_session_.Close());
}

TEST_F(MediaFoundationCdmSessionTest, Remove) {
  Initialize();
  GenerateRequest();

  COM_EXPECT_CALL(mf_cdm_session_, Remove()).WillOnce(Return(S_OK));
  COM_EXPECT_CALL(mf_cdm_session_, GetExpiration(_))
      .WillOnce(DoAll(SetArgPointee<0>(kExpirationMs), Return(S_OK)));
  EXPECT_CALL(cdm_client_, OnSessionExpirationUpdate(_, kExpirationTime));

  EXPECT_SUCCESS(cdm_session_.Remove());
}

TEST_F(MediaFoundationCdmSessionTest, Remove_Failure) {
  Initialize();
  GenerateRequest();

  COM_EXPECT_CALL(mf_cdm_session_, Remove()).WillOnce(Return(E_FAIL));
  EXPECT_FAILED(cdm_session_.Remove());
}

}  // namespace media
