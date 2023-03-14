// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_WIN_TEST_MEDIA_FOUNDATION_CLEAR_KEY_INPUT_TRUST_AUTHORITY_H_
#define MEDIA_CDM_WIN_TEST_MEDIA_FOUNDATION_CLEAR_KEY_INPUT_TRUST_AUTHORITY_H_

#include <mferror.h>
#include <mfidl.h>
#include <wrl/implements.h>

#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "media/cdm/aes_decryptor.h"

namespace media {

class MediaFoundationClearKeyInputTrustAuthority final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IMFInputTrustAuthority,
          IMFShutdown,
          Microsoft::WRL::FtmBase> {
 public:
  MediaFoundationClearKeyInputTrustAuthority();
  ~MediaFoundationClearKeyInputTrustAuthority() override;
  MediaFoundationClearKeyInputTrustAuthority(
      const MediaFoundationClearKeyInputTrustAuthority&) = delete;
  MediaFoundationClearKeyInputTrustAuthority& operator=(
      const MediaFoundationClearKeyInputTrustAuthority&) = delete;

  HRESULT RuntimeClassInitialize(
      _In_ UINT32 stream_id,
      _In_ scoped_refptr<AesDecryptor> aes_decryptor);

  // IMFInputTrustAuthority
  STDMETHODIMP GetDecrypter(_In_ REFIID riid, _COM_Outptr_ void** ppv) override;
  STDMETHODIMP RequestAccess(
      _In_ MFPOLICYMANAGER_ACTION action,
      _COM_Outptr_ IMFActivate** content_enabler_activate) override;
  STDMETHODIMP GetPolicy(_In_ MFPOLICYMANAGER_ACTION action,
                         _COM_Outptr_ IMFOutputPolicy** policy) override;
  STDMETHODIMP BindAccess(
      _In_ MFINPUTTRUSTAUTHORITY_ACCESS_PARAMS* params) override;
  STDMETHODIMP UpdateAccess(
      _In_ MFINPUTTRUSTAUTHORITY_ACCESS_PARAMS* params) override;
  STDMETHODIMP Reset() override;

  // IMFShutdown
  STDMETHODIMP GetShutdownStatus(_Out_ MFSHUTDOWN_STATUS* status) override;
  STDMETHODIMP Shutdown() override;

 private:
  HRESULT GetShutdownStatus() {
    base::AutoLock lock(lock_);
    return (is_shutdown_) ? MF_E_SHUTDOWN : S_OK;
  }

  scoped_refptr<AesDecryptor> aes_decryptor_;

  // For IMFShutdown
  base::Lock lock_;
  bool is_shutdown_ GUARDED_BY(lock_) = false;
};

}  // namespace media

#endif  // MEDIA_CDM_WIN_TEST_MEDIA_FOUNDATION_CLEAR_KEY_INPUT_TRUST_AUTHORITY_H_
