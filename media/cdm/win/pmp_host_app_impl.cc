// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/win/pmp_host_app_impl.h"

namespace {

// These GUIDs are required for IMFPMPHostApp. They will be made public in a
// future version of the API.
// TODO(crbug.com/412395836): Update when the GUIDs are made public.
EXTERN_GUID(CLSID_EMEStoreActivate,
            0x2df7b51e,
            0x797b,
            0x4d06,
            0xbe,
            0x71,
            0xd1,
            0x4a,
            0x52,
            0xcf,
            0x84,
            0x21);

EXTERN_GUID(GUID_ClassName,
            0x77631a31,
            0xe5e7,
            0x4785,
            0xbf,
            0x17,
            0x20,
            0xf5,
            0x7b,
            0x22,
            0x48,
            0x02);

EXTERN_GUID(GUID_ObjectStream,
            0x3e73735c,
            0xe6c0,
            0x481d,
            0x82,
            0x60,
            0xee,
            0x5d,
            0xb1,
            0x34,
            0x3b,
            0x5f);

}  // namespace

namespace media {

template <typename T>
PmpHostAppImpl<T>::PmpHostAppImpl() {
  DVLOG_FUNC(1);
  DCHECK(__uuidof(T) == IID_IMFPMPHost || __uuidof(T) == IID_IMFPMPHostApp);
}

template <typename T>
PmpHostAppImpl<T>::~PmpHostAppImpl() {
  DVLOG_FUNC(1);
}

template <typename T>
HRESULT PmpHostAppImpl<T>::RuntimeClassInitialize(T* pmp_host) {
  DVLOG_FUNC(1);
  return AsAgile<T>(pmp_host, &inner_pmp_host_);
}

template <typename T>
STDMETHODIMP PmpHostAppImpl<T>::LockProcess() {
  DVLOG_FUNC(1);
  ComPtr<T> pmp_host;
  RETURN_IF_FAILED(inner_pmp_host_.As(&pmp_host));
  RETURN_IF_FAILED(pmp_host->LockProcess());
  return S_OK;
}

template <typename T>
STDMETHODIMP PmpHostAppImpl<T>::UnlockProcess() {
  DVLOG_FUNC(1);
  ComPtr<T> pmp_host;
  RETURN_IF_FAILED(inner_pmp_host_.As(&pmp_host));
  RETURN_IF_FAILED(pmp_host->UnlockProcess());
  return S_OK;
}

template <typename T>
STDMETHODIMP PmpHostAppImpl<T>::ActivateClassById(LPCWSTR id,
                                                  IStream* stream,
                                                  REFIID riid,
                                                  void** activated_class) {
  DVLOG_FUNC(1);
  if (__uuidof(T) == IID_IMFPMPHostApp) {
    ComPtr<IMFPMPHostApp> pmp_host_app;
    RETURN_IF_FAILED(inner_pmp_host_.As(&pmp_host_app));
    RETURN_IF_FAILED(
        pmp_host_app->ActivateClassById(id, stream, riid, activated_class));
    return S_OK;
  }

  ComPtr<IMFAttributes> creation_attributes;
  RETURN_IF_FAILED(MFCreateAttributes(&creation_attributes, 3));
  RETURN_IF_FAILED(creation_attributes->SetString(GUID_ClassName, id));

  //  Copy the caller's stream across to the PMP as a blob attribute
  if (stream) {
    STATSTG statstg;
    RETURN_IF_FAILED(stream->Stat(&statstg, STATFLAG_NOOPEN | STATFLAG_NONAME));

    std::vector<uint8_t> stream_blob(statstg.cbSize.LowPart);
    unsigned long read_size = 0;
    RETURN_IF_FAILED(
        stream->Read(&stream_blob[0], stream_blob.size(), &read_size));
    RETURN_IF_FAILED(creation_attributes->SetBlob(GUID_ObjectStream,
                                                  &stream_blob[0], read_size));
  }

  // Serialize attributes
  ComPtr<IStream> output_stream;
  RETURN_IF_FAILED(CreateStreamOnHGlobal(nullptr, TRUE, &output_stream));
  RETURN_IF_FAILED(MFSerializeAttributesToStream(creation_attributes.Get(), 0,
                                                 output_stream.Get()));
  RETURN_IF_FAILED(output_stream->Seek({}, STREAM_SEEK_SET, nullptr));

  // Use EME store activate class (hosted in mfsvr.dll) to create object.
  ComPtr<IMFPMPHost> pmp_host;
  ComPtr<IMFActivate> activator;
  RETURN_IF_FAILED(inner_pmp_host_.As(&pmp_host));
  RETURN_IF_FAILED(pmp_host->CreateObjectByCLSID(
      CLSID_EMEStoreActivate, output_stream.Get(), IID_PPV_ARGS(&activator)));
  RETURN_IF_FAILED(activator->ActivateObject(riid, activated_class));

  return S_OK;
}

// Explicitly specify the template instantiation for the two required types.
// This is required so that the linker can find the implementation of the
// template class methods.
template class PmpHostAppImpl<IMFPMPHost>;
template class PmpHostAppImpl<IMFPMPHostApp>;

}  // namespace media
