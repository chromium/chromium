// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_TEST_SUPPORT_FAKE_RO_GET_ACTIVATION_FACTORY_H_
#define DEVICE_GAMEPAD_TEST_SUPPORT_FAKE_RO_GET_ACTIVATION_FACTORY_H_

#include <hstring.h>
#include <inspectable.h>
#include <roapi.h>
#include <windef.h>

namespace device {

// Fake implementation of base::win::RoGetActivationFactory for test.
HRESULT FakeRoGetActivationFactory(HSTRING class_id,
                                   const IID& iid,
                                   void** out_factory);

// Fake implementation of base::win::RoGetActivationFactory to test
// corresponding error handling.
HRESULT FakeRoGetActivationFactoryToTestErrorHandling(HSTRING class_id,
                                                      const IID& iid,
                                                      void** out_factory);

}  // namespace device

#endif  // DEVICE_GAMEPAD_TEST_SUPPORT_FAKE_RO_GET_ACTIVATION_FACTORY_H_
