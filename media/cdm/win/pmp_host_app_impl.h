// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_WIN_PMP_HOST_APP_IMPL_H_
#define MEDIA_CDM_WIN_PMP_HOST_APP_IMPL_H_

#include <unknwn.h>

#include <basetyps.h>
#include <windows.foundation.h>
#include <wrl/client.h>
#include <wrl/event.h>

#include "base/scoped_native_library.h"
#include "base/win/propvarutil.h"
#include "media/base/win/mf_helpers.h"

namespace media {

using Microsoft::WRL::ComPtr;

// It is not currently possible for a non-UWP application to create an instance
// of the default IMFPMPHostApp implementation. Typically, apps could create an
// instance of a PMP host via the MediaProtectionPMPServer WinRT class, but this
// is currently only supported for UWP apps. The PlayReady CDM requires an
// instance of IMFPMPHostApp to instantiate various objects within the
// appropriate environment (which is in-proc for hardware secure PlayReady).
template <typename T>
class PmpHostAppImpl : public Microsoft::WRL::RuntimeClass<
                           Microsoft::WRL::RuntimeClassFlags<
                               Microsoft::WRL::RuntimeClassType::ClassicCom>,
                           IMFPMPHostApp> {
 public:
  PmpHostAppImpl();
  ~PmpHostAppImpl() override;

  HRESULT RuntimeClassInitialize(T* pmp_host);

  // IMFPMPHostApp
  STDMETHODIMP LockProcess() override;
  STDMETHODIMP UnlockProcess() override;
  STDMETHODIMP ActivateClassById(LPCWSTR id,
                                 IStream* stream,
                                 REFIID riid,
                                 void** activated_class) override;

 private:
  Microsoft::WRL::AgileRef inner_pmp_host_;
};

}  // namespace media

#endif  // MEDIA_CDM_WIN_PMP_HOST_APP_IMPL_H_
