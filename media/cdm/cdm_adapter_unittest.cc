// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/cdm_adapter.h"

#include <stdint.h>
#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "media/base/cdm_callback_promise.h"
#include "media/base/cdm_key_information.h"
#include "media/base/content_decryption_module.h"
#include "media/base/media_switches.h"
#include "media/base/mock_filters.h"
#include "media/cdm/api/content_decryption_module.h"
#include "media/cdm/cdm_module.h"
#include "media/cdm/external_clear_key_test_helper.h"
#include "media/cdm/library_cdm/cdm_host_proxy.h"
#include "media/cdm/library_cdm/mock_library_cdm.h"
#include "media/cdm/mock_helpers.h"
#include "media/cdm/simple_cdm_allocator.h"
#include "media/media_buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Invoke;
using ::testing::IsNull;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::Values;
using ::testing::SaveArg;
using ::testing::StrictMock;
using ::testing::_;

MATCHER(IsNotEmpty, "") {
  return !arg.empty();
}

MATCHER(IsNullTime, "") {
  return arg.is_null();
}

MATCHER(IsNullPlatformChallengeResponse, "") {
  // Only check the |signed_data| for simplicity.
  return !arg.signed_data;
}

// TODO(jrummell): These tests are a subset of those in aes_decryptor_unittest.
// Refactor aes_decryptor_unittest.cc to handle AesDecryptor directly and
// via CdmAdapter once CdmAdapter supports decrypting functionality. There
// will also be tests that only CdmAdapter supports, like file IO, which
// will need to be handled separately.

namespace media {

namespace {

// Random key ID used to create a session.
const uint8_t kKeyId[] = {
    // base64 equivalent is AQIDBAUGBwgJCgsMDQ4PEA
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
    0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10,
};

const char kKeyIdAsJWK[] = "{\"kids\": [\"AQIDBAUGBwgJCgsMDQ4PEA\"]}";

const uint8_t kKeyIdAsPssh[] = {
    0x00, 0x00, 0x00, 0x34,                          // size = 52
    'p',  's',  's',  'h',                           // 'pssh'
    0x01,                                            // version = 1
    0x00, 0x00, 0x00,                                // flags
    0x10, 0x77, 0xEF, 0xEC, 0xC0, 0xB2, 0x4D, 0x02,  // Common SystemID
    0xAC, 0xE3, 0x3C, 0x1E, 0x52, 0xE2, 0xFB, 0x4B,
    0x00, 0x00, 0x00, 0x01,                          // key count
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,  // key
    0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10,
    0x00, 0x00, 0x00, 0x00,  // datasize
};

// Key is 0x0405060708090a0b0c0d0e0f10111213,
// base64 equivalent is BAUGBwgJCgsMDQ4PEBESEw.
const char kKeyAsJWK[] =
    "{"
    "  \"keys\": ["
    "    {"
    "      \"kty\": \"oct\","
    "      \"alg\": \"A128KW\","
    "      \"kid\": \"AQIDBAUGBwgJCgsMDQ4PEA\","
    "      \"k\": \"BAUGBwgJCgsMDQ4PEBESEw\""
    "    }"
    "  ],"
    "  \"type\": \"temporary\""
    "}";

class MockFileIOClient : public cdm::FileIOClient {
 public:
  MockFileIOClient() = default;
  ~MockFileIOClient() override = default;

  MOCK_METHOD1(OnOpenComplete, void(Status));
  MOCK_METHOD3(OnReadComplete, void(Status, const uint8_t*, uint32_t));
  MOCK_METHOD1(OnWriteComplete, void(Status));
};

}  // namespace

// Tests CdmAdapter with the following parameter:
// - int: CDM interface version to test.
class CdmAdapterTestBase : public testing::Test,
                           public testing::WithParamInterface<int> {
 public:
  enum ExpectedResult { SUCCESS, FAILURE };

  CdmAdapterTestBase() {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kOverrideEnabledCdmInterfaceVersion,
        base::NumberToString(GetCdmInterfaceVersion()));
  }

