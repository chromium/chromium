// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/win/test/media_foundation_clear_key_trusted_input.h"

#include <mfapi.h>
#include <wrl/client.h>
#include <wrl/implements.h>
#include <wrl/module.h>
#include <utility>

#include "media/base/win/mf_helpers.h"
#include "media/cdm/win/test/media_foundation_clear_key_guids.h"
#include "media/cdm/win/test/media_foundation_clear_key_input_trust_authority.h"

namespace media {

using Microsoft::WRL::ComPtr;
using Microsoft::WRL::MakeAndInitialize;

MediaFoundationClearKeyTrustedInput::MediaFoundationClearKeyTrustedInput() =
    default;

MediaFoundationClearKeyTrustedInput::~MediaFoundationClearKeyTrustedInput() {
  DVLOG_FUNC(1);
}

HRESULT MediaFoundationClearKeyTrustedInput::RuntimeClassInitialize(
    _In_ scoped_refptr<AesDecryptor> aes_decryptor) {
  DVLOG_FUNC(1);
  aes_decryptor_ = std::move(aes_decryptor);
  return S_OK;
}

// IMFTrustedInput
STDMETHODIMP MediaFoundationClearKeyTrustedInput::GetInputTrustAuthority(
    _In_ DWORD stream_id,
    _In_ REFIID riid,
    _COM_Outptr_ IUnknown** authority) {
  DVLOG_FUNC(1);
  ComPtr<IMFInputTrustAuthority> ita;
  RETURN_IF_FAILED(
      (MakeAndInitialize<MediaFoundationClearKeyInputTrustAuthority,
                         IMFInputTrustAuthority>(&ita, stream_id,
                                                 aes_decryptor_)));

  *authority = ita.Detach();

  return S_OK;
}

}  // namespace media
