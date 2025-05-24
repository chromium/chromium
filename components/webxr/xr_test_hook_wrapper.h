// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBXR_XR_TEST_HOOK_WRAPPER_H_
#define COMPONENTS_WEBXR_XR_TEST_HOOK_WRAPPER_H_

#include "base/task/single_thread_task_runner.h"
#include "device/vr/public/mojom/browser_test_interfaces.mojom.h"
#include "device/vr/test/test_hook.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace webxr {

// Wraps a mojo test hook to implement the VRTestHook C++ interface.  Our use
// of VR runtimes are single-threaded at a time, and we initialize/uninitialize
// as we switch between immersive and non-immersive sessions.
// The mojo pointer is thread-affine, but we can keep the same mojo connection
// by getting its pending remote so we temporarily make the test hook mojo
// pointer live on the thread that we are using the VR runtime on while the
// runtime is initialized.
class XRTestHookWrapper : public device::VRTestHook {
 public:
  explicit XRTestHookWrapper(
      mojo::PendingRemote<device_test::mojom::XRTestHook> hook_info);
  virtual ~XRTestHookWrapper();

  void OnFrameSubmitted(const std::vector<device::ViewData>& views) override;
  device::DeviceConfig WaitGetDeviceConfig() override;
  device::PoseFrameData WaitGetPresentingPose() override;
  device::PoseFrameData WaitGetMagicWindowPose() override;
  device::ControllerRole WaitGetControllerRoleForTrackedDeviceIndex(
      unsigned int index) override;
  device::TrackedDeviceClass WaitGetTrackedDeviceClass(
      unsigned int index) override;
  device::ControllerFrameData WaitGetControllerData(
      unsigned int index) override;
  device_test::mojom::EventData WaitGetEventData() override;
  bool WaitGetCanCreateSession() override;
  void AttachCurrentThread() override;
  void DetachCurrentThread() override;

  scoped_refptr<base::SingleThreadTaskRunner> GetBoundTaskRunner();

 private:
  mojo::Remote<device_test::mojom::XRTestHook> hook_;
  mojo::PendingRemote<device_test::mojom::XRTestHook> pending_hook_;
  scoped_refptr<base::SingleThreadTaskRunner> current_task_runner_;
};

}  // namespace webxr

#endif  // COMPONENTS_WEBXR_XR_TEST_HOOK_WRAPPER_H_