  ~CdmAdapterTestBase() override { CdmModule::ResetInstanceForTesting(); }

 protected:
  virtual std::string GetKeySystemName() = 0;
  virtual CdmAdapter::CreateCdmFunc GetCreateCdmFunc() = 0;

  int GetCdmInterfaceVersion() { return GetParam(); }

  // Initializes the adapter. |expected_result| tests that the call succeeds
  // or generates an error.
  void InitializeWithCdmConfigAndExpect(const CdmConfig& cdm_config,
                                        ExpectedResult expected_result) {
    std::unique_ptr<CdmAllocator> allocator(new SimpleCdmAllocator());
    std::unique_ptr<StrictMock<MockCdmAuxiliaryHelper>> cdm_helper(
        new StrictMock<MockCdmAuxiliaryHelper>(std::move(allocator)));
    cdm_helper_ = cdm_helper.get();
    CdmAdapter::Create(GetKeySystemName(),
                       url::Origin::Create(GURL("http://foo.com")), cdm_config,
                       GetCreateCdmFunc(), std::move(cdm_helper),
                       base::Bind(&MockCdmClient::OnSessionMessage,
                                  base::Unretained(&cdm_client_)),
                       base::Bind(&MockCdmClient::OnSessionClosed,
                                  base::Unretained(&cdm_client_)),
                       base::Bind(&MockCdmClient::OnSessionKeysChange,
                                  base::Unretained(&cdm_client_)),
                       base::Bind(&MockCdmClient::OnSessionExpirationUpdate,
                                  base::Unretained(&cdm_client_)),
                       base::Bind(&CdmAdapterTestBase::OnCdmCreated,
                                  base::Unretained(this), expected_result));
    RunUntilIdle();
    ASSERT_EQ(expected_result == SUCCESS, !!cdm_);
  }

  void InitializeAndExpect(ExpectedResult expected_result) {
    // Default settings of false are sufficient for most tests.
    CdmConfig cdm_config;
    InitializeWithCdmConfigAndExpect(cdm_config, expected_result);
  }

  void OnCdmCreated(ExpectedResult expected_result,
                    const scoped_refptr<ContentDecryptionModule>& cdm,
                    const std::string& error_message) {
    if (cdm) {
      ASSERT_EQ(expected_result, SUCCESS)
          << "CDM creation succeeded unexpectedly.";
      CdmAdapter* cdm_adapter = static_cast<CdmAdapter*>(cdm.get());
      ASSERT_EQ(GetCdmInterfaceVersion(), cdm_adapter->GetInterfaceVersion());
      cdm_ = cdm;
    } else {
      ASSERT_EQ(expected_result, FAILURE) << error_message;
    }
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  StrictMock<MockCdmClient> cdm_client_;
  StrictMock<MockCdmAuxiliaryHelper>* cdm_helper_ = nullptr;

  // Keep track of the loaded CDM.
  scoped_refptr<ContentDecryptionModule> cdm_;

  base::test::SingleThreadTaskEnvironment task_environment_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CdmAdapterTestBase);
};

class CdmAdapterTestWithClearKeyCdm : public CdmAdapterTestBase {
 public:
  ~CdmAdapterTestWithClearKeyCdm() {
    // Clear |cdm_| before we destroy |helper_|.
    cdm_ = nullptr;
    RunUntilIdle();
  }

  void SetUp() override {
    CdmAdapterTestBase::SetUp();

#if BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
    CdmModule::GetInstance()->Initialize(helper_.LibraryPath(), {});
#else
    CdmModule::GetInstance()->Initialize(helper_.LibraryPath());
#endif  // BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
  }

  // CdmAdapterTestBase implementation.
  std::string GetKeySystemName() override { return helper_.KeySystemName(); }
  CdmAdapter::CreateCdmFunc GetCreateCdmFunc() override {
    return CdmModule::GetInstance()->GetCreateCdmFunc();
  }

