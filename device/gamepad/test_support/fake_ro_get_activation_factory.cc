// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <wrl.h>
#include <wrl/event.h>

#include "base/notreached.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "base/win/scoped_hstring.h"
#include "device/gamepad/test_support/fake_igamepad_statics.h"
#include "device/gamepad/test_support/fake_ro_get_activation_factory.h"

namespace device {

HRESULT FakeRoGetActivationFactory(HSTRING class_id,
                                   const IID& iid,
                                   void** out_factory) {
  base::win::ScopedHString class_id_hstring(class_id);

  if (class_id_hstring.Get() != RuntimeClass_Windows_Gaming_Input_Gamepad)
    return E_NOTIMPL;

  Microsoft::WRL::ComPtr<FakeIGamepadStatics> gamepad_statics =
      FakeIGamepadStatics::GetInstance();
  *out_factory = gamepad_statics.Detach();

  if (*out_factory == nullptr) {
    NOTIMPLEMENTED();
    return E_NOTIMPL;
  }
  return S_OK;
}

HRESULT FakeRoGetActivationFactoryToTestErrorHandling(HSTRING class_id,
                                                      const IID& iid,
                                                      void** out_factory) {
  return E_FAIL;
}

}  // namespace device
