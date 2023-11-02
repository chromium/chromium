// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_BLOB_UTILS_H_
#define MEDIA_CAPTURE_VIDEO_BLOB_UTILS_H_

#include "media/capture/mojom/image_capture.mojom.h"

namespace media {

struct VideoCaptureFormat;

// Helper method to create a mojom::Blob out of |buffer|, whose pixel format and
// resolution are taken from |capture_format|. |rotation| is the clockwise
// rotation to be applied to the frame. The value can only be 0, 90, 180 or 270.
// It's only effective if |capture_format| is PIXEL_FORMAT_MJPEG.
// Returns a null BlobPtr in case of error.
mojom::BlobPtr RotateAndBlobify(const uint8_t* buffer,
                                const uint32_t bytesused,
                                const VideoCaptureFormat& capture_format,
                                const int rotation);

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_BLOB_UTILS_H_
