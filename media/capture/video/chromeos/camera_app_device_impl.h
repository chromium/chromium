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

class CAPTURE_EXPORT CameraAppDeviceImpl : public cros::mojom::CameraAppDevice {
 public:
  struct SizeComparator {
    bool operator()(const gfx::Size& size_1, const gfx::Size& size_2) const;
  };

  using GetFpsRangeCallback =
      base::OnceCallback<void(base::Optional<gfx::Range>)>;

  using ResolutionFpsRangeMap =
      base::flat_map<gfx::Size, gfx::Range, SizeComparator>;

  static int GetReprocessReturnCode(
      cros::mojom::Effect effect,
      const cros::mojom::CameraMetadataPtr* metadata);

  static ReprocessTaskQueue GetSingleShotReprocessOptions(
      media::mojom::ImageCapture::TakePhotoCallback take_photo_callback);

  CameraAppDeviceImpl(const std::string& device_id,
                      cros::mojom::CameraInfoPtr camera_info);
  ~CameraAppDeviceImpl() override;

  void BindReceiver(
      mojo::PendingReceiver<cros::mojom::CameraAppDevice> receiver);

  void ConsumeReprocessOptions(
      media::mojom::ImageCapture::TakePhotoCallback take_photo_callback,
      base::OnceCallback<void(ReprocessTaskQueue)> consumption_callback);

  void GetFpsRange(const gfx::Size& resolution, GetFpsRangeCallback callback);

  cros::mojom::CaptureIntent GetCaptureIntent();

  void OnResultMetadataAvailable(const cros::mojom::CameraMetadataPtr& metadata,
                                 const cros::mojom::StreamType stream_type);

  void OnShutterDone();

  void SetReprocessResult(SetReprocessOptionCallback callback,
                          const int32_t status,
                          media::mojom::BlobPtr blob);

  // cros::mojom::CameraAppDevice implementations.
  void GetCameraInfo(GetCameraInfoCallback callback) override;

  void SetReprocessOption(cros::mojom::Effect effect,
                          SetReprocessOptionCallback callback) override;

  void SetFpsRange(const gfx::Size& resolution,
                   const gfx::Range& fps_range,
                   SetFpsRangeCallback callback) override;

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
  std::string device_id_;

  mojo::ReceiverSet<cros::mojom::CameraAppDevice> receivers_;

  cros::mojom::CameraInfoPtr camera_info_;

  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  base::Lock reprocess_tasks_lock_;

  base::queue<ReprocessTask> reprocess_task_queue_;

  base::Lock fps_ranges_lock_;

  ResolutionFpsRangeMap resolution_fps_range_map_;

  base::Lock capture_intent_lock_;

  cros::mojom::CaptureIntent capture_intent_;

  base::Lock metadata_observers_lock_;

  base::Lock camera_event_observers_lock_;

  uint32_t next_metadata_observer_id_;

  uint32_t next_camera_event_observer_id_;

  base::flat_map<uint32_t, mojo::Remote<cros::mojom::ResultMetadataObserver>>
      metadata_observers_;

  base::flat_map<cros::mojom::StreamType, base::flat_set<uint32_t>>
      stream_metadata_observer_ids_;

  base::flat_map<uint32_t, mojo::Remote<cros::mojom::CameraEventObserver>>
      camera_event_observers_;

  std::unique_ptr<base::WeakPtrFactory<CameraAppDeviceImpl>> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CameraAppDeviceImpl);
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_APP_DEVICE_IMPL_H_
