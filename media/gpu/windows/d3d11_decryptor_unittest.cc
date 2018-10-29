// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d11_decryptor.h"

#include <initguid.h>

#include <array>

#include "base/bind.h"
#include "base/containers/span.h"
#include "base/stl_util.h"
#include "media/base/decoder_buffer.h"
#include "media/base/subsample_entry.h"
#include "media/cdm/cdm_proxy_context.h"
#include "media/gpu/windows/d3d11_mocks.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::DoAll;
using ::testing::ElementsAreArray;
using ::testing::Invoke;
using ::testing::IsNull;
using ::testing::Mock;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SetArgPointee;

template <class T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

namespace media {

namespace {
// clang-format off
// The value doesn't matter this is just a GUID.
DEFINE_GUID(TEST_GUID,
            0x01020304, 0xffee, 0xefba,
            0x93, 0xaa, 0x47, 0x77, 0x43, 0xb1, 0x22, 0x98);
// clang-format on

// Should be non-0 so that it's different from the default TimeDelta.
constexpr base::TimeDelta kTestTimestamp =
    base::TimeDelta::FromMilliseconds(33);

const uint8_t kAnyKeyBlob[] = {3, 5, 38, 19};
const char kKeyId[] = "some 16 byte id.";
const char kIv[] = "some 16 byte iv.";

// For tests where the input doesn't matter.
const uint8_t kAnyInput[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
};

const SubsampleEntry kAnyInputSubsample(0, base::size(kAnyInput));

scoped_refptr<DecoderBuffer> TestDecoderBuffer(
    const uint8_t* input,
    size_t data_size,
    const std::string& key_id,
    const std::string& iv,
    const std::vector<SubsampleEntry>& subsamples) {
  scoped_refptr<DecoderBuffer> encrypted_buffer =
      DecoderBuffer::CopyFrom(input, data_size);

  encrypted_buffer->set_decrypt_config(
      DecryptConfig::CreateCencConfig(key_id, iv, subsamples));
  encrypted_buffer->set_timestamp(kTestTimestamp);
  return encrypted_buffer;
}

CdmProxyContext::D3D11DecryptContext TestDecryptContext(
    ComPtr<D3D11CryptoSessionMock> crypto_session_mock) {
  CdmProxyContext::D3D11DecryptContext decrypt_context = {};
  decrypt_context.crypto_session = crypto_session_mock.Get();
  decrypt_context.key_blob = kAnyKeyBlob;
  decrypt_context.key_blob_size = base::size(kAnyKeyBlob);
  decrypt_context.key_info_guid = TEST_GUID;
  return decrypt_context;
}

class CallbackMock {
 public:
  MOCK_METHOD2(DecryptCallback, Decryptor::DecryptCB::RunType);
};

class CdmProxyContextMock : public CdmProxyContext {
 public:
  MOCK_METHOD2(GetD3D11DecryptContext,
               base::Optional<D3D11DecryptContext>(CdmProxy::KeyType key_type,
                                                   const std::string& key_id));
};

// Checks that BUFFER_DESC has these fields match.
// Flags are ORed values, so this only checks that the expected flags are set.
// The other fields are ignored.
MATCHER_P3(BufferDescHas, usage, bind_flags, cpu_access, "") {
  const D3D11_BUFFER_DESC& buffer_desc = *arg;
  if (buffer_desc.Usage != usage)
    return false;

  // Because the flags are enums the compiler infers that the input flags are
  // signed ints. And the compiler rejects comparing signed int and unsigned
  // int, so they are cast here.
  const UINT unsigned_bind_flags = bind_flags;
  const UINT unsigned_cpu_access_flags = cpu_access;

  if ((buffer_desc.BindFlags & unsigned_bind_flags) != unsigned_bind_flags)
    return false;

  return (buffer_desc.CPUAccessFlags & unsigned_cpu_access_flags) ==
         unsigned_cpu_access_flags;
}

// NumEncryptedBytesAtBeginning must be greater than or equal to the size of the
// encrypted data, and also a multiple of 16.
MATCHER_P(NumEncryptedBytesAtBeginningGreaterOrEq, value, "") {
  const D3D11_ENCRYPTED_BLOCK_INFO& block_info = *arg;
  if (block_info.NumEncryptedBytesAtBeginning < value) {
    *result_listener << block_info.NumEncryptedBytesAtBeginning
                     << " is not less than " << value;
    return false;
  }
  return block_info.NumEncryptedBytesAtBeginning % 16 == 0;
}

ACTION_P(SetBufferDescSize, size) {
  arg0->ByteWidth = size;
}

MATCHER_P2(OutputDataEquals, data, size, "") {
  scoped_refptr<DecoderBuffer> buffer = arg;
  if (size != buffer->data_size()) {
    return false;
  }
  if (buffer->timestamp() != kTestTimestamp) {
    return false;
  }

  std::vector<uint8_t> expected(data, data + size);
  std::vector<uint8_t> actual(buffer->data(),
                              buffer->data() + buffer->data_size());
  return actual == expected;
}

}  // namespace

class D3D11DecryptorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    decryptor_ = std::make_unique<D3D11Decryptor>(&mock_proxy_);