 protected:
  // Creates a new session using |key_id|. |session_id_| will be set
  // when the promise is resolved. |expected_result| tests that
  // CreateSessionAndGenerateRequest() succeeds or generates an error.
  void CreateSessionAndExpect(EmeInitDataType data_type,
                              const std::vector<uint8_t>& key_id,
                              ExpectedResult expected_result) {
    DCHECK(!key_id.empty());

    if (expected_result == SUCCESS) {
      EXPECT_CALL(cdm_client_, OnSessionMessage(IsNotEmpty(), _, _));
    }

    cdm_->CreateSessionAndGenerateRequest(
        CdmSessionType::kTemporary, data_type, key_id,
        CreateSessionPromise(expected_result));
    RunUntilIdle();
  }

  // Loads the session specified by |session_id|. |expected_result| tests
  // that LoadSession() succeeds or generates an error.
  void LoadSessionAndExpect(const std::string& session_id,
                            ExpectedResult expected_result) {
    DCHECK(!session_id.empty());
    ASSERT_EQ(expected_result, FAILURE) << "LoadSession not supported.";

    cdm_->LoadSession(CdmSessionType::kTemporary, session_id,
                      CreateSessionPromise(expected_result));
    RunUntilIdle();
  }

  // Updates the session specified by |session_id| with |key|. |expected_result|
  // tests that the update succeeds or generates an error. |new_key_expected|
  // is the expected parameter when the SessionKeysChange event happens.
  void UpdateSessionAndExpect(std::string session_id,
                              const std::string& key,
                              ExpectedResult expected_result,
                              bool new_key_expected) {
    DCHECK(!key.empty());

    if (expected_result == SUCCESS) {
      EXPECT_CALL(cdm_client_,
                  OnSessionKeysChangeCalled(session_id, new_key_expected));
      EXPECT_CALL(cdm_client_,
                  OnSessionExpirationUpdate(session_id, IsNullTime()));
    } else {
      EXPECT_CALL(cdm_client_, OnSessionKeysChangeCalled(_, _)).Times(0);
      EXPECT_CALL(cdm_client_, OnSessionExpirationUpdate(_, _)).Times(0);
    }

    cdm_->UpdateSession(session_id,
                        std::vector<uint8_t>(key.begin(), key.end()),
                        CreatePromise(expected_result));
    RunUntilIdle();
  }

  std::string SessionId() { return session_id_; }

 private:
  // Methods used for promise resolved/rejected.
  MOCK_METHOD0(OnResolve, void());
  MOCK_METHOD1(OnResolveWithSession, void(const std::string& session_id));
  MOCK_METHOD3(OnReject,
               void(CdmPromise::Exception exception_code,
                    uint32_t system_code,
                    const std::string& error_message));

  // Create a promise. |expected_result| is used to indicate how the promise
  // should be fulfilled.
  std::unique_ptr<SimpleCdmPromise> CreatePromise(
      ExpectedResult expected_result) {
    if (expected_result == SUCCESS) {
      EXPECT_CALL(*this, OnResolve());
    } else {
      EXPECT_CALL(*this, OnReject(_, _, IsNotEmpty()));
    }

    std::unique_ptr<SimpleCdmPromise> promise(new CdmCallbackPromise<>(
        base::Bind(&CdmAdapterTestWithClearKeyCdm::OnResolve,
                   base::Unretained(this)),
        base::Bind(&CdmAdapterTestWithClearKeyCdm::OnReject,
                   base::Unretained(this))));
    return promise;
  }

