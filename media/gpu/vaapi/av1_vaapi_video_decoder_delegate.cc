// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/av1_vaapi_video_decoder_delegate.h"

#include <string.h>
#include <va/va.h>
#include <algorithm>
#include <vector>

#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "media/gpu/av1_picture.h"
#include "media/gpu/decode_surface_handler.h"
#include "media/gpu/vaapi/vaapi_common.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "third_party/libgav1/src/src/obu_parser.h"
#include "third_party/libgav1/src/src/utils/types.h"

namespace media {
namespace {
bool FillAV1PictureParameter(const AV1Picture& pic,
                             const libgav1::ObuSequenceHeader& seq_header,
                             const AV1ReferenceFrameVector& ref_frames,
                             VADecPictureParameterBufferAV1& pic_param) {
  memset(&pic_param, 0, sizeof(VADecPictureParameterBufferAV1));
  NOTIMPLEMENTED();
  return false;
}

bool FillAV1SliceParameters(
    const libgav1::Vector<libgav1::TileBuffer>& tile_buffers,
    const size_t tile_columns,
    base::span<const uint8_t> data,
    std::vector<VASliceParameterBufferAV1>& slice_params) {
  NOTIMPLEMENTED();
  return false;
}
}  // namespace

AV1VaapiVideoDecoderDelegate::AV1VaapiVideoDecoderDelegate(
    DecodeSurfaceHandler<VASurface>* const vaapi_dec,
    scoped_refptr<VaapiWrapper> vaapi_wrapper)
    : VaapiVideoDecoderDelegate(vaapi_dec, std::move(vaapi_wrapper)) {}

AV1VaapiVideoDecoderDelegate::~AV1VaapiVideoDecoderDelegate() = default;

scoped_refptr<AV1Picture> AV1VaapiVideoDecoderDelegate::CreateAV1Picture(
    bool apply_grain) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto display_va_surface = vaapi_dec_->CreateSurface();
  if (!display_va_surface)
    return nullptr;

  auto reconstruct_va_surface = display_va_surface;
  if (apply_grain) {
    // TODO(hiroh): When no surface is available here, this returns nullptr and
    // |display_va_surface| is released. Since the surface is back to the pool,
    // VaapiVideoDecoder will detect that there are surfaces available and will
    // start another decode task which means that CreateSurface() might fail
    // again for |reconstruct_va_surface| since only one surface might have gone
    // back to the pool (the one for |display_va_surface|). We should avoid this
    // loop for the sake of efficiency.
    reconstruct_va_surface = vaapi_dec_->CreateSurface();
    if (!reconstruct_va_surface)
      return nullptr;
  }

  return base::MakeRefCounted<VaapiAV1Picture>(
      std::move(display_va_surface), std::move(reconstruct_va_surface));
}

bool AV1VaapiVideoDecoderDelegate::OutputPicture(const AV1Picture& pic) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto* vaapi_pic = static_cast<const VaapiAV1Picture*>(&pic);
  vaapi_dec_->SurfaceReady(vaapi_pic->display_va_surface(),
                           vaapi_pic->bitstream_id(), vaapi_pic->visible_rect(),
                           vaapi_pic->get_colorspace());
  return true;
}

bool AV1VaapiVideoDecoderDelegate::SubmitDecode(
    const AV1Picture& pic,
    const libgav1::ObuSequenceHeader& seq_header,
    const AV1ReferenceFrameVector& ref_frames,
    const libgav1::Vector<libgav1::TileBuffer>& tile_buffers,
    base::span<const uint8_t> data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // libgav1 ensures that tile_columns is >= 0 and <= MAX_TILE_COLS.
  DCHECK_LE(0, pic.frame_header.tile_info.tile_columns);
  DCHECK_LE(pic.frame_header.tile_info.tile_columns, libgav1::kMaxTileColumns);
  const size_t tile_columns =
      base::checked_cast<size_t>(pic.frame_header.tile_info.tile_columns);

  VADecPictureParameterBufferAV1 pic_param;
  std::vector<VASliceParameterBufferAV1> slice_params;
  if (!FillAV1PictureParameter(pic, seq_header, ref_frames, pic_param) ||
      !FillAV1SliceParameters(tile_buffers, tile_columns, data, slice_params)) {
    return false;
  }

  // TODO(hiroh): Batch VABuffer submissions like Vp9VaapiVideoDecoderDelegate.
  // Submit the picture parameters.
  if (!vaapi_wrapper_->SubmitBuffer(VAPictureParameterBufferType, &pic_param))
    return false;

  // Submit the entire buffer and the per-tile information.
  // TODO(hiroh): Don't submit the entire coded data to the buffer. Instead,
  // only pass the data starting from the tile list OBU to reduce the size of
  // the VA buffer.
  if (!vaapi_wrapper_->SubmitBuffer(VASliceDataBufferType, data.size(),
                                    data.data())) {
    return false;
  }
  for (const VASliceParameterBufferAV1& tile_param : slice_params) {
    if (!vaapi_wrapper_->SubmitBuffer(VASliceParameterBufferType,
                                      &tile_param)) {
      return false;
    }
  }

  const auto* vaapi_pic = static_cast<const VaapiAV1Picture*>(&pic);
  return vaapi_wrapper_->ExecuteAndDestroyPendingBuffers(
      vaapi_pic->display_va_surface()->id());
}
}  // namespace media