    device_mock_ = CreateD3D11Mock<D3D11DeviceMock>();
    device_context_mock_ = CreateD3D11Mock<D3D11DeviceContextMock>();
    video_context_mock_ = CreateD3D11Mock<D3D11VideoContextMock>();
    staging_buffer1_ = CreateD3D11Mock<D3D11BufferMock>();
    staging_buffer2_ = CreateD3D11Mock<D3D11BufferMock>();
    gpu_buffer_ = CreateD3D11Mock<D3D11BufferMock>();
  }

  // Only sets mock expectations to the objects. Use this for the case where the
  // buffers are expected to be created from |device_mock_|, that's accessible
  // through |crypto_session_mock|'s GetDevice() function.
  void SetExpectationsForSuccessfulBufferInitialization(
      D3D11CryptoSessionMock* crypto_session_mock,
      CdmProxyContext::D3D11DecryptContext* decrypt_context) {
    // As noted in the function comment, the device is accessible from the
    // crypto session.
    EXPECT_CALL(*crypto_session_mock, GetDevice(_))
        .Times(AtLeast(1))
        .WillRepeatedly(AddRefAndSetArgPointee<0>(device_mock_.Get()));

    // The other components accessible (directly or indirectly) from the device.
    EXPECT_CALL(*device_mock_.Get(), GetImmediateContext(_))
        .Times(AtLeast(1))
        .WillRepeatedly(AddRefAndSetArgPointee<0>(device_context_mock_.Get()));
    EXPECT_CALL(*device_context_mock_.Get(),
                QueryInterface(IID_ID3D11VideoContext, _))
        .Times(AtLeast(1))
        .WillRepeatedly(
            DoAll(AddRefAndSetArgPointee<1>(video_context_mock_.Get()),
                  Return(S_OK)));

    EXPECT_CALL(mock_proxy_,
                GetD3D11DecryptContext(CdmProxy::KeyType::kDecryptOnly, kKeyId))
        .WillOnce(Return(*decrypt_context));

    // These return big enough size.
    ON_CALL(*staging_buffer1_.Get(), GetDesc(_))
        .WillByDefault(SetBufferDescSize(20000));
    ON_CALL(*staging_buffer2_.Get(), GetDesc(_))
        .WillByDefault(SetBufferDescSize(20000));
    ON_CALL(*gpu_buffer_.Get(), GetDesc(_))
        .WillByDefault(SetBufferDescSize(20000));

    // It should be requesting for 2 staging buffers one for writing the data to
    // a GPU buffer and one for reading from the a GPU buffer.
    EXPECT_CALL(*device_mock_.Get(),
                CreateBuffer(BufferDescHas(D3D11_USAGE_STAGING, 0u,
                                           D3D11_CPU_ACCESS_READ |
                                               D3D11_CPU_ACCESS_WRITE),
                             nullptr, _))
        .WillOnce(DoAll(AddRefAndSetArgPointee<2>(staging_buffer1_.Get()),
                        Return(S_OK)))
        .WillOnce(DoAll(AddRefAndSetArgPointee<2>(staging_buffer2_.Get()),
                        Return(S_OK)));

    // It should be requesting a GPU only accessible buffer to the decrypted
    // output.
    EXPECT_CALL(*device_mock_.Get(),
                CreateBuffer(BufferDescHas(D3D11_USAGE_DEFAULT,
                                           D3D11_BIND_RENDER_TARGET, 0u),
                             nullptr, _))
        .WillOnce(
            DoAll(AddRefAndSetArgPointee<2>(gpu_buffer_.Get()), Return(S_OK)));
  }

  // |input| is the input to the Decrypt() function, the subsample information
  // is in |subsamples| therefore |input| may not be entirely encrypted. The
  // data that is encrypted in |input| should be |encrypted_input|.
  // |whole_output| is the expected output from the Decrypt() call, reported by
  // the callback. The decrypted result of |encrypted_input| should be
  // |decrypted_output|.
  void ExpectSuccessfulDecryption(D3D11CryptoSessionMock* crypto_session_mock,
                                  base::span<const uint8_t> input,
                                  base::span<const uint8_t> encrypted_input,
                                  base::span<const uint8_t> whole_output,
                                  base::span<const uint8_t> decrypted_output,
                                  const std::vector<SubsampleEntry>& subsamples,
                                  D3D11Decryptor* decryptor) {
    D3D11_MAPPED_SUBRESOURCE staging_buffer1_subresource = {};
    auto staging_buffer1_subresource_buffer =
        std::make_unique<uint8_t[]>(20000);
    staging_buffer1_subresource.pData =
        staging_buffer1_subresource_buffer.get();

    // It should be requesting for a memory mapped buffer, from the staging
    // buffer, to pass the encrypted data to the GPU.
    EXPECT_CALL(*device_context_mock_.Get(),
                Map(staging_buffer1_.Get(), 0, D3D11_MAP_WRITE, _, _))
        .WillOnce(
            DoAll(SetArgPointee<4>(staging_buffer1_subresource), Return(S_OK)));
    EXPECT_CALL(*device_context_mock_.Get(), Unmap(staging_buffer1_.Get(), 0));

    EXPECT_CALL(
        *video_context_mock_.Get(),
        DecryptionBlt(
            crypto_session_mock,
            reinterpret_cast<ID3D11Texture2D*>(staging_buffer1_.Get()),
            reinterpret_cast<ID3D11Texture2D*>(gpu_buffer_.Get()),
            NumEncryptedBytesAtBeginningGreaterOrEq(encrypted_input.size()),
            sizeof(kAnyKeyBlob), kAnyKeyBlob, _, _));
    EXPECT_CALL(*device_context_mock_.Get(),
                CopyResource(staging_buffer2_.Get(), gpu_buffer_.Get()));

    D3D11_MAPPED_SUBRESOURCE staging_buffer2_subresource = {};

    // pData field is non-const void* so make a copy of kFakeDecryptedData that
    // can be cast to void*.
    std::unique_ptr<uint8_t[]> decrypted_data =
        std::make_unique<uint8_t[]>(decrypted_output.size());
    memcpy(decrypted_data.get(), decrypted_output.data(),
           decrypted_output.size());
    staging_buffer2_subresource.pData = decrypted_data.get();

    // Tt should be requesting for a memory mapped buffer, from the staging
    // buffer, to read the decrypted data out from the GPU buffer.
    EXPECT_CALL(*device_context_mock_.Get(),
                Map(staging_buffer2_.Get(), 0, D3D11_MAP_READ, _, _))
        .WillOnce(
            DoAll(SetArgPointee<4>(staging_buffer2_subresource), Return(S_OK)));
    EXPECT_CALL(*device_context_mock_.Get(), Unmap(staging_buffer2_.Get(), 0));

    CallbackMock callbacks;
    EXPECT_CALL(callbacks,
                DecryptCallback(Decryptor::kSuccess,
                                OutputDataEquals(whole_output.data(),
                                                 whole_output.size())));

    scoped_refptr<DecoderBuffer> encrypted_buffer =
        TestDecoderBuffer(input.data(), input.size(), kKeyId, kIv, subsamples);
    decryptor->Decrypt(Decryptor::kAudio, encrypted_buffer,
                       base::BindRepeating(&CallbackMock::DecryptCallback,
                                           base::Unretained(&callbacks)));

    // Verify that the data copied to the GPU buffer is the encrypted portion.
    EXPECT_TRUE(std::equal(encrypted_input.begin(), encrypted_input.end(),
                           staging_buffer1_subresource_buffer.get()));
  }

  std::unique_ptr<D3D11Decryptor> decryptor_;
  CdmProxyContextMock mock_proxy_;

  ComPtr<D3D11DeviceMock> device_mock_;
  ComPtr<D3D11DeviceContextMock> device_context_mock_;
  ComPtr<D3D11VideoContextMock> video_context_mock_;

 private:
  ComPtr<D3D11BufferMock> staging_buffer1_;
  ComPtr<D3D11BufferMock> staging_buffer2_;
  ComPtr<D3D11BufferMock> gpu_buffer_;
};

