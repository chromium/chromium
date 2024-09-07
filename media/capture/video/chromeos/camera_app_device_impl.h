// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_APP_DEVICE_IMPL_H_
#define MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_APP_DEVICE_IMPL_H_

#include <map>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/queue.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "base/timer/elapsed_timer.h"
#include "media/base/video_transformation.h"
#include "media/capture/capture_export.h"
#include "media/capture/mojom/image_capture.mojom.h"
#include "media/capture/video/chromeos/camera_device_delegate.h"
#include "media/capture/video/chromeos/mojom/camera3.mojom.h"
#include "media/capture/video/chromeos/mojom/camera_app.mojom.h"
#include "media/capture/video/chromeos/mojom/camera_common.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/range/range.h"

namespace gpu {

class GpuMemoryBufferImpl;

}  // namespace gpu

namespace media {

class CameraDeviceContext;

// TODO(shik): Get the keys from VendorTagOps by names instead (b/130774415).
constexpr uint32_t kPortraitModeVendorKey = 0x80000000;
constexpr uint32_t kPortraitModeSegmentationResultVendorKey = 0x80000001;

// Implementation of CameraAppDevice that is used as the communication bridge
// between Chrome Camera App (CCA) and the ChromeOS Video Capture Device. By
// using this, we can do more complicated operations on cameras which is not
// supported by Chrome API.
class CAPTURE_EXPORT CameraAppDeviceImpl : public cros::mojom::CameraAppDevice {
 public:
  // TODO(b/244503017): Add definitions for the portrait mode segmentation
  // result in the mojom file.
  // Retrieve the return code for portrait mode segmentation result from the
  // |metadata|.
  static int GetPortraitSegResultCode(
      const cros::mojom::CameraMetadataPtr* metadata);

  CameraAppDeviceImpl(
      const std::string& device_id,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);

  CameraAppDeviceImpl(const CameraAppDeviceImpl&) = delete;
  CameraAppDeviceImpl& operator=(const CameraAppDeviceImpl&) = delete;

  ~CameraAppDeviceImpl() override;

  // Binds the mojo receiver to this implementation.
  void BindReceiver(
      mojo::PendingReceiver<cros::mojom::CameraAppDevice> receiver);

  // All the weak pointers should be retrieved, dereferenced and invalidated on
  // the camera device ipc thread.
  base::WeakPtr<CameraAppDeviceImpl> GetWeakPtr();

  // Resets things which need to be handled on device IPC thread, including
  // invalidating all the existing weak pointers, and then triggers |callback|.
  // When |should_disable_new_ptrs| is set to true, no more weak pointers can be
  // created. It is used when tearing down the CameraAppDeviceImpl instance.
  void ResetOnDeviceIpcThread(base::OnceClosure callback,
                              bool should_disable_new_ptrs);

  // Retrieves the fps range if it is specified by the app.
  std::optional<gfx::Range> GetFpsRange();

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

  // Notifies the camera info observers that the camera info is updated.
  void OnCameraInfoUpdated(cros::mojom::CameraInfoPtr camera_info);

  // Sets the pointer to the camera device context instance associated with the
  // opened camera.  Used to configure and query camera frame rotation.
  void SetCameraDeviceContext(CameraDeviceContext* device_context);

  // Detect document corners on the frame given by its gpu memory buffer if it
  // is supported.
  void MaybeDetectDocumentCorners(std::unique_ptr<gpu::GpuMemoryBufferImpl> gmb,
                                  VideoRotation rotation);

  bool IsMultipleStreamsEnabled();

  // cros::mojom::CameraAppDevice implementations.
  void TakePortraitModePhoto(
      mojo::PendingRemote<cros::mojom::StillCaptureResultObserver> observer,
      TakePortraitModePhotoCallback callback) override;
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
  void AddCameraEventObserver(
      mojo::PendingRemote<cros::mojom::CameraEventObserver> observer,
      AddCameraEventObserverCallback callback) override;
  void SetCameraFrameRotationEnabledAtSource(
      bool is_enabled,
      SetCameraFrameRotationEnabledAtSourceCallback callback) override;
  void GetCameraFrameRotation(GetCameraFrameRotationCallback callback) override;
  void RegisterDocumentCornersObserver(
      mojo::PendingRemote<cros::mojom::DocumentCornersObserver> observer,
      RegisterDocumentCornersObserverCallback callback) override;
  void SetMultipleStreamsEnabled(
      bool enabled,
      SetMultipleStreamsEnabledCallback callback) override;
  void RegisterCameraInfoObserver(
      mojo::PendingRemote<cros::mojom::CameraInfoObserver> observer,
      RegisterCameraInfoObserverCallback callback) override;
  std::optional<PortraitModeCallbacks> ConsumePortraitModeCallbacks();
  void SetCropRegion(const gfx::Rect& crop_region,
                     SetCropRegionCallback callback) override;
  void ResetCropRegion(ResetCropRegionCallback callback) override;
  std::optional<std::vector<int32_t>> GetCropRegion();

