// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_SCOPED_BUFFER_POOL_RESERVATION_H_
#define MEDIA_CAPTURE_VIDEO_SCOPED_BUFFER_POOL_RESERVATION_H_

#include "media/capture/capture_export.h"
#include "media/capture/video/video_capture_buffer_pool.h"
#include "media/capture/video/video_capture_device_client.h"

namespace media {

template <typename ReleaseTraits>
class CAPTURE_EXPORT ScopedBufferPoolReservation
    : public VideoCaptureDevice::Client::Buffer::ScopedAccessPermission {
 public:
  ScopedBufferPoolReservation(scoped_refptr<VideoCaptureBufferPool> buffer_pool,
                              int buffer_id)
      : buffer_pool_(std::move(buffer_pool)), buffer_id_(buffer_id) {}

  ~ScopedBufferPoolReservation() {
    ReleaseTraits::Release(buffer_pool_, buffer_id_);
  }

 private:
  const scoped_refptr<VideoCaptureBufferPool> buffer_pool_;
  const int buffer_id_;
};

class CAPTURE_EXPORT ProducerReleaseTraits {
 public:
  static void Release(const scoped_refptr<VideoCaptureBufferPool>& buffer_pool,
                      int buffer_id) {
    buffer_pool->RelinquishProducerReservation(buffer_id);
  }
};

class CAPTURE_EXPORT ConsumerReleaseTraits {
 public:
  static void Release(const scoped_refptr<VideoCaptureBufferPool>& buffer_pool,
                      int buffer_id) {
    buffer_pool->RelinquishConsumerHold(buffer_id, 1 /* num_clients */);
  }
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_SCOPED_BUFFER_POOL_RESERVATION_H_