// Verify that full sample encrypted sample works.
TEST_F(D3D11DecryptorTest, FullSampleCtrDecrypt) {
  const uint8_t kInput[] = {
      0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
  };
  const SubsampleEntry kSubsample(0, base::size(kInput));
  // This is arbitrary. Just used to check that this value is output from the
  // method.
  const uint8_t kFakeDecryptedData[] = {
      15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
  };
  static_assert(base::size(kFakeDecryptedData) == base::size(kInput),
                "Fake input and output data size must match.");

  ComPtr<D3D11CryptoSessionMock> crypto_session_mock =
      CreateD3D11Mock<D3D11CryptoSessionMock>();
  CdmProxyContext::D3D11DecryptContext decrypt_context =
      TestDecryptContext(crypto_session_mock);
  SetExpectationsForSuccessfulBufferInitialization(crypto_session_mock.Get(),
                                                   &decrypt_context);

  // The entire sample is encrypted so the encrypted/decrypted portions are the
  // input/output.
  ExpectSuccessfulDecryption(crypto_session_mock.Get(), kInput, kInput,
                             kFakeDecryptedData, kFakeDecryptedData,
                             {kSubsample}, decryptor_.get());
}

// Verify that it works for encrypted input that's not a multiple of 16.
TEST_F(D3D11DecryptorTest, InputSizeNotMultipleOf16) {
  const uint8_t kInput[] = {
      0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
  };
  const SubsampleEntry kSubsample(0, base::size(kInput));
  // This is arbitrary. Just used to check that this value is output from the
  // method.
  const uint8_t kFakeDecryptedData[] = {
      20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
  };
  static_assert(base::size(kFakeDecryptedData) == base::size(kInput),
                "Fake input and output data size must match.");

  ComPtr<D3D11CryptoSessionMock> crypto_session_mock =
      CreateD3D11Mock<D3D11CryptoSessionMock>();
  CdmProxyContext::D3D11DecryptContext decrypt_context =
      TestDecryptContext(crypto_session_mock);

  SetExpectationsForSuccessfulBufferInitialization(crypto_session_mock.Get(),
                                                   &decrypt_context);

  // The entire sample is encrypted so the encrypted/decrypted portions are the
  // input/output.
  ExpectSuccessfulDecryption(crypto_session_mock.Get(), kInput, kInput,
                             kFakeDecryptedData, kFakeDecryptedData,
                             {kSubsample}, decryptor_.get());
}

