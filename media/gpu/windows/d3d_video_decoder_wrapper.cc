// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d_video_decoder_wrapper.h"

#include <dxva.h>

#include "base/strings/string_number_conversions.h"
#include "media/base/media_log.h"

namespace media {

D3DVideoDecoderWrapper::D3DVideoDecoderWrapper(MediaLog* media_log)
    : media_log_(media_log) {}

D3DVideoDecoderWrapper::~D3DVideoDecoderWrapper() = default;

ScopedRandomAccessD3DInputBuffer
D3DVideoDecoderWrapper::GetPictureParametersBuffer(uint32_t desired_size) {
  DCHECK(!HasPendingBuffer(BufferType::kPictureParameters));
  return ScopedRandomAccessD3DInputBuffer(
      GetBuffer(BufferType::kPictureParameters, desired_size));
}

ScopedRandomAccessD3DInputBuffer
D3DVideoDecoderWrapper::GetInverseQuantizationMatrixBuffer(
    uint32_t desired_size) {
  DCHECK(!HasPendingBuffer(BufferType::kInverseQuantizationMatrix));
  return ScopedRandomAccessD3DInputBuffer(
      GetBuffer(BufferType::kInverseQuantizationMatrix, desired_size));
}

ScopedRandomAccessD3DInputBuffer D3DVideoDecoderWrapper::GetSliceControlBuffer(
    uint32_t desired_size) {
  DCHECK(!HasPendingBuffer(BufferType::kSliceControl));
  return ScopedRandomAccessD3DInputBuffer(
      GetBuffer(BufferType::kSliceControl, desired_size));
}

ScopedSequenceD3DInputBuffer& D3DVideoDecoderWrapper::GetBitstreamBuffer(
    uint32_t desired_size) {
  // Reuse the bitstream buffer before the slices are submitted.
  if (!bitstream_buffer_) {
    bitstream_buffer_.emplace(GetBuffer(BufferType::kBitstream, desired_size));
  }
  return bitstream_buffer_.value();
}

template <typename DXVASliceData>
bool D3DVideoDecoderWrapper::AppendBitstreamAndSliceDataWithStartCode(
    base::span<const uint8_t> bitstream,
    base::span<const uint8_t> start_code) {
  // Ideally all slices in a frame are put in the same bitstream buffer.
  // However the bitstream buffer may not fit all the data, so split on the
  // necessary boundaries.
  size_t total_size = start_code.size() + bitstream.size();
  auto& bitstream_buffer = GetBitstreamBuffer(total_size);
  if (bitstream_buffer.empty()) {
    return false;
  }
  size_t data_offset = 0;
  while (data_offset < bitstream.size()) {
    uint32_t bytes_submitted = 0;
    uint32_t buffer_offset = bitstream_buffer.BytesWritten();
    bool contains_start = data_offset == 0;
    if (contains_start && start_code.size() > 0) {
      if (bitstream_buffer.BytesAvailable() < start_code.size()) {
        if (!SubmitSlice() || GetBitstreamBuffer(total_size).empty()) {
          return false;
        }
      }
      bytes_submitted += bitstream_buffer.Write(start_code);
    }
    uint32_t data_bytes_submitted =
        bitstream_buffer.Write(bitstream.subspan(data_offset));
    bytes_submitted += data_bytes_submitted;
    data_offset += data_bytes_submitted;
    bool contains_end = data_offset == bitstream.size();
    DXVASliceData slice_info{
        .BSNALunitDataLocation = buffer_offset,
        .SliceBytesInBuffer = bytes_submitted,
        .wBadSliceChopping = static_cast<USHORT>((contains_start ? 0 : 2) |
                                                 (contains_end ? 0 : 1))};

    uint8_t* byte_ptr = reinterpret_cast<uint8_t*>(&slice_info);
    slice_info_bytes_.insert(slice_info_bytes_.end(), byte_ptr,
                             byte_ptr + sizeof(slice_info));
  }

  return true;
}

template bool D3DVideoDecoderWrapper::AppendBitstreamAndSliceDataWithStartCode<
    DXVA_Slice_H264_Short>(base::span<const uint8_t> bitstream,
                           base::span<const uint8_t> start_code);

template bool D3DVideoDecoderWrapper::AppendBitstreamAndSliceDataWithStartCode<
    DXVA_Slice_HEVC_Short>(base::span<const uint8_t> bitstream,
                           base::span<const uint8_t> start_code);

template bool D3DVideoDecoderWrapper::AppendBitstreamAndSliceDataWithStartCode<
    DXVA_Slice_VPx_Short>(base::span<const uint8_t> bitstream,
                          base::span<const uint8_t> start_code);

void D3DVideoDecoderWrapper::RecordFailure(base::StringPiece reason,
                                           D3D11Status::Codes code) const {
  DLOG(ERROR) << reason;
  MEDIA_LOG(ERROR, media_log_) << reason;
}

void D3DVideoDecoderWrapper::RecordFailure(base::StringPiece reason,
                                           D3D11Status::Codes code,
                                           HRESULT hr) const {
  DCHECK(FAILED(hr));
  std::string hr_string = logging::SystemErrorCodeToString(hr);
  if (!base::IsStringUTF8AllowingNoncharacters(hr_string)) {
    hr_string = "WARNING: system message could not be rendered!";
  }
  DLOG(ERROR) << reason << ": " << hr_string;
  MEDIA_LOG(ERROR, media_log_) << reason << ": " << hr_string;
}

}  // namespace media