  // Create a promise to be used when a new session is created.
  // |expected_result| is used to indicate how the promise should be fulfilled.
  std::unique_ptr<NewSessionCdmPromise> CreateSessionPromise(
      ExpectedResult expected_result) {
    if (expected_result == SUCCESS) {
      EXPECT_CALL(*this, OnResolveWithSession(_))
          .WillOnce(SaveArg<0>(&session_id_));
    } else {
      EXPECT_CALL(*this, OnReject(_, _, IsNotEmpty()));
    }

    std::unique_ptr<NewSessionCdmPromise> promise(
        new CdmCallbackPromise<std::string>(
            base::Bind(&CdmAdapterTestWithClearKeyCdm::OnResolveWithSession,
                       base::Unretained(this)),
            base::Bind(&CdmAdapterTestWithClearKeyCdm::OnReject,
                       base::Unretained(this))));
    return promise;
  }

  // Helper class to load/unload Clear Key CDM Library.
  // TODO(xhwang): CdmModule does CDM loading/unloading by itself. So it seems
  // we don't need to use ExternalClearKeyTestHelper. Simplify this if possible.
  ExternalClearKeyTestHelper helper_;

  // |session_id_| is the latest result of calling CreateSession().
  std::string session_id_;
};

class CdmAdapterTestWithMockCdm : public CdmAdapterTestBase {
 public:
  ~CdmAdapterTestWithMockCdm() override {
    // Makes sure Destroy() is called on CdmAdapter destruction.
    EXPECT_CALL(*mock_library_cdm_, DestroyCalled());
    cdm_ = nullptr;
    RunUntilIdle();
  }

  // CdmAdapterTestBase implementation.
  std::string GetKeySystemName() override { return "x-com.mock"; }
  CdmAdapter::CreateCdmFunc GetCreateCdmFunc() override {
    return CreateMockLibraryCdm;
  }

 protected:
  void InitializeWithCdmConfig(const CdmConfig& cdm_config) {
    // TODO(xhwang): Add tests for failure cases.
    InitializeWithCdmConfigAndExpect(cdm_config, SUCCESS);
    mock_library_cdm_ = MockLibraryCdm::GetInstance();
    ASSERT_TRUE(mock_library_cdm_);
    cdm_host_proxy_ = mock_library_cdm_->GetCdmHostProxy();
    ASSERT_TRUE(cdm_host_proxy_);
  }

  MockLibraryCdm* mock_library_cdm_ = nullptr;
  CdmHostProxy* cdm_host_proxy_ = nullptr;
};

// Instantiate test cases

INSTANTIATE_TEST_SUITE_P(CDM_10, CdmAdapterTestWithClearKeyCdm, Values(10));
INSTANTIATE_TEST_SUITE_P(CDM_11, CdmAdapterTestWithClearKeyCdm, Values(11));

INSTANTIATE_TEST_SUITE_P(CDM_10, CdmAdapterTestWithMockCdm, Values(10));
INSTANTIATE_TEST_SUITE_P(CDM_11, CdmAdapterTestWithMockCdm, Values(11));

// CdmAdapterTestWithClearKeyCdm Tests

TEST_P(CdmAdapterTestWithClearKeyCdm, Initialize) {
  InitializeAndExpect(SUCCESS);
}

// TODO(xhwang): This belongs to CdmModuleTest.
TEST_P(CdmAdapterTestWithClearKeyCdm, BadLibraryPath) {
  CdmModule::ResetInstanceForTesting();

#if BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
  CdmModule::GetInstance()->Initialize(
      base::FilePath(FILE_PATH_LITERAL("no_library_here")), {});
#else
  CdmModule::GetInstance()->Initialize(
      base::FilePath(FILE_PATH_LITERAL("no_library_here")));
#endif  // BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)

  ASSERT_FALSE(GetCreateCdmFunc());
}

TEST_P(CdmAdapterTestWithClearKeyCdm, CreateWebmSession) {
  InitializeAndExpect(SUCCESS);

  std::vector<uint8_t> key_id(kKeyId, kKeyId + base::size(kKeyId));
  CreateSessionAndExpect(EmeInitDataType::WEBM, key_id, SUCCESS);
}

