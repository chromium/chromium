// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef MEDIA_CAPTURE_VIDEO_CHROMEOS_CAPTURE_METADATA_DISPATCHER_H_
#define MEDIA_CAPTURE_VIDEO_CHROMEOS_CAPTURE_METADATA_DISPATCHER_H_

#include "media/capture/capture_export.h"
#include "media/capture/video/chromeos/mojom/camera_common.mojom.h"

namespace media {

// Interface that provides API to let Camera3AController and
// CameraDeviceDelegate to update the metadata that will be sent with capture
// request.
class CAPTURE_EXPORT CaptureMetadataDispatcher {
 public:
  class ResultMetadataObserver {
   public:
    virtual ~ResultMetadataObserver() {}
    virtual void OnResultMetadataAvailable(
        uint32_t frame_number,
        const cros::mojom::CameraMetadataPtr&) = 0;
  };

  virtual ~CaptureMetadataDispatcher() {}
  virtual void AddResultMetadataObserver(ResultMetadataObserver* observer) = 0;
  virtual void RemoveResultMetadataObserver(
      ResultMetadataObserver* observer) = 0;
  virtual void SetCaptureMetadata(cros::mojom::CameraMetadataTag tag,
                                  cros::mojom::EntryType type,
                                  size_t count,
                                  std::vector<uint8_t> value) = 0;
  virtual void SetRepeatingCaptureMetadata(cros::mojom::CameraMetadataTag tag,
                                           cros::mojom::EntryType type,
                                           size_t count,
                                           std::vector<uint8_t> value) = 0;
  virtual void UnsetRepeatingCaptureMetadata(
      cros::mojom::CameraMetadataTag tag) = 0;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_CAPTURE_METADATA_DISPATCHER_H_
