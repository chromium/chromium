// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_D3D11_CDM_PROXY_H_
#define MEDIA_GPU_WINDOWS_D3D11_CDM_PROXY_H_

#include "media/cdm/cdm_proxy.h"

#include <d3d11_1.h>
#include <dxgi1_4.h>
#include <wrl/client.h>

#include <map>
#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "media/base/win/d3d11_create_device_cb.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/windows/d3d11_com_defs.h"

namespace media {

class D3D11CdmContext;

// This is a CdmProxy implementation that uses D3D11.
class MEDIA_GPU_EXPORT D3D11CdmProxy : public CdmProxy {
 public:
  using FunctionIdMap = std::map<Function, uint32_t>;

  // |crypto_type| is the ID that is used to do crypto session operations. This
  // includes creating a crypto session with
  // ID3D11VideoDevice::CreateCryptoSession(). This is "a GUID that specifies
  // the type of encryption to use".
  // https://msdn.microsoft.com/en-us/library/windows/desktop/hh447785(v=vs.85).aspx
  // This is also used ot call
  // ID3D11VideoDevice1::GetCryptoSessionPrivateDataSize(). It "Indicates the
  // crypto type for which the private input and output size is queried."
  // https://msdn.microsoft.com/en-us/library/windows/desktop/dn894143(v=vs.85).aspx
  // |protocol| determines what protocol this is operating in. This
  // value is passed to callbacks that require a protocol enum value.
  // |function_id_map| maps Function enum to an integer.
  D3D11CdmProxy(const GUID& crypto_type,
                CdmProxy::Protocol protocol,
                const FunctionIdMap& function_id_map);
  ~D3D11CdmProxy() override;

  // CdmProxy implementation.
  base::WeakPtr<CdmContext> GetCdmContext() override;
  void Initialize(Client* client, InitializeCB init_cb) override;
  void Process(Function function,
               uint32_t crypto_session_id,
               const std::vector<uint8_t>& input_data,
               uint32_t expected_output_data_size,
               ProcessCB process_cb) override;
  void CreateMediaCryptoSession(
      const std::vector<uint8_t>& input_data,
      CreateMediaCryptoSessionCB create_media_crypto_session_cb) override;
  void SetKey(uint32_t crypto_session_id,
              const std::vector<uint8_t>& key_id,
              KeyType key_type,
              const std::vector<uint8_t>& key_blob,
              SetKeyCB set_key_cb) override;
  void RemoveKey(uint32_t crypto_session_id,
                 const std::vector<uint8_t>& key_id,
                 RemoveKeyCB remove_key_cb) override;

  void SetCreateDeviceCallbackForTesting(D3D11CreateDeviceCB callback);

 private:

  class HardwareEventWatcher;

  void NotifyHardwareContentProtectionTeardown();

  // Reset the state of this instance to be reinitializable.
  void Reset();

  const GUID crypto_type_;
  const CdmProxy::Protocol protocol_;
  const FunctionIdMap function_id_map_;

  std::unique_ptr<D3D11CdmContext> cdm_context_;

  // Implmenenting this class does not require this to be a callback. But in
  // order to inject D3D11CreateDevice() function for testing, this member is
  // required. The test will replace this with a function that returns a mock
  // devices.
  D3D11CreateDeviceCB create_device_func_;

  // Counter for assigning IDs to crypto sessions.
  uint32_t next_crypto_session_id_ = 1;

  // Everything from here until weak ptr factory (which must be at the end)
  // should be reset in Reset().
  Client* client_ = nullptr;
  bool initialized_ = false;

  ComD3D11Device device_;
  ComD3D11DeviceContext device_context_;
  // TODO(crbug.com/788880): Remove ID3D11VideoDevice and ID3D11VideoContext if
  // they are not required.
  ComD3D11VideoDevice video_device_;
  ComD3D11VideoDevice1 video_device1_;
  ComD3D11VideoContext video_context_;
  ComD3D11VideoContext1 video_context1_;

  std::unique_ptr<HardwareEventWatcher> hardware_event_watcher_;

  // Crypto session ID -> actual crypto session.
  std::map<uint32_t, ComD3D11CryptoSession> crypto_session_map_;

  // The values output from ID3D11VideoDevice1::GetCryptoSessionPrivateDataSize.
  // Used when calling NegotiateCryptoSessionKeyExchange.
  UINT private_input_size_ = 0;
  UINT private_output_size_ = 0;

  base::WeakPtrFactory<D3D11CdmProxy> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(D3D11CdmProxy);
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_D3D11_CDM_PROXY_H_
