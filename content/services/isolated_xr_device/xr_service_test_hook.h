// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_ISOLATED_XR_DEVICE_XR_SERVICE_TEST_HOOK_H_
#define CONTENT_SERVICES_ISOLATED_XR_DEVICE_XR_SERVICE_TEST_HOOK_H_

#include "device/vr/public/mojom/test/browser_test_interfaces.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace device {

class XRServiceTestHook final : public device_test::mojom::XRServiceTestHook {
 public:
  XRServiceTestHook();
  ~XRServiceTestHook() override;

  // device_test::mojom::XRServiceTestHook
  void SetTestHook(mojo::PendingRemote<device_test::mojom::XRTestHook> hook,
                   device_test::mojom::XRServiceTestHook::SetTestHookCallback
                       callback) override;
};

}  // namespace device

#endif  // CONTENT_SERVICES_ISOLATED_XR_DEVICE_XR_SERVICE_TEST_HOOK_H_
