// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_RENDERERS_WIN_MEDIA_FOUNDATION_PROTECTION_MANAGER_H_
#define MEDIA_RENDERERS_WIN_MEDIA_FOUNDATION_PROTECTION_MANAGER_H_

#include <mfapi.h>
#include <mfidl.h>
#include <windows.media.protection.h>
#include <wrl.h>

#include "base/memory/scoped_refptr.h"
#include "media/base/win/media_foundation_cdm_proxy.h"

namespace media {

// Implements IMFContentProtectionManager
// (https://docs.microsoft.com/en-us/windows/win32/api/mfidl/nn-mfidl-imfcontentprotectionmanager)
// and ABI::Windows::Media::Protection::IMediaProtectionManager
// (https://docs.microsoft.com/en-us/uwp/api/windows.media.protection.mediaprotectionmanager)
// required by IMFMediaEngineProtectedContent::SetContentProtectionManager in
// https://docs.microsoft.com/en-us/windows/win32/api/mfmediaengine/nf-mfmediaengine-imfmediaengineprotectedcontent-setcontentprotectionmanager.
//
class MediaFoundationProtectionManager
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::RuntimeClassType::WinRtClassicComMix |
              Microsoft::WRL::RuntimeClassType::InhibitRoOriginateError>,
          IMFContentProtectionManager,
          ABI::Windows::Media::Protection::IMediaProtectionManager> {
 public:
  MediaFoundationProtectionManager();
  ~MediaFoundationProtectionManager() override;

  HRESULT RuntimeClassInitialize();
  HRESULT SetCdmProxy(scoped_refptr<MediaFoundationCdmProxy> cdm_proxy);

  // IMFContentProtectionManager.
  IFACEMETHODIMP BeginEnableContent(IMFActivate* enabler_activate,
                                    IMFTopology* topology,
                                    IMFAsyncCallback* callback,
                                    IUnknown* state) override;
  IFACEMETHODIMP EndEnableContent(IMFAsyncResult* async_result) override;

  // IMediaProtectionManager.
  // MFMediaEngine can query this interface to invoke get_Properties().
  IFACEMETHODIMP add_ServiceRequested(
      ABI::Windows::Media::Protection::IServiceRequestedEventHandler* handler,
      EventRegistrationToken* cookie) override;
  IFACEMETHODIMP remove_ServiceRequested(
      EventRegistrationToken cookie) override;
  IFACEMETHODIMP add_RebootNeeded(
      ABI::Windows::Media::Protection::IRebootNeededEventHandler* handler,
      EventRegistrationToken* cookie) override;
  IFACEMETHODIMP remove_RebootNeeded(EventRegistrationToken cookie) override;
  IFACEMETHODIMP add_ComponentLoadFailed(
      ABI::Windows::Media::Protection::IComponentLoadFailedEventHandler*
          handler,
      EventRegistrationToken* cookie) override;
  IFACEMETHODIMP remove_ComponentLoadFailed(
      EventRegistrationToken cookie) override;
  IFACEMETHODIMP get_Properties(
      ABI::Windows::Foundation::Collections::IPropertySet** value) override;

 protected:
  Microsoft::WRL::ComPtr<ABI::Windows::Foundation::Collections::IPropertySet>
      property_set_;

  scoped_refptr<MediaFoundationCdmProxy> cdm_proxy_;

  HRESULT SetPMPServer(
      ABI::Windows::Media::Protection::IMediaProtectionPMPServer* pmp_server);
};

}  // namespace media

#endif  // MEDIA_RENDERERS_WIN_MEDIA_FOUNDATION_PROTECTION_MANAGER_H_