TEST_P(CdmAdapterTestWithClearKeyCdm, CreateKeyIdsSession) {
  InitializeAndExpect(SUCCESS);

  // Don't include the trailing /0 from the string in the data passed in.
  std::vector<uint8_t> key_id(kKeyIdAsJWK,
                              kKeyIdAsJWK + base::size(kKeyIdAsJWK) - 1);
  CreateSessionAndExpect(EmeInitDataType::KEYIDS, key_id, SUCCESS);
}

TEST_P(CdmAdapterTestWithClearKeyCdm, CreateCencSession) {
  InitializeAndExpect(SUCCESS);

  std::vector<uint8_t> key_id(kKeyIdAsPssh,
                              kKeyIdAsPssh + base::size(kKeyIdAsPssh));
  CreateSessionAndExpect(EmeInitDataType::CENC, key_id, SUCCESS);
}

TEST_P(CdmAdapterTestWithClearKeyCdm, CreateSessionWithBadData) {
  InitializeAndExpect(SUCCESS);

  // Use |kKeyId| but specify KEYIDS format.
  std::vector<uint8_t> key_id(kKeyId, kKeyId + base::size(kKeyId));
  CreateSessionAndExpect(EmeInitDataType::KEYIDS, key_id, FAILURE);
}

TEST_P(CdmAdapterTestWithClearKeyCdm, LoadSession) {
  InitializeAndExpect(SUCCESS);

  // LoadSession() is not supported by AesDecryptor.
  std::vector<uint8_t> key_id(kKeyId, kKeyId + base::size(kKeyId));
  CreateSessionAndExpect(EmeInitDataType::KEYIDS, key_id, FAILURE);
}

TEST_P(CdmAdapterTestWithClearKeyCdm, UpdateSession) {
  InitializeAndExpect(SUCCESS);

  std::vector<uint8_t> key_id(kKeyId, kKeyId + base::size(kKeyId));
  CreateSessionAndExpect(EmeInitDataType::WEBM, key_id, SUCCESS);

  UpdateSessionAndExpect(SessionId(), kKeyAsJWK, SUCCESS, true);
}

TEST_P(CdmAdapterTestWithClearKeyCdm, UpdateSessionWithBadData) {
  InitializeAndExpect(SUCCESS);

  std::vector<uint8_t> key_id(kKeyId, kKeyId + base::size(kKeyId));
  CreateSessionAndExpect(EmeInitDataType::WEBM, key_id, SUCCESS);

  UpdateSessionAndExpect(SessionId(), "random data", FAILURE, true);
}

// CdmAdapterTestWithMockCdm Tests

// ChallengePlatform() will ask the helper to send platform challenge.
TEST_P(CdmAdapterTestWithMockCdm, ChallengePlatform) {
  CdmConfig cdm_config;
  cdm_config.allow_distinctive_identifier = true;
  InitializeWithCdmConfig(cdm_config);

  std::string service_id = "service_id";
  std::string challenge = "challenge";
  EXPECT_CALL(*cdm_helper_, ChallengePlatformCalled(service_id, challenge))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_library_cdm_, OnPlatformChallengeResponse(_));
  cdm_host_proxy_->SendPlatformChallenge(service_id.data(), service_id.size(),
                                         challenge.data(), challenge.size());
  RunUntilIdle();
}

// ChallengePlatform() will always fail if |allow_distinctive_identifier| is
// false.
TEST_P(CdmAdapterTestWithMockCdm,
       ChallengePlatform_DistinctiveIdentifierNotAllowed) {
  CdmConfig cdm_config;
  InitializeWithCdmConfig(cdm_config);

  EXPECT_CALL(*mock_library_cdm_,
              OnPlatformChallengeResponse(IsNullPlatformChallengeResponse()));
  std::string service_id = "service_id";
  std::string challenge = "challenge";
  cdm_host_proxy_->SendPlatformChallenge(service_id.data(), service_id.size(),
                                         challenge.data(), challenge.size());
  RunUntilIdle();
}