 private:
  class DocumentScanner;

  void OnMojoConnectionError();

  bool IsCloseToPreviousDetectionRequest();

  void DetectDocumentCornersOnMojoThread(
      std::unique_ptr<gpu::GpuMemoryBufferImpl> image,
      VideoRotation rotation);

  void OnDetectedDocumentCornersOnMojoThread(
      VideoRotation rotation,
      bool success,
      const std::vector<gfx::PointF>& corners);

  void NotifyPortraitResultOnMojoThread(cros::mojom::Effect effect,
                                        const int32_t status,
                                        media::mojom::BlobPtr blob);

  void NotifyShutterDoneOnMojoThread();
  void NotifyResultMetadataOnMojoThread(cros::mojom::CameraMetadataPtr metadata,
                                        cros::mojom::StreamType streamType);
  void NotifyCameraInfoUpdatedOnMojoThread();

  std::string device_id_;

  // If it is set to false, no weak pointers for this instance can be generated
  // for IPC thread.
  bool allow_new_ipc_weak_ptrs_;

  mojo::ReceiverSet<cros::mojom::CameraAppDevice> receivers_;

  base::Lock camera_info_lock_;
  cros::mojom::CameraInfoPtr camera_info_ GUARDED_BY(camera_info_lock_);

  // It is used for calls which should run on the mojo thread.
  scoped_refptr<base::SingleThreadTaskRunner> mojo_task_runner_;

  mojo::Remote<cros::mojom::StillCaptureResultObserver>
      portrait_mode_observers_;
  base::Lock portrait_mode_callbacks_lock_;
  std::optional<PortraitModeCallbacks> take_portrait_photo_callbacks_
      GUARDED_BY(portrait_mode_callbacks_lock_);

  // It will be inserted and read from different threads.
  base::Lock fps_ranges_lock_;
  std::optional<gfx::Range> specified_fps_range_ GUARDED_BY(fps_ranges_lock_);

  // It will be inserted and read from different threads.
  base::Lock still_capture_resolution_lock_;
  gfx::Size still_capture_resolution_
      GUARDED_BY(still_capture_resolution_lock_);

  // It will be modified and read from different threads.
  base::Lock capture_intent_lock_;
  cros::mojom::CaptureIntent capture_intent_ GUARDED_BY(capture_intent_lock_);

  // Those maps will be changed and used only on the mojo thread.
  std::map<cros::mojom::StreamType,
           mojo::RemoteSet<cros::mojom::ResultMetadataObserver>>
      stream_to_metadata_observers_map_;

  mojo::RemoteSet<cros::mojom::CameraEventObserver> camera_event_observers_;

  base::Lock camera_device_context_lock_;
  raw_ptr<CameraDeviceContext> camera_device_context_
      GUARDED_BY(camera_device_context_lock_);

  base::Lock document_corners_observers_lock_;
  mojo::RemoteSet<cros::mojom::DocumentCornersObserver>
      document_corners_observers_ GUARDED_BY(document_corners_observers_lock_);
  bool has_ongoing_document_detection_task_ = false;
  std::unique_ptr<base::ElapsedTimer> document_detection_timer_ = nullptr;

  mojo::RemoteSet<cros::mojom::CameraInfoObserver> camera_info_observers_;

  // Client to connect to the CrosDocumentScanner service.
  base::SequenceBound<DocumentScanner> document_scanner_;

  base::Lock multi_stream_lock_;
  bool multi_stream_enabled_ GUARDED_BY(multi_stream_lock_) = false;

  base::Lock crop_region_lock_;
  std::optional<std::vector<int32_t>> crop_region_
      GUARDED_BY(crop_region_lock_);

  // The weak pointers should be dereferenced and invalidated on camera device
  // ipc thread.
  base::WeakPtrFactory<CameraAppDeviceImpl> weak_ptr_factory_{this};

  // The weak pointers should be dereferenced and invalidated on the Mojo
  // thread.
  base::WeakPtrFactory<CameraAppDeviceImpl> weak_ptr_factory_for_mojo_{this};
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_APP_DEVICE_IMPL_H_
