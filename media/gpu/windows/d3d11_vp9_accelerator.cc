// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d11_vp9_accelerator.h"

#include <windows.h>
#include <string>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "media/gpu/windows/d3d11_vp9_picture.h"

namespace media {

using DecodeStatus = VP9Decoder::VP9Accelerator::Status;

#define RETURN_ON_HR_FAILURE(expr_name, expr, code)                           \
  do {                                                                        \
    HRESULT expr_value = (expr);                                              \
    if (FAILED(expr_value)) {                                                 \
      RecordFailure(#expr_name, logging::SystemErrorCodeToString(expr_value), \
                    code);                                                    \
      return false;                                                           \
    }                                                                         \
  } while (0)

std::vector<D3D11_VIDEO_DECODER_SUB_SAMPLE_MAPPING_BLOCK>
CreateSubsampleMappingBlock(const std::vector<SubsampleEntry>& from) {
  std::vector<D3D11_VIDEO_DECODER_SUB_SAMPLE_MAPPING_BLOCK> to(from.size());
  for (const auto& entry : from) {
    D3D11_VIDEO_DECODER_SUB_SAMPLE_MAPPING_BLOCK subsample = {
        .ClearSize = entry.clear_bytes, .EncryptedSize = entry.cypher_bytes};
    to.push_back(subsample);
  }
  return to;
}

D3D11VP9Accelerator::D3D11VP9Accelerator(
    D3D11VideoDecoderClient* client,
    MediaLog* media_log,
    ComD3D11VideoDevice video_device,
    std::unique_ptr<VideoContextWrapper> video_context)
    : client_(client),
      media_log_(media_log),
      status_feedback_(0),
      video_device_(std::move(video_device)),
      video_context_(std::move(video_context)) {
  DCHECK(client);
  DCHECK(media_log_);
  client->SetDecoderCB(base::BindRepeating(
      &D3D11VP9Accelerator::SetVideoDecoder, base::Unretained(this)));
}

D3D11VP9Accelerator::~D3D11VP9Accelerator() {}

void D3D11VP9Accelerator::RecordFailure(const std::string& fail_type,
                                        D3D11Status error) {
  RecordFailure(fail_type, error.message(), error.code());
}

void D3D11VP9Accelerator::RecordFailure(const std::string& fail_type,
                                        const std::string& reason,
                                        D3D11Status::Codes code) {
  MEDIA_LOG(ERROR, media_log_)
      << "DX11VP9Failure(" << fail_type << ")=" << reason;
}

scoped_refptr<VP9Picture> D3D11VP9Accelerator::CreateVP9Picture() {
  D3D11PictureBuffer* picture_buffer = client_->GetPicture();
  if (!picture_buffer)
    return nullptr;
  return base::MakeRefCounted<D3D11VP9Picture>(picture_buffer, client_);
}

bool D3D11VP9Accelerator::BeginFrame(const D3D11VP9Picture& pic) {
  const bool is_encrypted = pic.decrypt_config();
  if (is_encrypted) {
    RecordFailure("crypto_config",
                  "Cannot find the decrypt context for the frame.",
                  D3D11Status::Codes::kCryptoConfigFailed);
    return false;
  }

  HRESULT hr;
  do {
    ID3D11VideoDecoderOutputView* output_view = nullptr;
    auto result = pic.picture_buffer()->AcquireOutputView();
    if (result.has_value()) {
      output_view = std::move(result).value();
    } else {
      D3D11Status error = std::move(result).error();
      RecordFailure("AcquireOutputView", error.message(), error.code());
      return false;
    }

    hr = video_context_->DecoderBeginFrame(video_decoder_.Get(), output_view, 0,
                                           nullptr);
  } while (hr == E_PENDING || hr == D3DERR_WASSTILLDRAWING);

  if (FAILED(hr)) {
    RecordFailure("DecoderBeginFrame", logging::SystemErrorCodeToString(hr),
                  D3D11Status::Codes::kDecoderBeginFrameFailed);
    return false;
  }

  return true;
}

void D3D11VP9Accelerator::CopyFrameParams(const D3D11VP9Picture& pic,
                                          DXVA_PicParams_VP9* pic_params) {
#define SET_PARAM(a, b) pic_params->a = pic.frame_hdr->b
#define COPY_PARAM(a) SET_PARAM(a, a)

  COPY_PARAM(profile);
  COPY_PARAM(show_frame);
  COPY_PARAM(error_resilient_mode);
  COPY_PARAM(refresh_frame_context);
  COPY_PARAM(frame_parallel_decoding_mode);
  COPY_PARAM(intra_only);
  COPY_PARAM(frame_context_idx);
  COPY_PARAM(reset_frame_context);
  COPY_PARAM(allow_high_precision_mv);
  COPY_PARAM(frame_parallel_decoding_mode);
  COPY_PARAM(intra_only);
  COPY_PARAM(frame_context_idx);
  COPY_PARAM(allow_high_precision_mv);

  // extra_plane is initialized to zero.

  pic_params->BitDepthMinus8Luma = pic_params->BitDepthMinus8Chroma =
      pic.frame_hdr->bit_depth - 8;

  pic_params->CurrPic.Index7Bits = pic.picture_index();
  pic_params->frame_type = !pic.frame_hdr->IsKeyframe();

  COPY_PARAM(subsampling_x);
  COPY_PARAM(subsampling_y);

  SET_PARAM(width, frame_width);
  SET_PARAM(height, frame_height);
  SET_PARAM(interp_filter, interpolation_filter);
  SET_PARAM(log2_tile_cols, tile_cols_log2);
  SET_PARAM(log2_tile_rows, tile_rows_log2);
#undef COPY_PARAM
#undef SET_PARAM

  // This is taken, approximately, from libvpx.
  gfx::Size this_frame_size(pic.frame_hdr->frame_width,
                            pic.frame_hdr->frame_height);
  pic_params->use_prev_in_find_mv_refs = last_frame_size_ == this_frame_size &&
                                         !pic.frame_hdr->error_resilient_mode &&
                                         !pic.frame_hdr->intra_only &&
                                         last_show_frame_;

  // TODO(liberato): So, uh, do we ever need to reset this?
  last_frame_size_ = this_frame_size;
  last_show_frame_ = pic.frame_hdr->show_frame;
}

void D3D11VP9Accelerator::CopyReferenceFrames(
    const D3D11VP9Picture& pic,
    DXVA_PicParams_VP9* pic_params,
    const Vp9ReferenceFrameVector& ref_frames) {
  D3D11_TEXTURE2D_DESC texture_descriptor;
  pic.picture_buffer()->Texture()->GetDesc(&texture_descriptor);

  for (size_t i = 0; i < std::size(pic_params->ref_frame_map); i++) {
    auto ref_pic = ref_frames.GetFrame(i);
    if (ref_pic) {
      scoped_refptr<D3D11VP9Picture> our_ref_pic(
          static_cast<D3D11VP9Picture*>(ref_pic.get()));
      pic_params->ref_frame_map[i].Index7Bits = our_ref_pic->picture_index();
      pic_params->ref_frame_coded_width[i] = texture_descriptor.Width;
      pic_params->ref_frame_coded_height[i] = texture_descriptor.Height;
    } else {
      pic_params->ref_frame_map[i].bPicEntry = 0xff;
      pic_params->ref_frame_coded_width[i] = 0;
      pic_params->ref_frame_coded_height[i] = 0;
    }
  }
}

void D3D11VP9Accelerator::CopyFrameRefs(DXVA_PicParams_VP9* pic_params,
                                        const D3D11VP9Picture& pic) {
  for (size_t i = 0; i < std::size(pic_params->frame_refs); i++) {
    pic_params->frame_refs[i] =
        pic_params->ref_frame_map[pic.frame_hdr->ref_frame_idx[i]];
  }

  for (size_t i = 0; i < std::size(pic_params->ref_frame_sign_bias); i++) {
    pic_params->ref_frame_sign_bias[i] = pic.frame_hdr->ref_frame_sign_bias[i];
  }
}

void D3D11VP9Accelerator::CopyLoopFilterParams(
    DXVA_PicParams_VP9* pic_params,
    const Vp9LoopFilterParams& loop_filter_params) {
#define SET_PARAM(a, b) pic_params->a = loop_filter_params.b
  SET_PARAM(filter_level, level);
  SET_PARAM(sharpness_level, sharpness);
  SET_PARAM(mode_ref_delta_enabled, delta_enabled);
  SET_PARAM(mode_ref_delta_update, delta_update);
#undef SET_PARAM

  // std::size(...) doesn't work well in an array initializer.
  DCHECK_EQ(4lu, std::size(pic_params->ref_deltas));
  for (size_t i = 0; i < std::size(pic_params->ref_deltas); i++) {
    // The update_ref_deltas[i] is _only_ for parsing! it allows omission of the
    // 6 bytes that would otherwise be needed for a new value to overwrite the
    // global one. It has nothing to do with setting the ref_deltas here.
    pic_params->ref_deltas[i] = loop_filter_params.ref_deltas[i];
  }

  DCHECK_EQ(2lu, std::size(pic_params->mode_deltas));
  for (size_t i = 0; i < std::size(pic_params->mode_deltas); i++) {
    pic_params->mode_deltas[i] = loop_filter_params.mode_deltas[i];
  }
}

void D3D11VP9Accelerator::CopyQuantParams(DXVA_PicParams_VP9* pic_params,
                                          const D3D11VP9Picture& pic) {
#define SET_PARAM(a, b) pic_params->a = pic.frame_hdr->quant_params.b
  SET_PARAM(base_qindex, base_q_idx);
  SET_PARAM(y_dc_delta_q, delta_q_y_dc);
  SET_PARAM(uv_dc_delta_q, delta_q_uv_dc);
  SET_PARAM(uv_ac_delta_q, delta_q_uv_ac);
#undef SET_PARAM
}

void D3D11VP9Accelerator::CopySegmentationParams(
    DXVA_PicParams_VP9* pic_params,
    const Vp9SegmentationParams& segmentation_params) {
#define SET_PARAM(a, b) pic_params->stVP9Segments.a = segmentation_params.b
#define COPY_PARAM(a) SET_PARAM(a, a)
  COPY_PARAM(enabled);
  COPY_PARAM(update_map);
  COPY_PARAM(temporal_update);
  SET_PARAM(abs_delta, abs_or_delta_update);

  for (size_t i = 0; i < std::size(segmentation_params.tree_probs); i++) {
    COPY_PARAM(tree_probs[i]);
  }

  for (size_t i = 0; i < std::size(segmentation_params.pred_probs); i++) {
    COPY_PARAM(pred_probs[i]);
  }

  for (size_t i = 0; i < 8; i++) {
    for (size_t j = 0; j < 4; j++) {
      COPY_PARAM(feature_data[i][j]);
      if (segmentation_params.feature_enabled[i][j])
        pic_params->stVP9Segments.feature_mask[i] |= (1 << j);
    }
  }
#undef COPY_PARAM
#undef SET_PARAM
}

void D3D11VP9Accelerator::CopyHeaderSizeAndID(DXVA_PicParams_VP9* pic_params,
                                              const D3D11VP9Picture& pic) {
  pic_params->uncompressed_header_size_byte_aligned =
      static_cast<USHORT>(pic.frame_hdr->uncompressed_header_size);
  pic_params->first_partition_size =
      static_cast<USHORT>(pic.frame_hdr->header_size_in_bytes);

  // StatusReportFeedbackNumber "should not be equal to 0".
  pic_params->StatusReportFeedbackNumber = ++status_feedback_;
}

bool D3D11VP9Accelerator::SubmitDecoderBuffer(
    const DXVA_PicParams_VP9& pic_params,
    const D3D11VP9Picture& pic) {
#define GET_BUFFER(type, code)                                                 \
  RETURN_ON_HR_FAILURE(GetDecoderBuffer,                                       \
                       video_context_->GetDecoderBuffer(                       \
                           video_decoder_.Get(), type, &buffer_size, &buffer), \
                       code)
#define RELEASE_BUFFER(type, code) \
  RETURN_ON_HR_FAILURE(            \
      ReleaseDecoderBuffer,        \
      video_context_->ReleaseDecoderBuffer(video_decoder_.Get(), type), code)

  UINT buffer_size;
  void* buffer;

  GET_BUFFER(D3D11_VIDEO_DECODER_BUFFER_PICTURE_PARAMETERS,
             D3D11Status::Codes::kGetPicParamBufferFailed);
  memcpy(buffer, &pic_params, sizeof(pic_params));
  RELEASE_BUFFER(D3D11_VIDEO_DECODER_BUFFER_PICTURE_PARAMETERS,
                 D3D11Status::Codes::kReleasePicParamBufferFailed);

  size_t buffer_offset = 0;
  while (buffer_offset < pic.frame_hdr->frame_size) {
    GET_BUFFER(D3D11_VIDEO_DECODER_BUFFER_BITSTREAM,
               D3D11Status::Codes::kGetBitstreamBufferFailed);
    size_t copy_size = pic.frame_hdr->frame_size - buffer_offset;
    bool contains_end = true;
    if (copy_size > buffer_size) {
      copy_size = buffer_size;
      contains_end = false;
    }
    memcpy(buffer, pic.frame_hdr->data + buffer_offset, copy_size);
    RELEASE_BUFFER(D3D11_VIDEO_DECODER_BUFFER_BITSTREAM,
                   D3D11Status::Codes::kReleaseBitstreamBufferFailed);

    DXVA_Slice_VPx_Short slice_info;

    GET_BUFFER(D3D11_VIDEO_DECODER_BUFFER_SLICE_CONTROL,
               D3D11Status::Codes::kGetSliceControlBufferFailed);
    slice_info.BSNALunitDataLocation = 0;
    slice_info.SliceBytesInBuffer = (UINT)copy_size;

    // See the DXVA header specification for values of wBadSliceChopping:
    // https://docs.microsoft.com/en-us/windows-hardware/drivers/ddi/content/dxva/ns-dxva-_dxva_sliceinfo#wBadSliceChopping
    if (buffer_offset == 0 && contains_end)
      slice_info.wBadSliceChopping = 0;
    else if (buffer_offset == 0 && !contains_end)
      slice_info.wBadSliceChopping = 1;
    else if (buffer_offset != 0 && contains_end)
      slice_info.wBadSliceChopping = 2;
    else if (buffer_offset != 0 && !contains_end)
      slice_info.wBadSliceChopping = 3;

    memcpy(buffer, &slice_info, sizeof(slice_info));
    RELEASE_BUFFER(D3D11_VIDEO_DECODER_BUFFER_SLICE_CONTROL,
                   D3D11Status::Codes::kReleaseSliceControlBufferFailed);

    constexpr int buffers_count = 3;
    VideoContextWrapper::VideoBufferWrapper buffers[buffers_count] = {};
    buffers[0].BufferType = D3D11_VIDEO_DECODER_BUFFER_PICTURE_PARAMETERS;
    buffers[0].DataOffset = 0;
    buffers[0].DataSize = sizeof(pic_params);
    buffers[1].BufferType = D3D11_VIDEO_DECODER_BUFFER_SLICE_CONTROL;
    buffers[1].DataOffset = 0;
    buffers[1].DataSize = sizeof(slice_info);
    buffers[2].BufferType = D3D11_VIDEO_DECODER_BUFFER_BITSTREAM;
    buffers[2].DataOffset = 0;
    buffers[2].DataSize = copy_size;

    const DecryptConfig* config = pic.decrypt_config();
    if (config) {
      buffers[2].pIV = const_cast<char*>(config->iv().data());
      buffers[2].IVSize = config->iv().size();
      // Subsamples matter iff there is IV, for decryption.
      if (!config->subsamples().empty()) {
        buffers[2].pSubSampleMappingBlock =
            CreateSubsampleMappingBlock(config->subsamples()).data();
        buffers[2].SubSampleMappingCount = config->subsamples().size();
      }
    }

    RETURN_ON_HR_FAILURE(SubmitDecoderBuffers,
                         video_context_->SubmitDecoderBuffers(
                             video_decoder_.Get(), buffers_count, buffers),
                         D3D11Status::Codes::kSubmitDecoderBuffersFailed);
    buffer_offset += copy_size;
  }

  return true;
#undef GET_BUFFER
#undef RELEASE_BUFFER
}

DecodeStatus D3D11VP9Accelerator::SubmitDecode(
    scoped_refptr<VP9Picture> picture,
    const Vp9SegmentationParams& segmentation_params,
    const Vp9LoopFilterParams& loop_filter_params,
    const Vp9ReferenceFrameVector& reference_frames,
    base::OnceClosure on_finished_cb) {
  D3D11VP9Picture* pic = static_cast<D3D11VP9Picture*>(picture.get());

  if (!BeginFrame(*pic))
    return DecodeStatus::kFail;

  DXVA_PicParams_VP9 pic_params = {};
  CopyFrameParams(*pic, &pic_params);
  CopyReferenceFrames(*pic, &pic_params, reference_frames);
  CopyFrameRefs(&pic_params, *pic);
  CopyLoopFilterParams(&pic_params, loop_filter_params);
  CopyQuantParams(&pic_params, *pic);
  CopySegmentationParams(&pic_params, segmentation_params);
  CopyHeaderSizeAndID(&pic_params, *pic);

  if (!SubmitDecoderBuffer(pic_params, *pic))
    return DecodeStatus::kFail;

  HRESULT hr = video_context_->DecoderEndFrame(video_decoder_.Get());
  if (FAILED(hr)) {
    RecordFailure("DecoderEndFrame", logging::SystemErrorCodeToString(hr),
                  D3D11Status::Codes::kDecoderEndFrameFailed);
    return DecodeStatus::kFail;
  }

  if (on_finished_cb)
    std::move(on_finished_cb).Run();
  return DecodeStatus::kOk;
}

bool D3D11VP9Accelerator::OutputPicture(scoped_refptr<VP9Picture> picture) {
  D3D11VP9Picture* pic = static_cast<D3D11VP9Picture*>(picture.get());
  return client_->OutputResult(picture.get(), pic->picture_buffer());
}

bool D3D11VP9Accelerator::NeedsCompressedHeaderParsed() const {
  return false;
}

bool D3D11VP9Accelerator::GetFrameContext(scoped_refptr<VP9Picture> picture,
                                          Vp9FrameContext* frame_context) {
  return false;
}

void D3D11VP9Accelerator::SetVideoDecoder(ComD3D11VideoDecoder video_decoder) {
  video_decoder_ = std::move(video_decoder);
}

}  // namespace media