// CreateFileIO() will ask helper to create FileIO.
TEST_P(CdmAdapterTestWithMockCdm, CreateFileIO) {
  CdmConfig cdm_config;
  cdm_config.allow_persistent_state = true;
  InitializeWithCdmConfig(cdm_config);

  MockFileIOClient file_io_client;
  EXPECT_CALL(*cdm_helper_, CreateCdmFileIO(_));
  cdm_host_proxy_->CreateFileIO(&file_io_client);
  RunUntilIdle();
}

// CreateFileIO() will always fail if |allow_persistent_state| is false.
TEST_P(CdmAdapterTestWithMockCdm, CreateFileIO_PersistentStateNotAllowed) {
  CdmConfig cdm_config;
  InitializeWithCdmConfig(cdm_config);

  // When |allow_persistent_state| is false, should return null immediately
  // without asking helper to create FileIO.
  MockFileIOClient file_io_client;
  ASSERT_FALSE(cdm_host_proxy_->CreateFileIO(&file_io_client));
  RunUntilIdle();
}

// RequestStorageId() with version 0 (latest) is supported.
TEST_P(CdmAdapterTestWithMockCdm, RequestStorageId_Version_0) {
  CdmConfig cdm_config;
  cdm_config.allow_persistent_state = true;
  InitializeWithCdmConfig(cdm_config);

  std::vector<uint8_t> storage_id = {1, 2, 3};
  EXPECT_CALL(*cdm_helper_, GetStorageIdCalled(0)).WillOnce(Return(storage_id));
  EXPECT_CALL(*mock_library_cdm_, OnStorageId(0, NotNull(), 3));
  cdm_host_proxy_->RequestStorageId(0);
  RunUntilIdle();
}

// RequestStorageId() with version 1 is supported.
TEST_P(CdmAdapterTestWithMockCdm, RequestStorageId_Version_1) {
  CdmConfig cdm_config;
  cdm_config.allow_persistent_state = true;
  InitializeWithCdmConfig(cdm_config);

  std::vector<uint8_t> storage_id = {1, 2, 3};
  EXPECT_CALL(*cdm_helper_, GetStorageIdCalled(1)).WillOnce(Return(storage_id));
  EXPECT_CALL(*mock_library_cdm_, OnStorageId(1, NotNull(), 3));
  cdm_host_proxy_->RequestStorageId(1);
  RunUntilIdle();
}

// RequestStorageId() with version 2 is not supported.
TEST_P(CdmAdapterTestWithMockCdm, RequestStorageId_Version_2) {
  CdmConfig cdm_config;
  cdm_config.allow_persistent_state = true;
  InitializeWithCdmConfig(cdm_config);

  EXPECT_CALL(*mock_library_cdm_, OnStorageId(2, IsNull(), 0));
  cdm_host_proxy_->RequestStorageId(2);
  RunUntilIdle();
}

// RequestStorageId() will always fail if |allow_persistent_state| is false.
TEST_P(CdmAdapterTestWithMockCdm, RequestStorageId_PersistentStateNotAllowed) {
  CdmConfig cdm_config;
  InitializeWithCdmConfig(cdm_config);

  EXPECT_CALL(*mock_library_cdm_, OnStorageId(1, IsNull(), 0));
  cdm_host_proxy_->RequestStorageId(1);
  RunUntilIdle();
}

TEST_P(CdmAdapterTestWithMockCdm, GetDecryptor) {
  CdmConfig cdm_config;
  InitializeWithCdmConfig(cdm_config);
  auto* cdm_context = cdm_->GetCdmContext();
  ASSERT_TRUE(cdm_context);
  EXPECT_TRUE(cdm_context->GetDecryptor());
}

TEST_P(CdmAdapterTestWithMockCdm, GetDecryptor_UseHwSecureCodecs) {
  CdmConfig cdm_config;
  cdm_config.use_hw_secure_codecs = true;
  InitializeWithCdmConfig(cdm_config);
  auto* cdm_context = cdm_->GetCdmContext();
  ASSERT_TRUE(cdm_context);
  EXPECT_FALSE(cdm_context->GetDecryptor());
}

}  // namespace media
