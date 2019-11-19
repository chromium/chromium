// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_D3D11_DECRYPTOR_H_
#define MEDIA_GPU_WINDOWS_D3D11_DECRYPTOR_H_

#include <wrl/client.h>

#include "base/containers/span.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "media/base/decryptor.h"
#include "media/base/win/d3d11_create_device_cb.h"
#include "media/cdm/cdm_proxy_context.h"
#include "media/gpu/media_gpu_export.h"

namespace media {

class MEDIA_GPU_EXPORT D3D11Decryptor : public Decryptor {
 public:
  explicit D3D11Decryptor(CdmProxyContext* cdm_proxy_context);
  ~D3D11Decryptor() final;

  // Decryptor implementation.
  void RegisterNewKeyCB(StreamType stream_type,
                        const NewKeyCB& key_added_cb) final;
  void Decrypt(StreamType stream_type,
               scoped_refptr<DecoderBuffer> encrypted,
               const DecryptCB& decrypt_cb) final;
  void CancelDecrypt(StreamType stream_type) final;
  void InitializeAudioDecoder(const AudioDecoderConfig& config,
                              const DecoderInitCB& init_cb) final;
  void InitializeVideoDecoder(const VideoDecoderConfig& config,
                              const DecoderInitCB& init_cb) final;
  void DecryptAndDecodeAudio(scoped_refptr<DecoderBuffer> encrypted,
                             const AudioDecodeCB& audio_decode_cb) final;
  void DecryptAndDecodeVideo(scoped_refptr<DecoderBuffer> encrypted,
                             const VideoDecodeCB& video_decode_cb) final;
  void ResetDecoder(StreamType stream_type) final;
  void DeinitializeDecoder(StreamType stream_type) final;

 private:
  // Initialize the buffers for decryption from decryption context.
  bool InitializeDecryptionBuffer(
      const CdmProxyContext::D3D11DecryptContext& decrypt_context);

  // CTR mode decrypts |encrypted| data into |output|. |output| is always
  // cleared. Returns true on success.
  bool CtrDecrypt(base::span<const uint8_t> input,
                  const std::string& iv,
                  const CdmProxyContext::D3D11DecryptContext& context,
                  std::vector<uint8_t>* output);

  // CTR mode decryption method, aware of subsamples. |output| is always
  // cleared. Returns true and populates |output| on success.
  bool SubsampleCtrDecrypt(scoped_refptr<DecoderBuffer> encrypted,
                           const CdmProxyContext::D3D11DecryptContext& context,
                           std::vector<uint8_t>* output);

  CdmProxyContext* cdm_proxy_context_;

  template <class T>
  using ComPtr = Microsoft::WRL::ComPtr<T>;

  // After a successful InitializeDecryptionBuffer() call, these are set for the
  // current Decrypt() call.
  ComPtr<ID3D11Device> device_;
  ComPtr<ID3D11DeviceContext> device_context_;
  ComPtr<ID3D11VideoContext> video_context_;

  // Due to how D3D11 resource permissons work, there are differences between
  // CPU (user) and HW accessible buffers. And things get more complicated with
  // what can read or write from/to it, what combinations are valid, and
  // performance tradeoffs in giving different permissions. The most straight
  // forward way is to use three buffers as described below.

  // A buffer where encrypted data is written by the CPU and is readable by the
  // HW.
  ComPtr<ID3D11Buffer> encrypted_sample_buffer_;

  // A buffer where the decrypted buffer is written by the HW that is not CPU
  // accessible.
  ComPtr<ID3D11Buffer> decrypted_sample_buffer_;

  // A CPU accessible buffer where the content of |decrypted_buffer_| is copied
  // to.
  ComPtr<ID3D11Buffer> cpu_accessible_buffer_;

  base::WeakPtrFactory<D3D11Decryptor> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(D3D11Decryptor);
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_D3D11_DECRYPTOR_H_
