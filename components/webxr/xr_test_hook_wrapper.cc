// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webxr/xr_test_hook_wrapper.h"

#include "base/task/single_thread_task_runner.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"

namespace webxr {
XRTestHookWrapper::XRTestHookWrapper(
    mojo::PendingRemote<device_test::mojom::XRTestHook> pending_hook)
    : pending_hook_(std::move(pending_hook)) {}

void XRTestHookWrapper::OnFrameSubmitted(
    const std::vector<device::ViewData>& views) {
  if (hook_) {
    mojo::ScopedAllowSyncCallForTesting scoped_allow_sync;
    hook_->OnFrameSubmitted(views);
  }
}

device::DeviceConfig XRTestHookWrapper::WaitGetDeviceConfig() {
  if (hook_) {
    mojo::ScopedAllowSyncCallForTesting scoped_allow_sync;
    device::DeviceConfig config;
    hook_->WaitGetDeviceConfig(&config);
    return config;
  }

  return {};
}

std::optional<gfx::Transform> XRTestHookWrapper::WaitGetPresentingPose() {
  if (hook_) {
    mojo::ScopedAllowSyncCallForTesting scoped_allow_sync;
    std::optional<gfx::Transform> pose;
    hook_->WaitGetPresentingPose(&pose);
    return pose;
  }

  return std::nullopt;
}

std::optional<gfx::Transform> XRTestHookWrapper::WaitGetMagicWindowPose() {
  if (hook_) {
    mojo::ScopedAllowSyncCallForTesting scoped_allow_sync;
    std::optional<gfx::Transform> pose;
    hook_->WaitGetMagicWindowPose(&pose);
    return pose;
  }

  return std::nullopt;
}

device::ControllerRole
XRTestHookWrapper::WaitGetControllerRoleForTrackedDeviceIndex(uint32_t index) {
  if (hook_) {
    mojo::ScopedAllowSyncCallForTesting scoped_allow_sync;
    device::ControllerRole role;
    hook_->WaitGetControllerRoleForTrackedDeviceIndex(index, &role);
    return role;
  }

  return device::ControllerRole::kControllerRoleInvalid;
}

device::ControllerFrameData XRTestHookWrapper::WaitGetControllerData(
    uint32_t index) {
  if (hook_) {
    mojo::ScopedAllowSyncCallForTesting scoped_allow_sync;
    device::ControllerFrameData data;
    hook_->WaitGetControllerData(index, &data);
    return data;
  }

  return {};
}

device_test::mojom::EventData XRTestHookWrapper::WaitGetEventData() {
  device_test::mojom::EventData ret = {};
  if (hook_) {
    mojo::ScopedAllowSyncCallForTesting scoped_allow_sync;
    device_test::mojom::EventDataPtr data;
    hook_->WaitGetEventData(&data);
    if (data) {
      ret = *data;
    }
  }
  return ret;
}

bool XRTestHookWrapper::WaitGetCanCreateSession() {
  if (hook_) {
    mojo::ScopedAllowSyncCallForTesting scoped_allow_sync;
    bool can_create_session;
    hook_->WaitGetCanCreateSession(&can_create_session);
    return can_create_session;
  }

  // In the absence of a test hook telling us that we can't create a session;
  // assume that we can, there's often enough default behavior to do so, and
  // some tests expect to be able to get a session without creating a test hook.
  return true;
}

std::optional<device::VisibilityMaskData>
XRTestHookWrapper::WaitGetVisibilityMask(uint32_t view_index) {
  if (hook_) {
    mojo::ScopedAllowSyncCallForTesting scoped_allow_sync;
    std::optional<device::VisibilityMaskData> mask;
    hook_->WaitGetVisibilityMask(view_index, &mask);
    return mask;
  }

  return std::nullopt;
}

void XRTestHookWrapper::AttachCurrentThread() {
  if (pending_hook_) {
    hook_.Bind(std::move(pending_hook_));
  }

  current_task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
}

void XRTestHookWrapper::DetachCurrentThread() {
  if (hook_) {
    pending_hook_ = hook_.Unbind();
  }

  current_task_runner_ = nullptr;
}

scoped_refptr<base::SingleThreadTaskRunner>
XRTestHookWrapper::GetBoundTaskRunner() {
  return current_task_runner_;
}

XRTestHookWrapper::~XRTestHookWrapper() = default;

}  // namespace webxr