// Verify subsample decryption works.
TEST_F(D3D11DecryptorTest, SubsampleCtrDecrypt) {
  // clang-format off
  const uint8_t kInput[] = {
      // clear 16 bytes.
      0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
      // encrypted 16 bytes.
      0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
      // clear 5 bytes.
      0, 1, 2, 3, 4,
      // encrypted 16 bytes.
      0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
  };
  // Encrypted parts of the input
  const uint8_t kInputEncrypted[] = {
      0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
      0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
  };
  // This is arbitrary. Just used to check that this value is output from the
  // method.
  const uint8_t kFakeOutputData[] = {
      // clear 16 bytes.
      0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
      // decrypted 16 bytes.
      15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
      // clear 5 bytes.
      0, 1, 2, 3, 4,
      // decrypted 16 bytes.
      15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
  };
  const uint8_t kFakeDecryptedData[] = {
      15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
      15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
  };
  // clang-format on
  static_assert(base::size(kFakeOutputData) == base::size(kInput),
                "Fake input and output data size must match.");
  const std::vector<SubsampleEntry> subsamples = {SubsampleEntry(16, 16),
                                                  SubsampleEntry(5, 16)};

  ComPtr<D3D11CryptoSessionMock> crypto_session_mock =
      CreateD3D11Mock<D3D11CryptoSessionMock>();
  CdmProxyContext::D3D11DecryptContext decrypt_context =
      TestDecryptContext(crypto_session_mock);

  SetExpectationsForSuccessfulBufferInitialization(crypto_session_mock.Get(),
                                                   &decrypt_context);
  ExpectSuccessfulDecryption(crypto_session_mock.Get(), kInput, kInputEncrypted,
                             kFakeOutputData, kFakeDecryptedData, subsamples,
                             decryptor_.get());
}

