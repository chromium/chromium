// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_HRD_BUFFER_H_
#define MEDIA_GPU_HRD_BUFFER_H_

#include <stdint.h>

#include "base/time/time.h"
#include "media/gpu/media_gpu_export.h"

namespace media {

// Model of Hypothetical Reference Decoder (HRD) Buffer
// It's a model of a leaky bucket, where the data flows out of the buffer at a
// constant bitrate, while the data is filled in after every encoded frame. The
// encoding loop should call HRD Buffer methods in the following order:
// ...
// Shrink(timestamp); <- optional
// ...
// AddFrameBytes(frame_bytes, frame_timestamp);
// ...
class MEDIA_GPU_EXPORT HRDBuffer {
 public:
  // A basic constructor. Buffer size and bitrate are specified while the
  // internal buffer state is set to default values. `buffer_size` is in bytes,
  // `avg_bitrate` is in bits per second.
  HRDBuffer(size_t buffer_size, uint32_t avg_bitrate);

  // A constructor where the initial internal buffer state is specified: the
  // size of the buffer and the timestamp when the last frame was added. It is
  // suitable to be used when an encoding layer is added or removed in SVC case.
  HRDBuffer(size_t buffer_size,
            uint32_t avg_bitrate,
            int last_frame_buffer_bytes,
            base::TimeDelta last_frame_timestamp);
  ~HRDBuffer();

  HRDBuffer(const HRDBuffer& other) = delete;
  HRDBuffer& operator=(const HRDBuffer& other) = delete;

  size_t buffer_size() const { return buffer_size_; }
  uint32_t average_bitrate() const { return avg_bitrate_; }

  // Indicates whether the buffer was overridden when the latest frame bytes
  // were added to the buffer.
  bool frame_overshooting() const { return frame_overshooting_; }

  // Number of bytes in the buffer after the last encoded frame has been added.
  int last_frame_buffer_bytes() const { return last_frame_buffer_bytes_; }

  // The timestamp of the last added frame.
  base::TimeDelta last_frame_timestamp() const { return last_frame_timestamp_; }

  // The buffer fullness at the provided time. It is non-negative and equals
  // zero in the case of buffer undershoot.
  int GetBytesAtTime(base::TimeDelta timestamp) const;

  // Space left in the buffer at the provided time. It is non-negative and
  // equals zero in the case of buffer undershoot.
  int GetBytesRemainingAtTime(base::TimeDelta timestamp) const;

  // Sets the HRD buffer main parameters: buffer size and output data
  // bitrate. `peak_bitrate` and `ease_hrd_reduction` parameters are used
  // in smooth size transition, when the buffer size is reduced.
  void SetParameters(size_t buffer_size,
                     uint32_t avg_bitrate,
                     uint32_t peak_bitrate,
                     bool ease_hrd_reduction);

  // The method shrinks the buffer size gradually until it reaches the
  // previously set buffer size. This method could be called in every encoding
  // cycle.
  void Shrink(base::TimeDelta timestamp);

  // Adds encoded bytes to the buffer.
  void AddFrameBytes(size_t frame_bytes, base::TimeDelta frame_timestamp);

 private:
  // Buffer size in bytes after the last frame has been added.
  int last_frame_buffer_bytes_ = 0;
  // Timestamp of the last added frame.
  base::TimeDelta last_frame_timestamp_ = base::Microseconds(-1);

  // Current HRD buffer size in bytes.
  size_t buffer_size_ = 0;
  // New HRD buffer size in bytes - when the buffer size changes.
  size_t new_buffer_size_ = 0;

  // Stream bitrate used in HRD model.
  uint32_t avg_bitrate_ = 0;

  // Frame overshooting indicator.
  bool frame_overshooting_ = false;

  // Shrinking duration of HRD buffer to a new, smaller size.
  base::TimeDelta shrinking_bucket_wait_time_ = base::Microseconds(0);
  // Delta rate used in the buffer size change.
  int buffer_size_delta_rate_ = 0;
};

}  // namespace media

#endif  // MEDIA_GPU_HRD_BUFFER_H_
