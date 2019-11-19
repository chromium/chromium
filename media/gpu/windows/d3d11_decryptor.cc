// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d11_decryptor.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "media/base/decoder_buffer.h"
#include "media/gpu/windows/d3d11_com_defs.h"

namespace media {

namespace {

// "A buffer is defined as a single subresource."
// https://msdn.microsoft.com/en-us/library/windows/desktop/ff476901(v=vs.85).aspx
const UINT kSubresourceIndex = 0;
const UINT kWaitIfGPUBusy = 0;

// This value is somewhat arbitrary but is a multiple of 16 and 4K and is
// equal to D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION. Since the buffers are cast
// to ID3D11Texture2D, setting it as its size should make sense.
const UINT kBufferSize = 16384;

// Creates ID3D11Buffer using the values. Return true on success.
bool CreateBuffer(ID3D11Device* device,
                  D3D11_USAGE usage,
                  UINT bind_flags,
                  UINT cpu_access,
                  ID3D11Buffer** out) {
  D3D11_BUFFER_DESC buf_desc = {};

  buf_desc.ByteWidth = kBufferSize;
  buf_desc.BindFlags = bind_flags;
  buf_desc.Usage = usage;
  buf_desc.CPUAccessFlags = cpu_access;

  HRESULT hresult = device->CreateBuffer(&buf_desc, nullptr, out);
  return SUCCEEDED(hresult);
}

// Copies |input| into |output|, the output buffer should be a staging buffer
// that is CPU writable.
bool CopyDataToBuffer(base::span<const uint8_t> input,
                      ID3D11DeviceContext* device_context,
                      ID3D11Buffer* output) {
  D3D11_BUFFER_DESC output_buffer_desc = {};
  output->GetDesc(&output_buffer_desc);

  if (output_buffer_desc.ByteWidth < input.size()) {
    DVLOG(1) << input.size() << " does not fit in "
             << output_buffer_desc.ByteWidth;
    return false;
  }

  D3D11_MAPPED_SUBRESOURCE map_resource = {};
  HRESULT hresult =
      device_context->Map(output, kSubresourceIndex, D3D11_MAP_WRITE,
                          kWaitIfGPUBusy, &map_resource);
  if (FAILED(hresult)) {
    DVLOG(3) << "Failed to map buffer for writing.";
    return false;
  }
  memcpy(map_resource.pData, input.data(), input.size_bytes());
  device_context->Unmap(output, kSubresourceIndex);
  return true;
}

// Copies |input| into |output|. The input buffer is should be a staging buffer
// that is CPU readable.
bool CopyDataOutFromBuffer(ID3D11Buffer* input,
                           size_t input_size,
                           ID3D11DeviceContext* device_context,
                           std::vector<uint8_t>* output) {
  D3D11_MAPPED_SUBRESOURCE map_resource = {};
  HRESULT hresult = device_context->Map(
      input, kSubresourceIndex, D3D11_MAP_READ, kWaitIfGPUBusy, &map_resource);
  if (FAILED(hresult)) {
    DVLOG(3) << "Failed to map buffer for reading.";
    return false;
  }
  output->resize(input_size);
  memcpy(output->data(), map_resource.pData, input_size);
  device_context->Unmap(input, kSubresourceIndex);
  return true;
}

D3D11_AES_CTR_IV StringIvToD3D11Iv(const std::string& iv) {
  D3D11_AES_CTR_IV d3d11_iv = {};
  DCHECK_LE(iv.size(), 16u);
  memcpy(&d3d11_iv, iv.data(), iv.size());
  return d3d11_iv;
}

// Returns true if the entire sample is encrypted.
bool IsWholeSampleEncrypted(const DecryptConfig& decrypt_config,
                            size_t sample_size) {
  const auto& subsamples = decrypt_config.subsamples();
  if (subsamples.size() != 1)
    return false;

  return subsamples.front().clear_bytes == 0 &&
         subsamples.front().cypher_bytes == sample_size;
}

// Checks whether |device1| is the same component as |device2|.
// Note that comparing COM pointers require using their IUnknowns.
// https://docs.microsoft.com/en-us/windows/desktop/api/unknwn/nf-unknwn-iunknown-queryinterface(q_)
bool SameDevices(ComD3D11Device device1, ComD3D11Device device2) {
  // For the case where both are nullptrs, they aren't devices, so returning
  // false here.
  if (!device1 || !device2)
    return false;
  Microsoft::WRL::ComPtr<IUnknown> device1_iunknown;
  Microsoft::WRL::ComPtr<IUnknown> device2_iunknown;
  HRESULT hr = device1.CopyTo(device1_iunknown.ReleaseAndGetAddressOf());
  if (FAILED(hr))
    return false;
  hr = device2.CopyTo(device2_iunknown.ReleaseAndGetAddressOf());
  if (FAILED(hr))
    return false;
  return device1_iunknown == device2_iunknown;
}

// Returns a value that is bigger than or equal to |num| that is a
// multiple of 16.
// E.g. num = 15 returns 16, 17 returns 32.
UINT To16Multiple(size_t num) {
  return ((num + 15) >> 4) << 4;
}

}  // namespace

D3D11Decryptor::D3D11Decryptor(CdmProxyContext* cdm_proxy_context)
    : cdm_proxy_context_(cdm_proxy_context) {
  DCHECK(cdm_proxy_context_);
}

D3D11Decryptor::~D3D11Decryptor() {}

void D3D11Decryptor::RegisterNewKeyCB(StreamType stream_type,
                                      const NewKeyCB& new_key_cb) {
  // TODO(crbug.com/821288): Use RegisterNewKeyCB() on CdmContext, and remove
  // RegisterNewKeyCB from Decryptor interface.
  NOTREACHED();
}

void D3D11Decryptor::Decrypt(StreamType stream_type,
                             scoped_refptr<DecoderBuffer> encrypted,
                             const DecryptCB& decrypt_cb) {
  if (encrypted->end_of_stream()) {
    decrypt_cb.Run(kSuccess, encrypted);
    return;
  }

  const auto* decrypt_config = encrypted->decrypt_config();
  if (!decrypt_config) {
    // Not encrypted, nothing to do.
    decrypt_cb.Run(kSuccess, encrypted);
    return;
  }

  if (decrypt_config->HasPattern()) {
    DVLOG(3) << "Cannot handle pattern decryption.";
    decrypt_cb.Run(kError, nullptr);
    return;
  }

  auto context = cdm_proxy_context_->GetD3D11DecryptContext(
      CdmProxy::KeyType::kDecryptOnly, decrypt_config->key_id());
  if (!context) {
    decrypt_cb.Run(kNoKey, nullptr);
    return;
  }

  // Because DecryptionBlt() implementation checks whether the device, buffers,
  // and the crypto session are from the same device, the buffers have to be
  // recreated.
  if (!InitializeDecryptionBuffer(*context)) {
    decrypt_cb.Run(kError, nullptr);
    return;
  }

  std::vector<uint8_t> output;
  if (IsWholeSampleEncrypted(*encrypted->decrypt_config(),
                             encrypted->data_size())) {
    if (!CtrDecrypt(base::make_span(encrypted->data(), encrypted->data_size()),
                    encrypted->decrypt_config()->iv(), *context, &output)) {
      decrypt_cb.Run(kError, nullptr);
      return;
    }
  } else {
    if (!SubsampleCtrDecrypt(encrypted, *context, &output)) {
      decrypt_cb.Run(kError, nullptr);
      return;
    }
  }

  auto decoder_buffer = DecoderBuffer::CopyFrom(output.data(), output.size());
  decoder_buffer->set_timestamp(encrypted->timestamp());
  decoder_buffer->set_duration(encrypted->duration());
  decoder_buffer->set_is_key_frame(encrypted->is_key_frame());
  decoder_buffer->CopySideDataFrom(encrypted->side_data(),
                                   encrypted->side_data_size());
  decrypt_cb.Run(kSuccess, decoder_buffer);
}

void D3D11Decryptor::CancelDecrypt(StreamType stream_type) {
  // Decrypt() calls the DecryptCB synchronously so there's nothing to cancel.
}

void D3D11Decryptor::InitializeAudioDecoder(const AudioDecoderConfig& config,
                                            const DecoderInitCB& init_cb) {
  // D3D11Decryptor does not support audio decoding.
  init_cb.Run(false);
}

void D3D11Decryptor::InitializeVideoDecoder(const VideoDecoderConfig& config,
                                            const DecoderInitCB& init_cb) {
  // D3D11Decryptor does not support video decoding.
  init_cb.Run(false);
}

void D3D11Decryptor::DecryptAndDecodeAudio(
    scoped_refptr<DecoderBuffer> encrypted,
    const AudioDecodeCB& audio_decode_cb) {
  NOTREACHED() << "D3D11Decryptor does not support audio decoding";
}

void D3D11Decryptor::DecryptAndDecodeVideo(
    scoped_refptr<DecoderBuffer> encrypted,
    const VideoDecodeCB& video_decode_cb) {
  NOTREACHED() << "D3D11Decryptor does not support video decoding";
}

void D3D11Decryptor::ResetDecoder(StreamType stream_type) {
  NOTREACHED() << "D3D11Decryptor does not support audio/video decoding";
}

void D3D11Decryptor::DeinitializeDecoder(StreamType stream_type) {
  // D3D11Decryptor does not support audio/video decoding, but since this can be
  // called any time after InitializeAudioDecoder/InitializeVideoDecoder,
  // nothing to be done here.
}

bool D3D11Decryptor::InitializeDecryptionBuffer(
    const CdmProxyContext::D3D11DecryptContext& decrypt_context) {
  ComPtr<ID3D11Device> crypto_session_device;
  decrypt_context.crypto_session->GetDevice(
      crypto_session_device.ReleaseAndGetAddressOf());

  // If they are the same devices, then there is no reason to reinitialize the
  // buffers.
  if (SameDevices(crypto_session_device, device_))
    return true;

  device_ = crypto_session_device;
  device_->GetImmediateContext(device_context_.ReleaseAndGetAddressOf());

  HRESULT hresult =
      device_context_.CopyTo(video_context_.ReleaseAndGetAddressOf());
  if (FAILED(hresult)) {
    DVLOG(2) << "Failed to get video context.";
    return false;
  }

  // The buffer is staging so that the data can be accessed by the CPU and HW.
  if (!CreateBuffer(device_.Get(), D3D11_USAGE_STAGING, 0,  // no binding.
                    D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE,
                    encrypted_sample_buffer_.ReleaseAndGetAddressOf())) {
    DVLOG(2) << "Failed to create buffer for encrypted sample.";
    return false;
  }

  // Note that the cpu access flag is 0 because this buffer is used to write the
  // decrypted buffer in HW.
  if (!CreateBuffer(device_.Get(), D3D11_USAGE_DEFAULT,
                    D3D11_BIND_RENDER_TARGET,
                    0,  // no cpu access.
                    decrypted_sample_buffer_.ReleaseAndGetAddressOf())) {
    DVLOG(2) << "Failed to create buffer for decrypted sample.";
    return false;
  }

  if (!CreateBuffer(device_.Get(), D3D11_USAGE_STAGING, 0,  // no binding.
                    D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE,
                    cpu_accessible_buffer_.ReleaseAndGetAddressOf())) {
    DVLOG(2) << "Failed to create cpu accessible buffer.";
    return false;
  }

  return true;
}

bool D3D11Decryptor::CtrDecrypt(
    base::span<const uint8_t> input,
    const std::string& iv,
    const CdmProxyContext::D3D11DecryptContext& context,
    std::vector<uint8_t>* output) {
  output->clear();
  if (input.empty())
    return true;

  if (!CopyDataToBuffer(input, device_context_.Get(),
                        encrypted_sample_buffer_.Get())) {
    return false;
  }

  D3D11_AES_CTR_IV aes_ctr_iv = StringIvToD3D11Iv(iv);

  // The size of the encrypted bytes must be a multiple of 16. See more at
  // https://crbug.com/849466.
  D3D11_ENCRYPTED_BLOCK_INFO block_info = {};
  block_info.NumEncryptedBytesAtBeginning = To16Multiple(input.size());
  DCHECK_LE(block_info.NumEncryptedBytesAtBeginning, kBufferSize);
  block_info.NumBytesInSkipPattern =
      kBufferSize - block_info.NumEncryptedBytesAtBeginning;

  // ID3D11Buffers should be used but since the interface takes ID3D11Texture2D,
  // it is reinterpret cast. See more at https://crbug.com/849466.
  video_context_->DecryptionBlt(
      context.crypto_session,
      reinterpret_cast<ID3D11Texture2D*>(encrypted_sample_buffer_.Get()),
      reinterpret_cast<ID3D11Texture2D*>(decrypted_sample_buffer_.Get()),
      &block_info, context.key_blob_size, context.key_blob, sizeof(aes_ctr_iv),
      &aes_ctr_iv);

  // Because DecryptionBlt() doesn't have a return value, this is a hack to
  // check for decryption operation status. If it has been modified, then there
  // was an error. See more at https://crbug.com/849466.
  HRESULT result = static_cast<HRESULT>(block_info.NumBytesInEncryptPattern);
  if (FAILED(result)) {
    DVLOG(3) << "Decryption error :"
             << logging::SystemErrorCodeToString(result);
    return false;
  }

  device_context_->CopyResource(cpu_accessible_buffer_.Get(),
                                decrypted_sample_buffer_.Get());
  return CopyDataOutFromBuffer(cpu_accessible_buffer_.Get(), input.size(),
                               device_context_.Get(), output);
}

// TODO(crbug.com/845631): This is the same as DecryptCencBuffer(), so it should
// be deduped.
bool D3D11Decryptor::SubsampleCtrDecrypt(
    scoped_refptr<DecoderBuffer> encrypted,
    const CdmProxyContext::D3D11DecryptContext& context,
    std::vector<uint8_t>* output) {
  const auto& subsamples = encrypted->decrypt_config()->subsamples();
  std::vector<uint8_t> encrypted_data;
  const uint8_t* data = encrypted->data();
  for (const auto& subsample : subsamples) {
    data += subsample.clear_bytes;
    encrypted_data.insert(encrypted_data.end(), data,
                          data + subsample.cypher_bytes);
    data += subsample.cypher_bytes;
  }

  std::vector<uint8_t> decrypted_data;
  if (!CtrDecrypt(encrypted_data, encrypted->decrypt_config()->iv(), context,
                  &decrypted_data)) {
    return false;
  }

  data = encrypted->data();
  const uint8_t* decrypted_data_ptr = decrypted_data.data();
  for (const auto& subsample : subsamples) {
    output->insert(output->end(), data, data + subsample.clear_bytes);
    data += subsample.clear_bytes;
    output->insert(output->end(), decrypted_data_ptr,
                   decrypted_data_ptr + subsample.cypher_bytes);
    decrypted_data_ptr += subsample.cypher_bytes;
    data += subsample.cypher_bytes;
  }
  return true;
}

}  // namespace media