// Verify that if the input is too big, it fails. This may be removed if the
// implementation supports big input.
TEST_F(D3D11DecryptorTest, DecryptInputTooBig) {
  // Something pretty big to be an audio frame. The actual data size doesn't
  // matter.
  std::array<uint8_t, 1000000> kInput;
  const SubsampleEntry kSubsample(0, base::size(kInput));

  ComPtr<D3D11CryptoSessionMock> crypto_session_mock =
      CreateD3D11Mock<D3D11CryptoSessionMock>();
  CdmProxyContext::D3D11DecryptContext decrypt_context =
      TestDecryptContext(crypto_session_mock);

  SetExpectationsForSuccessfulBufferInitialization(crypto_session_mock.Get(),
                                                   &decrypt_context);
  CallbackMock callbacks;
  EXPECT_CALL(callbacks, DecryptCallback(Decryptor::kError, IsNull()));

  scoped_refptr<DecoderBuffer> encrypted_buffer = TestDecoderBuffer(
      kInput.data(), base::size(kInput), kKeyId, kIv, {kSubsample});
  decryptor_->Decrypt(Decryptor::kAudio, encrypted_buffer,
                      base::BindRepeating(&CallbackMock::DecryptCallback,
                                          base::Unretained(&callbacks)));
}

// If there is no decrypt config, it must be in the clear, so it shouldn't
// change the output.
TEST_F(D3D11DecryptorTest, NoDecryptConfig) {
  scoped_refptr<DecoderBuffer> clear_buffer =
      DecoderBuffer::CopyFrom(kAnyInput, base::size(kAnyInput));
  clear_buffer->set_timestamp(kTestTimestamp);
  CallbackMock callbacks;
  EXPECT_CALL(
      callbacks,
      DecryptCallback(Decryptor::kSuccess,
                      OutputDataEquals(kAnyInput, base::size(kAnyInput))));
  decryptor_->Decrypt(Decryptor::kAudio, clear_buffer,
                      base::BindRepeating(&CallbackMock::DecryptCallback,
                                          base::Unretained(&callbacks)));
}

