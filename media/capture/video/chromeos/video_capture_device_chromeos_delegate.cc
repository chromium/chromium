// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/video_capture_device_chromeos_delegate.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "chromeos/ash/components/mojo_service_manager/connection.h"
#include "media/capture/video/chromeos/camera_app_device_bridge_impl.h"
#include "media/capture/video/chromeos/camera_device_delegate.h"
#include "media/capture/video/chromeos/camera_hal_delegate.h"
#include "media/capture/video/chromeos/mojom/system_event_monitor.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/cros_system_api/mojo/service_constants.h"

namespace media {

class VideoCaptureDeviceChromeOSDelegate::PowerObserver
    : public cros::mojom::CrosPowerObserver {
 public:
  PowerObserver(base::WeakPtr<VideoCaptureDeviceChromeOSDelegate> device,
                scoped_refptr<base::SingleThreadTaskRunner> device_task_runner)
      : device_suspend_handler_(device_task_runner, std::move(device)) {
    if (!ash::mojo_service_manager::IsServiceManagerBound()) {
      return;
    }
    ash::mojo_service_manager::GetServiceManagerProxy()->Request(
        /*service_name=*/chromeos::mojo_services::kCrosSystemEventMonitor,
        std::nullopt, monitor_.BindNewPipeAndPassReceiver().PassPipe());
    monitor_->AddPowerObserver("VideoCaptureDeviceChromeOSDelegate",
                               receiver_.BindNewPipeAndPassRemote());
  }

  PowerObserver(const PowerObserver&) = delete;
  PowerObserver& operator=(const PowerObserver&) = delete;

  ~PowerObserver() override = default;

  void OnSystemSuspend(OnSystemSuspendCallback callback) override {
    device_suspend_handler_.AsyncCall(&DeviceSuspendHandler::TryCloseDevice)
        .WithArgs(base::BindPostTaskToCurrentDefault(std::move(callback)));
  }

  void OnSystemResume() override {
    device_suspend_handler_.AsyncCall(&DeviceSuspendHandler::TryOpenDevice);
  }

 private:
  class DeviceSuspendHandler {
   public:
    DeviceSuspendHandler(
        base::WeakPtr<VideoCaptureDeviceChromeOSDelegate> device)
        : device_(std::move(device)) {}

    void TryOpenDevice() {
      if (!device_) {
        return;
      }
      device_->OpenDevice();
    }

    void TryCloseDevice(OnSystemSuspendCallback callback) {
      if (!device_) {
        std::move(callback).Run();
        return;
      }
      device_->CloseDevice(std::move(callback));
    }

   private:
    base::WeakPtr<VideoCaptureDeviceChromeOSDelegate> device_;
  };

  base::SequenceBound<DeviceSuspendHandler> device_suspend_handler_;

  mojo::Remote<cros::mojom::CrosSystemEventMonitor> monitor_;

  mojo::Receiver<cros::mojom::CrosPowerObserver> receiver_{this};
};

VideoCaptureDeviceChromeOSDelegate::VideoCaptureDeviceChromeOSDelegate(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    const VideoCaptureDeviceDescriptor& device_descriptor,
    CameraHalDelegate* camera_hal_delegate,
    base::OnceClosure cleanup_callback)
    : device_descriptor_(device_descriptor),
      camera_hal_delegate_(camera_hal_delegate),
      capture_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      camera_device_ipc_thread_(std::string("CameraDeviceIpcThread") +
                                device_descriptor.device_id),
      lens_facing_(device_descriptor.facing),
      // External cameras have lens_facing as MEDIA_VIDEO_FACING_NONE.
      // We don't want to rotate the frame even if the device rotates.
      rotates_with_device_(lens_facing_ !=
                           VideoFacingMode::MEDIA_VIDEO_FACING_NONE),
      rotation_(0),
      cleanup_callback_(std::move(cleanup_callback)),
      device_closed_(base::WaitableEvent::ResetPolicy::MANUAL,
                     base::WaitableEvent::InitialState::NOT_SIGNALED),
      ui_task_runner_(ui_task_runner) {
  power_observer_ = base::SequenceBound<PowerObserver>(
      ui_task_runner_, weak_ptr_factory_.GetWeakPtr(), capture_task_runner_);
  screen_observer_delegate_ = base::SequenceBound<ScreenObserverDelegate>(
      ui_task_runner_,
      base::BindPostTask(
          capture_task_runner_,
          base::BindRepeating(&VideoCaptureDeviceChromeOSDelegate::SetRotation,
                              weak_ptr_factory_.GetWeakPtr())));
}

VideoCaptureDeviceChromeOSDelegate::~VideoCaptureDeviceChromeOSDelegate() {
  camera_hal_delegate_->DisableAllVirtualDevices();
}

void VideoCaptureDeviceChromeOSDelegate::Shutdown() {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());
  if (!HasDeviceClient()) {
    DCHECK(!camera_device_ipc_thread_.IsRunning());
    // |cleanup_callback_| will call the destructor, so any access to |this|
    // after executing |cleanup_callback_| in this function is unsafe.
    std::move(cleanup_callback_).Run();
  }
}

bool VideoCaptureDeviceChromeOSDelegate::HasDeviceClient() {
  return device_context_ && device_context_->HasClient();
}

