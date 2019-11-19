// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/video_capture_device_chromeos_halv3.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/platform_thread.h"
#include "base/trace_event/trace_event.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "media/base/bind_to_current_loop.h"
#include "media/capture/video/chromeos/camera_device_context.h"
#include "media/capture/video/chromeos/camera_device_delegate.h"
#include "media/capture/video/chromeos/camera_hal_delegate.h"
#include "ui/display/display.h"
#include "ui/display/display_observer.h"
#include "ui/display/screen.h"

namespace media {

class VideoCaptureDeviceChromeOSHalv3::PowerManagerClientProxy
    : public base::RefCountedThreadSafe<PowerManagerClientProxy>,
      public chromeos::PowerManagerClient::Observer {
 public:
  PowerManagerClientProxy() = default;

  void Init(base::WeakPtr<VideoCaptureDeviceChromeOSHalv3> device,
            scoped_refptr<base::SingleThreadTaskRunner> device_task_runner,
            scoped_refptr<base::SingleThreadTaskRunner> dbus_task_runner) {
    device_ = std::move(device);
    device_task_runner_ = std::move(device_task_runner);
    dbus_task_runner_ = std::move(dbus_task_runner);

    dbus_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&PowerManagerClientProxy::InitOnDBusThread, this));
  }

  void Shutdown() {
    dbus_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&PowerManagerClientProxy::ShutdownOnDBusThread, this));
  }

  void UnblockSuspend(const base::UnguessableToken& unblock_suspend_token) {
    dbus_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&PowerManagerClientProxy::UnblockSuspendOnDBusThread,
                       this, unblock_suspend_token));
  }

 private:
  friend class base::RefCountedThreadSafe<PowerManagerClientProxy>;

  ~PowerManagerClientProxy() override = default;

  void InitOnDBusThread() {
    DCHECK(dbus_task_runner_->RunsTasksInCurrentSequence());
    chromeos::PowerManagerClient::Get()->AddObserver(this);
  }

  void ShutdownOnDBusThread() {
    DCHECK(dbus_task_runner_->RunsTasksInCurrentSequence());
    chromeos::PowerManagerClient::Get()->RemoveObserver(this);
  }

  void UnblockSuspendOnDBusThread(
      const base::UnguessableToken& unblock_suspend_token) {
    DCHECK(dbus_task_runner_->RunsTasksInCurrentSequence());
    chromeos::PowerManagerClient::Get()->UnblockSuspend(unblock_suspend_token);
  }

  // chromeos::PowerManagerClient::Observer:
  void SuspendImminent(power_manager::SuspendImminent::Reason reason) final {
    auto token = base::UnguessableToken::Create();
    chromeos::PowerManagerClient::Get()->BlockSuspend(
        token, "VideoCaptureDeviceChromeOSHalv3");
    device_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&VideoCaptureDeviceChromeOSHalv3::CloseDevice,
                                  device_, token));
  }

  void SuspendDone(const base::TimeDelta& sleep_duration) final {
    device_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&VideoCaptureDeviceChromeOSHalv3::OpenDevice, device_));
  }

  base::WeakPtr<VideoCaptureDeviceChromeOSHalv3> device_;
  scoped_refptr<base::SingleThreadTaskRunner> device_task_runner_;
  scoped_refptr<base::TaskRunner> dbus_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(PowerManagerClientProxy);
};

VideoCaptureDeviceChromeOSHalv3::VideoCaptureDeviceChromeOSHalv3(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    const VideoCaptureDeviceDescriptor& device_descriptor,
    scoped_refptr<CameraHalDelegate> camera_hal_delegate,
    CameraAppDeviceImpl* camera_app_device,
    base::OnceClosure cleanup_callback)
    : device_descriptor_(device_descriptor),
      camera_hal_delegate_(std::move(camera_hal_delegate)),
      capture_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      camera_device_ipc_thread_(std::string("CameraDeviceIpcThread") +
                                device_descriptor.device_id),
      screen_observer_delegate_(
          ScreenObserverDelegate::Create(this, ui_task_runner)),
      lens_facing_(device_descriptor.facing),
      // External cameras have lens_facing as MEDIA_VIDEO_FACING_NONE.
      // We don't want to rotate the frame even if the device rotates.
      rotates_with_device_(lens_facing_ !=
                           VideoFacingMode::MEDIA_VIDEO_FACING_NONE),
      rotation_(0),
      camera_app_device_(camera_app_device),
      cleanup_callback_(std::move(cleanup_callback)),
      power_manager_client_proxy_(
          base::MakeRefCounted<PowerManagerClientProxy>()) {
  power_manager_client_proxy_->Init(weak_ptr_factory_.GetWeakPtr(),
                                    capture_task_runner_,
                                    std::move(ui_task_runner));
}

VideoCaptureDeviceChromeOSHalv3::~VideoCaptureDeviceChromeOSHalv3() {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());
  DCHECK(!camera_device_ipc_thread_.IsRunning());
  screen_observer_delegate_->RemoveObserver();
  power_manager_client_proxy_->Shutdown();
  std::move(cleanup_callback_).Run();
}

