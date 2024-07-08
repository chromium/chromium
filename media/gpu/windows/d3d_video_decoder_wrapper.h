// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_D3D_VIDEO_DECODER_WRAPPER_H_
#define MEDIA_GPU_WINDOWS_D3D_VIDEO_DECODER_WRAPPER_H_

#include <Windows.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "media/gpu/windows/d3d11_status.h"
#include "media/gpu/windows/scoped_d3d_buffers.h"

namespace media {

class MediaLog;
class D3D11PictureBuffer;

class D3DVideoDecoderWrapper {
 public:
  enum class BufferType {
    kPictureParameters,
    kInverseQuantizationMatrix,
    kSliceControl,
    kBitstream,
  };

  explicit D3DVideoDecoderWrapper(MediaLog* media_log);
  virtual ~D3DVideoDecoderWrapper();

  // Get whether single video decoder texture is recommended by the driver.
  // Returns whether this operation succeeds.
  virtual std::optional<bool> UseSingleTexture() const = 0;

  // Clear all internal states.
  virtual void Reset() = 0;

  // Start a frame and wait for the hardware to be ready for decoding.
  virtual bool WaitForFrameBegins(D3D11PictureBuffer* output_picture) = 0;

  // Returns whether a buffer of the |type| has already been done in this frame.
  // If so, re-copying same data could be avoided.
  virtual bool HasPendingBuffer(BufferType type) = 0;

  // Submit a slice.
  virtual bool SubmitSlice() = 0;

  // Submit a frame to start decoding.
  virtual bool SubmitDecode() = 0;

  ScopedRandomAccessD3DInputBuffer GetPictureParametersBuffer(
      uint32_t desired_size);
  ScopedRandomAccessD3DInputBuffer GetInverseQuantizationMatrixBuffer(
      uint32_t desired_size);
  ScopedRandomAccessD3DInputBuffer GetSliceControlBuffer(uint32_t desired_size);

  // Get the sequentially writable shared bitstream buffer. The buffer will be
  // owned by the D3DVideoDecoderWrapper, shared across multiple calls, and be
  // automatically submitted before the slice ends. When the buffer is full, the
  // caller should call SubmitSlice(), then call this method for another
  // time to get a new unused bitstream buffer.
  ScopedSequenceD3DInputBuffer& GetBitstreamBuffer(uint32_t desired_size);

  // Append the |start_code| and the |bitstream| to the bitstream buffer. When
  // the buffer is full, the |bitstream| will be chopped, then the buffer will
  // be submitted, together with the slice data for each (possibly bad) chops.
  // The remaining |bitstream| will be append to the new empty buffer.
  // The typename |DXVASliceData| could be any short-form slice control
  // structure.
  // Note: This helper method will do the GetSliceControlBuffer() and
  // GetBitstreamBuffer(), you should not do it again before SubmitSlice().
  template <typename DXVASliceData>
  bool AppendBitstreamAndSliceDataWithStartCode(
      base::span<const uint8_t> bitstream,
      base::span<const uint8_t> start_code = {});

 private:
  // Calls SubmitSlice() and GetBitstreamBuffer() to empty `bitstream_buffer_`.
  bool SubmitAndGetBitstreamBuffer(size_t needed_size);

 protected:
  virtual std::unique_ptr<ScopedD3DBuffer> GetBuffer(BufferType type,
                                                     uint32_t desired_size) = 0;

  // Information that's accumulated during slices and submitted at the end
  std::vector<uint8_t> slice_info_bytes_;
  std::optional<ScopedSequenceD3DInputBuffer> bitstream_buffer_;

  raw_ptr<MediaLog> media_log_ = nullptr;
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_D3D_VIDEO_DECODER_WRAPPER_H_