void VideoCaptureDeviceChromeOSDelegate::AllocateAndStart(
    const VideoCaptureParams& params,
    std::unique_ptr<VideoCaptureDevice::Client> client,
    ClientType client_type) {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());
  if (!HasDeviceClient()) {
    TRACE_EVENT("camera", "Start Device");
    if (!camera_device_ipc_thread_.Start()) {
      std::string error_msg = "Failed to start device thread";
      LOG(ERROR) << error_msg;
      client->OnError(
          media::VideoCaptureError::kCrosHalV3FailedToStartDeviceThread,
          FROM_HERE, error_msg);
      return;
    }

    device_context_ = std::make_unique<CameraDeviceContext>();
    if (device_context_->AddClient(client_type, std::move(client))) {
      capture_params_[client_type] = params;
      camera_device_delegate_ = std::make_unique<CameraDeviceDelegate>(
          device_descriptor_, camera_hal_delegate_,
          camera_device_ipc_thread_.task_runner(), ui_task_runner_);
      OpenDevice();
    }
    CameraAppDeviceBridgeImpl::GetInstance()->OnVideoCaptureDeviceCreated(
        device_descriptor_.device_id, camera_device_ipc_thread_.task_runner());
  } else {
    if (device_context_->AddClient(client_type, std::move(client))) {
      capture_params_[client_type] = params;
      ReconfigureStreams();
    }
  }
}

void VideoCaptureDeviceChromeOSDelegate::StopAndDeAllocate(
    ClientType client_type) {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());
  DCHECK(camera_device_delegate_);
  if (device_context_) {
    device_context_->RemoveClient(client_type);
    camera_device_ipc_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&CameraDeviceDelegate::OnAllBufferRetired,
                       camera_device_delegate_->GetWeakPtr(), client_type));
  }
  if (!HasDeviceClient()) {
    CloseDevice(base::DoNothing());
    CameraAppDeviceBridgeImpl::GetInstance()->OnVideoCaptureDeviceClosing(
        device_descriptor_.device_id);
    camera_device_ipc_thread_.task_runner()->DeleteSoon(
        FROM_HERE, std::move(camera_device_delegate_));
    camera_device_ipc_thread_.Stop();
    device_context_.reset();
  }
}

void VideoCaptureDeviceChromeOSDelegate::TakePhoto(
    VideoCaptureDevice::TakePhotoCallback callback) {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());
  DCHECK(camera_device_delegate_);
  camera_device_ipc_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&CameraDeviceDelegate::TakePhoto,
                                camera_device_delegate_->GetWeakPtr(),
                                std::move(callback)));
}

void VideoCaptureDeviceChromeOSDelegate::GetPhotoState(
    VideoCaptureDevice::GetPhotoStateCallback callback) {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());
  camera_device_ipc_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&CameraDeviceDelegate::GetPhotoState,
                                camera_device_delegate_->GetWeakPtr(),
                                std::move(callback)));
}

void VideoCaptureDeviceChromeOSDelegate::SetPhotoOptions(
    mojom::PhotoSettingsPtr settings,
    VideoCaptureDevice::SetPhotoOptionsCallback callback) {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());
  camera_device_ipc_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&CameraDeviceDelegate::SetPhotoOptions,
                                camera_device_delegate_->GetWeakPtr(),
                                std::move(settings), std::move(callback)));
}

void VideoCaptureDeviceChromeOSDelegate::OpenDevice() {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());

  if (!camera_device_delegate_) {
    return;
  }
  // It's safe to pass unretained |device_context_| here since
  // VideoCaptureDeviceChromeOSDelegate owns |camera_device_delegate_| and makes
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

void VideoCaptureDeviceChromeOSDelegate::ReconfigureStreams() {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());
  DCHECK(camera_device_delegate_);

  camera_device_ipc_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&CameraDeviceDelegate::ReconfigureStreams,
                     camera_device_delegate_->GetWeakPtr(), capture_params_));
  camera_device_ipc_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&CameraDeviceDelegate::SetRotation,
                     camera_device_delegate_->GetWeakPtr(), rotation_));
}

void VideoCaptureDeviceChromeOSDelegate::CloseDevice(
    base::OnceClosure suspend_callback) {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());

  if (!camera_device_delegate_) {
    std::move(suspend_callback).Run();
    return;
  }
  // We do our best to allow the camera HAL cleanly shut down the device.  In
  // general we don't trust the camera HAL so if the device does not close in
  // time we simply terminate the Mojo channel by resetting
  // |camera_device_delegate_|.
  //
  // VideoCaptureDeviceChromeOSDelegate owns both |camera_device_delegate_| and
  // |device_closed_| and it stops |camera_device_ipc_thread_| in
  // StopAndDeAllocate, so it's safe to pass |device_closed_| as unretained in
  // the callback.
  device_closed_.Reset();
  camera_device_ipc_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&CameraDeviceDelegate::StopAndDeAllocate,
                                camera_device_delegate_->GetWeakPtr(),
                                base::BindOnce(
                                    [](base::WaitableEvent* device_closed) {
                                      device_closed->Signal();
                                    },
                                    base::Unretained(&device_closed_))));
  // TODO(kamesan): Reduce the timeout back to 1 second when we have a solution
  // in platform level (b/258048698).
  const int kWaitTimeoutSecs = 2;
  bool is_signaled = device_closed_.TimedWait(base::Seconds(kWaitTimeoutSecs));
  if (!is_signaled) {
    LOG(WARNING) << "Camera "
                 << camera_hal_delegate_->GetCameraIdFromDeviceId(
                        device_descriptor_.device_id)
                 << " can't be closed in " << kWaitTimeoutSecs << " seconds.";
  }

  std::move(suspend_callback).Run();
}

void VideoCaptureDeviceChromeOSDelegate::SetRotation(int rotation) {
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
