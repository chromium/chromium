// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/services/isolated_xr_device/xr_test_hook_wrapper.h"
#include "base/task/single_thread_task_runner.h"

namespace device {

// TODO(crbug.com/41418750): Remove these as conversion functions as part
// of the switch to only mojom types.
ControllerRole MojoToDeviceControllerRole(
    device_test::mojom::ControllerRole role) {
  switch (role) {
    case device_test::mojom::ControllerRole::kControllerRoleInvalid:
      return device::kControllerRoleInvalid;
    case device_test::mojom::ControllerRole::kControllerRoleLeft:
      return device::kControllerRoleLeft;
    case device_test::mojom::ControllerRole::kControllerRoleRight:
      return device::kControllerRoleRight;
    case device_test::mojom::ControllerRole::kControllerRoleVoice:
      return device::kControllerRoleVoice;
  }
  return device::kControllerRoleInvalid;
}

PoseFrameData MojoToDevicePoseFrameData(
    device_test::mojom::PoseFrameDataPtr& pose) {
  PoseFrameData ret = {};
  ret.is_valid = !!pose->device_to_origin;
  if (ret.is_valid)
    pose->device_to_origin->GetColMajorF(ret.device_to_origin);

  return ret;
}

device_test::mojom::Eye XrEyeToMojoEye(XrEye eye) {
  switch (eye) {
    case XrEye::kLeft:
      return device_test::mojom::Eye::LEFT;
    case XrEye::kRight:
      return device_test::mojom::Eye::RIGHT;
    case XrEye::kNone:
      return device_test::mojom::Eye::NONE;
  }
}

XRTestHookWrapper::XRTestHookWrapper(
    mojo::PendingRemote<device_test::mojom::XRTestHook> pending_hook)
    : pending_hook_(std::move(pending_hook)) {}

void XRTestHookWrapper::OnFrameSubmitted(const std::vector<ViewData>& views) {
  if (hook_) {
    std::vector<device_test::mojom::ViewDataPtr> submitted_views;
    for (const ViewData& view : views) {
      device_test::mojom::ViewDataPtr view_data =
          device_test::mojom::ViewData::New();
      view_data->color = device_test::mojom::Color::New(
          view.color.r, view.color.g, view.color.b, view.color.a);
      view_data->viewport = view.viewport;
      view_data->eye = XrEyeToMojoEye(view.eye);
      submitted_views.push_back(std::move(view_data));
    }
    hook_->OnFrameSubmitted(std::move(submitted_views));
  }
}

DeviceConfig XRTestHookWrapper::WaitGetDeviceConfig() {
  if (hook_) {
    device_test::mojom::DeviceConfigPtr config;
    hook_->WaitGetDeviceConfig(&config);
    if (config) {
      DeviceConfig ret = {};
      ret.interpupillary_distance = config->interpupillary_distance;
      ret.viewport_left[0] = config->projection_left->left;
      ret.viewport_left[1] = config->projection_left->right;
      ret.viewport_left[2] = config->projection_left->top;
      ret.viewport_left[3] = config->projection_left->left;

      ret.viewport_right[0] = config->projection_right->left;
      ret.viewport_right[1] = config->projection_right->right;
      ret.viewport_right[2] = config->projection_right->top;
      ret.viewport_right[3] = config->projection_right->left;
      return ret;
    }
  }

  return {};
}

PoseFrameData XRTestHookWrapper::WaitGetPresentingPose() {
  if (hook_) {
    device_test::mojom::PoseFrameDataPtr pose;
    hook_->WaitGetPresentingPose(&pose);
    if (pose) {
      return MojoToDevicePoseFrameData(pose);
    }
  }

  return {};
}

PoseFrameData XRTestHookWrapper::WaitGetMagicWindowPose() {
  if (hook_) {
    device_test::mojom::PoseFrameDataPtr pose;
    hook_->WaitGetMagicWindowPose(&pose);
    if (pose) {
      return MojoToDevicePoseFrameData(pose);
    }
  }

  return {};
}

ControllerRole XRTestHookWrapper::WaitGetControllerRoleForTrackedDeviceIndex(
    unsigned int index) {
  if (hook_) {
    device_test::mojom::ControllerRole role;
    hook_->WaitGetControllerRoleForTrackedDeviceIndex(index, &role);
    return MojoToDeviceControllerRole(role);
  }

  return device::kControllerRoleInvalid;
}

TrackedDeviceClass XRTestHookWrapper::WaitGetTrackedDeviceClass(
    unsigned int index) {
  if (hook_) {
    device_test::mojom::TrackedDeviceClass device_class;
    hook_->WaitGetTrackedDeviceClass(index, &device_class);
    switch (device_class) {
      case device_test::mojom::TrackedDeviceClass::kTrackedDeviceInvalid:
        return device::kTrackedDeviceInvalid;
      case device_test::mojom::TrackedDeviceClass::kTrackedDeviceHmd:
        return device::kTrackedDeviceHmd;
      case device_test::mojom::TrackedDeviceClass::kTrackedDeviceController:
        return device::kTrackedDeviceController;
      case device_test::mojom::TrackedDeviceClass::kTrackedDeviceGenericTracker:
        return device::kTrackedDeviceGenericTracker;
      case device_test::mojom::TrackedDeviceClass::
          kTrackedDeviceTrackingReference:
        return device::kTrackedDeviceTrackingReference;
      case device_test::mojom::TrackedDeviceClass::
          kTrackedDeviceDisplayRedirect:
        return device::kTrackedDeviceDisplayRedirect;
    }
  }

  return device::kTrackedDeviceInvalid;
}

ControllerFrameData XRTestHookWrapper::WaitGetControllerData(
    unsigned int index) {
  if (hook_) {
    device_test::mojom::ControllerFrameDataPtr data;
    hook_->WaitGetControllerData(index, &data);
    if (data) {
      ControllerFrameData ret = {};
      ret.packet_number = data->packet_number;
      ret.buttons_pressed = data->buttons_pressed;
      ret.buttons_touched = data->buttons_touched;
      ret.supported_buttons = data->supported_buttons;
      ret.pose_data = MojoToDevicePoseFrameData(data->pose_data);
      ret.role = MojoToDeviceControllerRole(data->role);
      ret.is_valid = data->is_valid;
      for (unsigned int i = 0; i < device::kMaxNumAxes; ++i) {
        ret.axis_data[i].x = data->axis_data[i]->x;
        ret.axis_data[i].y = data->axis_data[i]->y;
        ret.axis_data[i].axis_type = data->axis_data[i]->axis_type;
      }
      if (data->hand_data) {
        ret.has_hand_data = true;
        auto& joint_data = ret.hand_data;
        CHECK_GE(std::size(joint_data),
                 data->hand_data->hand_joint_data.size());

        for (const auto& joint_entry : data->hand_data->hand_joint_data) {
          uint32_t joint_index = static_cast<uint32_t>(joint_entry->joint);

          joint_data[joint_index] = {joint_entry->joint,
                                     joint_entry->mojo_from_joint,
                                     joint_entry->radius};
        }
      }
      return ret;
    }
  }

  return {};
}

device_test::mojom::EventData XRTestHookWrapper::WaitGetEventData() {
  device_test::mojom::EventData ret = {};
  if (hook_) {
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
    bool can_create_session;
    hook_->WaitGetCanCreateSession(&can_create_session);
    return can_create_session;
  }

  // In the absence of a test hook telling us that we can't create a session;
  // assume that we can, there's often enough default behavior to do so, and
  // some tests expect to be able to get a session without creating a test hook.
  return true;
}

void XRTestHookWrapper::AttachCurrentThread() {
  if (pending_hook_)
    hook_.Bind(std::move(pending_hook_));

  current_task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
}

void XRTestHookWrapper::DetachCurrentThread() {
  if (hook_)
    pending_hook_ = hook_.Unbind();

  current_task_runner_ = nullptr;
}

scoped_refptr<base::SingleThreadTaskRunner>
XRTestHookWrapper::GetBoundTaskRunner() {
  return current_task_runner_;
}

XRTestHookWrapper::~XRTestHookWrapper() = default;

}  // namespace device
