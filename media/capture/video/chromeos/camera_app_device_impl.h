// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_APP_DEVICE_IMPL_H_
#define MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_APP_DEVICE_IMPL_H_

#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "media/capture/capture_export.h"
#include "media/capture/mojom/image_capture.mojom.h"
#include "media/capture/video/chromeos/mojom/camera3.mojom.h"
#include "media/capture/video/chromeos/mojom/camera_app.mojom.h"
#include "media/capture/video/chromeos/mojom/camera_common.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/range/range.h"

namespace media {

struct ReprocessTask {
 public:
  ReprocessTask();
  ReprocessTask(ReprocessTask&& other);
  ~ReprocessTask();
  cros::mojom::Effect effect;
  cros::mojom::CameraAppDevice::SetReprocessOptionCallback callback;
  std::vector<cros::mojom::CameraMetadataEntryPtr> extra_metadata;
};

using ReprocessTaskQueue = base::queue<ReprocessTask>;

// TODO(shik): Get the keys from VendorTagOps by names instead (b/130774415).
constexpr uint32_t kPortraitModeVendorKey = 0x80000000;
constexpr uint32_t kPortraitModeSegmentationResultVendorKey = 0x80000001;
constexpr int32_t kReprocessSuccess = 0;

// Implementation of CameraAppDevice that is used as the ommunication bridge
// between Chrome Camera App (CCA) and the ChromeOS Video Capture Device. By
// using this, we can do more complicated operations on cameras which is not
// supported by Chrome API.
class CAPTURE_EXPORT CameraAppDeviceImpl : public cros::mojom::CameraAppDevice {
 public:
  // Retrieve the return code for reprocess |effect| from the |metadata|.
  static int GetReprocessReturnCode(
      cros::mojom::Effect effect,
      const cros::mojom::CameraMetadataPtr* metadata);

  // Construct a ReprocessTaskQueue for regular capture with
  // |take_photo_callback|.
  static ReprocessTaskQueue GetSingleShotReprocessOptions(
      media::mojom::ImageCapture::TakePhotoCallback take_photo_callback);

  CameraAppDeviceImpl(const std::string& device_id,
                      cros::mojom::CameraInfoPtr camera_info);
  ~CameraAppDeviceImpl() override;

  // Binds the mojo receiver to this implementation.
  void BindReceiver(
      mojo::PendingReceiver<cros::mojom::CameraAppDevice> receiver);

  // All the weak pointers should be dereferenced and invalidated on the camera
  // device ipc thread.
  base::WeakPtr<CameraAppDeviceImpl> GetWeakPtr();

  void InvalidatePtrs(base::OnceClosure callback);

  // Consumes all the pending reprocess tasks if there is any and eventually
  // generates a ReprocessTaskQueue which contains:
  //   1. A regular capture task with |take_photo_callback|.
  //   2. One or more reprocess tasks if there is any.
  // And passes the generated ReprocessTaskQueue through |consumption_callback|.
  void ConsumeReprocessOptions(
      media::mojom::ImageCapture::TakePhotoCallback take_photo_callback,
      base::OnceCallback<void(ReprocessTaskQueue)> consumption_callback);

  // Retrieves the fps range if it is specified by the app.
  base::Optional<gfx::Range> GetFpsRange();

  // Retrieves the corresponding capture resolution which is specified by the
  // app.
  gfx::Size GetStillCaptureResolution();

  // Gets the capture intent which is specified by the app.
  cros::mojom::CaptureIntent GetCaptureIntent();

  // Delivers the result |metadata| with its |stream_type| to the metadata
  // observers.
  void OnResultMetadataAvailable(const cros::mojom::CameraMetadataPtr& metadata,
                                 const cros::mojom::StreamType stream_type);

  // Notifies the camera event observers that the shutter is finished.
  void OnShutterDone();

  // cros::mojom::CameraAppDevice implementations.
  void GetCameraInfo(GetCameraInfoCallback callback) override;
  void SetReprocessOption(cros::mojom::Effect effect,
                          SetReprocessOptionCallback callback) override;
  void SetFpsRange(const gfx::Range& fps_range,
                   SetFpsRangeCallback callback) override;
  void SetStillCaptureResolution(
      const gfx::Size& resolution,
      SetStillCaptureResolutionCallback callback) override;
  void SetCaptureIntent(cros::mojom::CaptureIntent capture_intent,
                        SetCaptureIntentCallback callback) override;
  void AddResultMetadataObserver(
      mojo::PendingRemote<cros::mojom::ResultMetadataObserver> observer,
      cros::mojom::StreamType streamType,
      AddResultMetadataObserverCallback callback) override;
  void RemoveResultMetadataObserver(
      uint32_t id,
      RemoveResultMetadataObserverCallback callback) override;
  void AddCameraEventObserver(
      mojo::PendingRemote<cros::mojom::CameraEventObserver> observer,
      AddCameraEventObserverCallback callback) override;
  void RemoveCameraEventObserver(
      uint32_t id,
      RemoveCameraEventObserverCallback callback) override;

 private:
  static void DisableEeNr(ReprocessTask* task);

  void OnMojoConnectionError();

  void SetReprocessResultOnMojoThread(SetReprocessOptionCallback callback,
                                      const int32_t status,
                                      media::mojom::BlobPtr blob);

  void NotifyShutterDoneOnMojoThread();

  std::string device_id_;

  mojo::ReceiverSet<cros::mojom::CameraAppDevice> receivers_;

  cros::mojom::CameraInfoPtr camera_info_;

  // It is used for calls which should run on the mojo thread.
  scoped_refptr<base::SingleThreadTaskRunner> mojo_task_runner_;

  // The queue will be enqueued and dequeued from different threads.
  base::Lock reprocess_tasks_lock_;
  base::queue<ReprocessTask> reprocess_task_queue_
      GUARDED_BY(reprocess_tasks_lock_);

  // It will be inserted and read from different threads.
  base::Lock fps_ranges_lock_;
  base::Optional<gfx::Range> specified_fps_range_ GUARDED_BY(fps_ranges_lock_);

  // It will be inserted and read from different threads.
  base::Lock still_capture_resolution_lock_;
  gfx::Size still_capture_resolution_
      GUARDED_BY(still_capture_resolution_lock_);

  // It will be modified and read from different threads.
  base::Lock capture_intent_lock_;
  cros::mojom::CaptureIntent capture_intent_ GUARDED_BY(capture_intent_lock_);

  // Those maps will be changed and used from different threads.
  base::Lock metadata_observers_lock_;
  uint32_t next_metadata_observer_id_ GUARDED_BY(metadata_observers_lock_);
  base::flat_map<uint32_t, mojo::Remote<cros::mojom::ResultMetadataObserver>>
      metadata_observers_ GUARDED_BY(metadata_observers_lock_);
  base::flat_map<cros::mojom::StreamType, base::flat_set<uint32_t>>
      stream_metadata_observer_ids_ GUARDED_BY(metadata_observers_lock_);

  uint32_t next_camera_event_observer_id_;
  base::flat_map<uint32_t, mojo::Remote<cros::mojom::CameraEventObserver>>
      camera_event_observers_;

  // The weak pointers should be dereferenced and invalidated on camera device
  // ipc thread.
  base::WeakPtrFactory<CameraAppDeviceImpl> weak_ptr_factory_{this};

  // The weak pointers should be dereferenced and invalidated on the Mojo
  // thread.
  base::WeakPtrFactory<CameraAppDeviceImpl> weak_ptr_factory_for_mojo_{this};

  DISALLOW_COPY_AND_ASSIGN(CameraAppDeviceImpl);
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_APP_DEVICE_IMPL_H_
