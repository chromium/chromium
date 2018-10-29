// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_LIBRARY_CDM_CLEAR_KEY_CDM_CDM_PROXY_HANDLER_H_
#define MEDIA_CDM_LIBRARY_CDM_CLEAR_KEY_CDM_CDM_PROXY_HANDLER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "media/cdm/api/content_decryption_module.h"

namespace media {

class CdmHostProxy;

class CdmProxyHandler : public cdm::CdmProxyClient {
 public:
  using InitCB = base::OnceCallback<void(bool success)>;
  using SetKeyCB = base::OnceCallback<void(bool success)>;

  explicit CdmProxyHandler(CdmHostProxy* cdm_host_proxy);
  ~CdmProxyHandler() override;

  // Initializes the CdmProxyHandler and returns the result through |init_cb|.
  // This will request and initialize the CdmProxy, create media crypto session
  // and do some trivial procesing for better test coverage.
  void Initialize(InitCB init_cb);

  // Push a response that contains a license to the CdmProxy.
  void SetKey(const std::vector<uint8_t>& response, SetKeyCB set_key_cb);

 private:
  void FinishInitialization(bool success);

  // cdm::CdmProxyClient implementation.
  void OnInitialized(Status status,
                     Protocol protocol,
                     uint32_t crypto_session_id) final;
  void OnProcessed(Status status,
                   const uint8_t* output_data,
                   uint32_t output_data_size) final;
  void OnMediaCryptoSessionCreated(Status status,
                                   uint32_t crypto_session_id,
                                   uint64_t output_data) final;
  void OnKeySet(Status status) final;
  void OnKeyRemoved(Status status) final;
  void NotifyHardwareReset() final;

  CdmHostProxy* const cdm_host_proxy_ = nullptr;
  InitCB init_cb_;
  SetKeyCB set_key_cb_;
  cdm::CdmProxy* cdm_proxy_ = nullptr;
  uint32_t crypto_session_id_ = 0u;

  DISALLOW_COPY_AND_ASSIGN(CdmProxyHandler);
};

}  // namespace media

#endif  // MEDIA_CDM_LIBRARY_CDM_CLEAR_KEY_CDM_CDM_PROXY_HANDLER_H_