// VideoCaptureDevice implementation.
void VideoCaptureDeviceChromeOSHalv3::AllocateAndStart(
    const VideoCaptureParams& params,
    std::unique_ptr<Client> client) {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());
  DCHECK(!camera_device_delegate_);
  TRACE_EVENT0("camera", "Start Device");
  if (!camera_device_ipc_thread_.Start()) {
    std::string error_msg = "Failed to start device thread";
    LOG(ERROR) << error_msg;
    client->OnError(
        media::VideoCaptureError::kCrosHalV3FailedToStartDeviceThread,
        FROM_HERE, error_msg);
    return;
  }
  capture_params_ = params;
  device_context_ = std::make_unique<CameraDeviceContext>(std::move(client));

  camera_device_delegate_ = std::make_unique<CameraDeviceDelegate>(
      device_descriptor_, camera_hal_delegate_,
      camera_device_ipc_thread_.task_runner(), camera_app_device_);
  OpenDevice();
}

void VideoCaptureDeviceChromeOSHalv3::StopAndDeAllocate() {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());

  if (!camera_device_delegate_) {
    return;
  }
  CloseDevice(base::UnguessableToken());
  camera_device_ipc_thread_.Stop();
  camera_device_delegate_.reset();
  device_context_.reset();
}

void VideoCaptureDeviceChromeOSHalv3::TakePhoto(TakePhotoCallback callback) {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());
  DCHECK(camera_device_delegate_);
  camera_device_ipc_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&CameraDeviceDelegate::TakePhoto,
                                camera_device_delegate_->GetWeakPtr(),
                                base::Passed(&callback)));
}

void VideoCaptureDeviceChromeOSHalv3::GetPhotoState(
    GetPhotoStateCallback callback) {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());
  camera_device_ipc_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&CameraDeviceDelegate::GetPhotoState,
                                camera_device_delegate_->GetWeakPtr(),
                                base::Passed(&callback)));
}

void VideoCaptureDeviceChromeOSHalv3::SetPhotoOptions(
    mojom::PhotoSettingsPtr settings,
    SetPhotoOptionsCallback callback) {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());
  camera_device_ipc_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&CameraDeviceDelegate::SetPhotoOptions,
                     camera_device_delegate_->GetWeakPtr(),
                     base::Passed(&settings), base::Passed(&callback)));
}

void VideoCaptureDeviceChromeOSHalv3::OpenDevice() {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());

  if (!camera_device_delegate_) {
    return;
  }
  // It's safe to pass unretained |device_context_| here since
  // VideoCaptureDeviceChromeOSHalv3 owns |camera_device_delegate_| and makes
  // sure |device_context_| outlives |camera_device_delegate_|.
  camera_device_ipc_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&CameraDeviceDelegate::AllocateAndStart,
                     camera_device_delegate_->GetWeakPtr(), capture_params_,
                     base::Unretained(device_context_.get())));
  camera_device_ipc_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&CameraDeviceDelegate::SetRotation,
                     camera_device_delegate_->GetWeakPtr(), rotation_));
}

void VideoCaptureDeviceChromeOSHalv3::CloseDevice(
    base::UnguessableToken unblock_suspend_token) {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());

  if (!camera_device_delegate_) {
    return;
  }
  // We do our best to allow the camera HAL cleanly shut down the device.  In
  // general we don't trust the camera HAL so if the device does not close in
  // time we simply terminate the Mojo channel by resetting
  // |camera_device_delegate_|.
  base::WaitableEvent device_closed(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  camera_device_ipc_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&CameraDeviceDelegate::StopAndDeAllocate,
                                camera_device_delegate_->GetWeakPtr(),
                                base::BindOnce(
                                    [](base::WaitableEvent* device_closed) {
                                      device_closed->Signal();
                                    },
                                    base::Unretained(&device_closed))));
  base::TimeDelta kWaitTimeoutSecs = base::TimeDelta::FromSeconds(3);
  device_closed.TimedWait(kWaitTimeoutSecs);
  if (!unblock_suspend_token.is_empty())
    power_manager_client_proxy_->UnblockSuspend(unblock_suspend_token);
}

void VideoCaptureDeviceChromeOSHalv3::SetDisplayRotation(
    const display::Display& display) {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());
  if (display.IsInternal())
    SetRotation(display.rotation() * 90);
}

void VideoCaptureDeviceChromeOSHalv3::SetRotation(int rotation) {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());
  if (!rotates_with_device_) {
    rotation = 0;
  } else if (lens_facing_ == VideoFacingMode::MEDIA_VIDEO_FACING_ENVIRONMENT) {
    // Original frame when |rotation| = 0
    // -----------------------
    // |          *          |
    // |         * *         |
    // |        *   *        |
    // |       *******       |
    // |      *       *      |
    // |     *         *     |
    // -----------------------
    //
    // |rotation| = 90, this is what back camera sees
    // -----------------------
    // |    ********         |
    // |       *   ****      |
    // |       *      ***    |
    // |       *      ***    |
    // |       *   ****      |
    // |    ********         |
    // -----------------------
    //
    // |rotation| = 90, this is what front camera sees
    // -----------------------
    // |         ********    |
    // |      ****   *       |
    // |    ***      *       |
    // |    ***      *       |
    // |      ****   *       |
    // |         ********    |
    // -----------------------
    //
    // Therefore, for back camera, we need to rotate (360 - |rotation|).
    rotation = (360 - rotation) % 360;
  }
  rotation_ = rotation;
  if (camera_device_ipc_thread_.IsRunning()) {
    camera_device_ipc_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&CameraDeviceDelegate::SetRotation,
                       camera_device_delegate_->GetWeakPtr(), rotation_));
  }
}

}  // namespace media