// The current decryptor cannot deal with pattern encryption.
TEST_F(D3D11DecryptorTest, PatternDecryption) {
  scoped_refptr<DecoderBuffer> encrypted_buffer =
      DecoderBuffer::CopyFrom(kAnyInput, base::size(kAnyInput));
  encrypted_buffer->set_decrypt_config(DecryptConfig::CreateCbcsConfig(
      kKeyId, kIv, {kAnyInputSubsample}, EncryptionPattern(1, 9)));

  CallbackMock callbacks;
  EXPECT_CALL(callbacks, DecryptCallback(Decryptor::kError, IsNull()));
  decryptor_->Decrypt(Decryptor::kAudio, encrypted_buffer,
                      base::BindRepeating(&CallbackMock::DecryptCallback,
                                          base::Unretained(&callbacks)));
}

// If there is no decrypt context, it's missing a key.
TEST_F(D3D11DecryptorTest, NoDecryptContext) {
  scoped_refptr<DecoderBuffer> encrypted_buffer = TestDecoderBuffer(
      kAnyInput, base::size(kAnyInput), kKeyId, kIv, {kAnyInputSubsample});

  EXPECT_CALL(mock_proxy_,
              GetD3D11DecryptContext(CdmProxy::KeyType::kDecryptOnly, kKeyId))
      .WillOnce(Return(base::nullopt));

  CallbackMock callbacks;
  EXPECT_CALL(callbacks, DecryptCallback(Decryptor::kNoKey, IsNull()));
  decryptor_->Decrypt(Decryptor::kAudio, encrypted_buffer,
                      base::BindRepeating(&CallbackMock::DecryptCallback,
                                          base::Unretained(&callbacks)));
}

// Verify that if the crypto session's device is the same as the previous call,
// the buffers aren't recreated.
TEST_F(D3D11DecryptorTest, ReuseBuffers) {
  ComPtr<D3D11CryptoSessionMock> crypto_session_mock =
      CreateD3D11Mock<D3D11CryptoSessionMock>();
  CdmProxyContext::D3D11DecryptContext decrypt_context =
      TestDecryptContext(crypto_session_mock);

  SetExpectationsForSuccessfulBufferInitialization(crypto_session_mock.Get(),
                                                   &decrypt_context);

  // This test doesn't require checking the output or the correctness of the
  // decyrption, so just pass the input buffer for output.
  ExpectSuccessfulDecryption(crypto_session_mock.Get(), kAnyInput, kAnyInput,
                             kAnyInput, kAnyInput, {kAnyInputSubsample},
                             decryptor_.get());
  Mock::VerifyAndClearExpectations(crypto_session_mock.Get());
  Mock::VerifyAndClearExpectations(device_mock_.Get());
  Mock::VerifyAndClearExpectations(video_context_mock_.Get());
  Mock::VerifyAndClearExpectations(device_context_mock_.Get());
  Mock::VerifyAndClearExpectations(&mock_proxy_);

  EXPECT_CALL(*crypto_session_mock.Get(), GetDevice(_))
      .Times(AtLeast(1))
      .WillRepeatedly(AddRefAndSetArgPointee<0>(device_mock_.Get()));
  EXPECT_CALL(mock_proxy_,
              GetD3D11DecryptContext(CdmProxy::KeyType::kDecryptOnly, kKeyId))
      .WillOnce(Return(decrypt_context));

  // Buffers should not be (re)initialized on the next call to decrypt because
  // it's the same device as the previous call.
  EXPECT_CALL(*device_mock_.Get(), CreateBuffer(_, _, _)).Times(0);

  // This calls Decrypt() so that the expectations above are triggered.
  ExpectSuccessfulDecryption(crypto_session_mock.Get(), kAnyInput, kAnyInput,
                             kAnyInput, kAnyInput, {kAnyInputSubsample},
                             decryptor_.get());
}

}  // namespace media
