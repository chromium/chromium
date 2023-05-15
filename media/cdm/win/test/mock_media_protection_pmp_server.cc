// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/win/test/mock_media_protection_pmp_server.h"

#include <mfapi.h>
#include <mferror.h>

#include "base/notreached.h"
#include "media/base/win/mf_helpers.h"

namespace media {

using Microsoft::WRL::ComPtr;
using Microsoft::WRL::MakeAndInitialize;

MockMediaProtectionPMPServer::MockMediaProtectionPMPServer() = default;

MockMediaProtectionPMPServer::~MockMediaProtectionPMPServer() {
  DVLOG_FUNC(1);
}

HRESULT MockMediaProtectionPMPServer::RuntimeClassInitialize(
    ABI::Windows::Foundation::Collections::IPropertySet* property_pmp) {
  DVLOG_FUNC(1);
  property_pmp_ = property_pmp;

  // If we create a PMP media session with `MFPMPSESSION_IN_PROCESS` flag, the
  // Protected Media Path will exist in process. This way we can use the
  // in-process PMP server and PMP host for the rest of Media Foundation
  // pipelines.
  RETURN_IF_FAILED(MFCreatePMPMediaSession(MFPMPSESSION_IN_PROCESS, nullptr,
                                           &media_session_, nullptr));
  RETURN_IF_FAILED(MFGetService(media_session_.Get(), MF_PMP_SERVER_CONTEXT,
                                IID_PPV_ARGS(&pmp_server_)));
  RETURN_IF_FAILED(MFGetService(media_session_.Get(), MF_PMP_SERVICE,
                                IID_PPV_ARGS(&pmp_host_)));

  return S_OK;
}

// IInspectable
STDMETHODIMP MockMediaProtectionPMPServer::GetIids(_COM_Outptr_ ULONG* iidCount,
                                                   _COM_Outptr_ IID** iids) {
  DVLOG_FUNC(3);

  // API not used.
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MockMediaProtectionPMPServer::GetRuntimeClassName(
    _COM_Outptr_ HSTRING* className) {
  DVLOG_FUNC(3);

  // API not used.
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MockMediaProtectionPMPServer::GetTrustLevel(
    _COM_Outptr_ TrustLevel* trustLevel) {
  DVLOG_FUNC(3);

  // API not used.
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

// IMediaProtectionPMPServer
STDMETHODIMP MockMediaProtectionPMPServer::get_Properties(
    _COM_Outptr_ ABI::Windows::Foundation::Collections::IPropertySet**
        ppProperties) {
  DVLOG_FUNC(1);
  RETURN_IF_FAILED(property_pmp_.CopyTo(ppProperties));
  return S_OK;
}

// IMFGetService
STDMETHODIMP MockMediaProtectionPMPServer::GetService(
    __RPC__in REFGUID guid_service,
    __RPC__in REFIID riid,
    __RPC__deref_out_opt LPVOID* object) {
  DVLOG_FUNC(1);
  if (!object) {
    return E_POINTER;
  }

  *object = nullptr;

  if (guid_service == MF_PMP_SERVER_CONTEXT) {
    RETURN_IF_FAILED(pmp_server_.CopyTo(riid, object));
  } else if (guid_service == MF_PMP_SERVICE && riid == IID_IMFPMPHost) {
    RETURN_IF_FAILED(pmp_host_.CopyTo(riid, object));
  } else {
    return MF_E_UNSUPPORTED_SERVICE;
  }

  return S_OK;
}

}  // namespace media
