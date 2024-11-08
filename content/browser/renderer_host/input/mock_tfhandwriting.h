// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_MOCK_TFHANDWRITING_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_MOCK_TFHANDWRITING_H_

#include <ShellHandwriting.h>
#include <msctf.h>
#include <winerror.h>
#include <wrl/implements.h>

#include "testing/gmock/include/gmock/gmock.h"

namespace content {

// Assign Com pointer to the variable pointed by the k-th (0-based) argument if
// the result param value is S_OK. Returns the result param value.
ACTION_TEMPLATE(SetComPointeeAndReturnResult,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_2_VALUE_PARAMS(pointer, result)) {
  if (result != S_OK) {
    return result;
  }

  pointer->AddRef();
  *std::get<k>(args) = pointer;
  return S_OK;
}

// Mock various ITf interfaces used for testing SHell Handwriting API.
class MockTfImpl
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          ITfThreadMgr,
          ITfSource,
          ::ITfHandwriting> {
 public:
  MockTfImpl();
  MockTfImpl(const MockTfImpl&) = delete;
  MockTfImpl& operator=(const MockTfImpl&) = delete;
  ~MockTfImpl() override;

  // IUnknown:
  MOCK_METHOD(HRESULT,
              QueryInterface,
              (REFIID interface_id,
               _Outptr_result_nullonfailure_ void** result),
              (final, Calltype(STDMETHODCALLTYPE)));

  // ITfThreadMgr:
  MOCK_METHOD(HRESULT,
              Activate,
              (__RPC__out TfClientId * ptid),
              (final, Calltype(STDMETHODCALLTYPE)));
  MOCK_METHOD(HRESULT, Deactivate, (), (final, Calltype(STDMETHODCALLTYPE)));
  MOCK_METHOD(HRESULT,
              CreateDocumentMgr,
              (__RPC__deref_out_opt ITfDocumentMgr * *ppdim),
              (final, Calltype(STDMETHODCALLTYPE)));
  MOCK_METHOD(HRESULT,
              EnumDocumentMgrs,
              (__RPC__deref_out_opt IEnumTfDocumentMgrs * *ppEnum),
              (final, Calltype(STDMETHODCALLTYPE)));
  MOCK_METHOD(HRESULT,
              GetFocus,
              (__RPC__deref_out_opt ITfDocumentMgr * *ppdimFocus),
              (final, Calltype(STDMETHODCALLTYPE)));
  MOCK_METHOD(HRESULT,
              SetFocus,
              (__RPC__in_opt ITfDocumentMgr * pdimFocus),
              (final, Calltype(STDMETHODCALLTYPE)));
  MOCK_METHOD(HRESULT,
              AssociateFocus,
              (__RPC__in HWND hwnd,
               __RPC__in_opt ITfDocumentMgr* pdimNew,
               __RPC__deref_out_opt ITfDocumentMgr** ppdimPrev),
              (final, Calltype(STDMETHODCALLTYPE)));
  MOCK_METHOD(HRESULT,
              IsThreadFocus,
              (__RPC__out BOOL * pfThreadFocus),
              (final, Calltype(STDMETHODCALLTYPE)));
  MOCK_METHOD(HRESULT,
              GetFunctionProvider,
              (__RPC__in REFCLSID clsid,
               __RPC__deref_out_opt ITfFunctionProvider** ppFuncProv),
              (final, Calltype(STDMETHODCALLTYPE)));
  MOCK_METHOD(HRESULT,
              EnumFunctionProviders,
              (__RPC__deref_out_opt IEnumTfFunctionProviders * *ppEnum),
              (final, Calltype(STDMETHODCALLTYPE)));
  MOCK_METHOD(HRESULT,
              GetGlobalCompartment,
              (__RPC__deref_out_opt ITfCompartmentMgr * *ppCompMgr),
              (final, Calltype(STDMETHODCALLTYPE)));

  // ITfSource:
  MOCK_METHOD(HRESULT,
              AdviseSink,
              (__RPC__in REFIID riid,
               __RPC__in_opt IUnknown* punk,
               __RPC__out DWORD* pdwCookie),
              (final, Calltype(STDMETHODCALLTYPE)));
  MOCK_METHOD(HRESULT,
              UnadviseSink,
              (DWORD dwCookie),
              (final, Calltype(STDMETHODCALLTYPE)));

  // ::ITfHandwriting:
  MOCK_METHOD(HRESULT,
              GetHandwritingState,
              (::TfHandwritingState * handwriting_state),
              (final, Calltype(STDMETHODCALLTYPE)));
  MOCK_METHOD(HRESULT,
              SetHandwritingState,
              (::TfHandwritingState handwriting_state),
              (final, Calltype(STDMETHODCALLTYPE)));
  MOCK_METHOD(HRESULT,
              RequestHandwritingForPointer,
              (UINT32 pointerId,
               UINT64 handwritingStrokeId,
               BOOL* requestAccepted,
               ::ITfHandwritingRequest** request),
              (final, Calltype(STDMETHODCALLTYPE)));
  MOCK_METHOD(HRESULT,
              GetHandwritingDistanceThreshold,
              (SIZE * distance_buffer_pixels),
              (final, Calltype(STDMETHODCALLTYPE)));
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_MOCK_TFHANDWRITING_H_
