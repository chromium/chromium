// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_WIN_TEST_MEDIA_FOUNDATION_CLEAR_KEY_CDM_FACTORY_H_
#define MEDIA_CDM_WIN_TEST_MEDIA_FOUNDATION_CLEAR_KEY_CDM_FACTORY_H_

#include <mfcontentdecryptionmodule.h>
#include <mfidl.h>
#include <wrl/implements.h>

#include "base/threading/thread_checker.h"

namespace media {

class MediaFoundationClearKeyCdmFactory final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::WinRtClassicComMix>,
          Microsoft::WRL::CloakedIid<IMFContentDecryptionModuleFactory>,
          Microsoft::WRL::FtmBase> {
  InspectableClass(
      L"org.chromium.externalclearkey.mediafoundation."
      L"ContentDecryptionModuleFactory",
      BaseTrust)

      public : MediaFoundationClearKeyCdmFactory();
  ~MediaFoundationClearKeyCdmFactory() override;
  MediaFoundationClearKeyCdmFactory(const MediaFoundationClearKeyCdmFactory&) =
      delete;
  MediaFoundationClearKeyCdmFactory& operator=(
      const MediaFoundationClearKeyCdmFactory&) = delete;

  HRESULT RuntimeClassInitialize();

  // IMFContentDecryptionModuleFactory
  STDMETHODIMP_(BOOL)
  IsTypeSupported(_In_ LPCWSTR key_system,
                  _In_opt_ LPCWSTR content_type) override;
  STDMETHODIMP CreateContentDecryptionModuleAccess(
      _In_ LPCWSTR key_system,
      _In_count_(num_configurations) IPropertyStore** configurations,
      _In_ DWORD num_configurations,
      _COM_Outptr_ IMFContentDecryptionModuleAccess** cdm_access) override;

 private:
  // Thread checker to enforce that this object is used on a specific thread.
  THREAD_CHECKER(thread_checker_);
};

}  // namespace media

#endif  // MEDIA_CDM_WIN_TEST_MEDIA_FOUNDATION_CLEAR_KEY_CDM_FACTORY_H_
