// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d11_vp9_accelerator.h"

#include <windows.h>
#include <string>
#include <utility>

#include "base/memory/ptr_util.h"
#include "media/cdm/cdm_proxy_context.h"
#include "media/gpu/windows/d3d11_vp9_picture.h"

namespace media {

#define RETURN_ON_HR_FAILURE(expr_name, expr)                                  \
  do {                                                                         \
    HRESULT expr_value = (expr);                                               \
    if (FAILED(expr_value)) {                                                  \
      RecordFailure(#expr_name, logging::SystemErrorCodeToString(expr_value)); \
      return false;                                                            \
    }                                                                          \
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
    CdmProxyContext* cdm_proxy_context,
    ComD3D11VideoDecoder video_decoder,
    ComD3D11VideoDevice video_device,
    std::unique_ptr<VideoContextWrapper> video_context)
    : client_(client),
      media_log_(media_log),
      cdm_proxy_context_(cdm_proxy_context),
      status_feedback_(0),
      video_decoder_(std::move(video_decoder)),
      video_device_(std::move(video_device)),
      video_context_(std::move(video_context)) {
  DCHECK(client);
  DCHECK(media_log_);
  // |cdm_proxy_context_| is non-null for encrypted content but can be null for
  // clear content.
}

D3D11VP9Accelerator::~D3D11VP9Accelerator() {}

void D3D11VP9Accelerator::RecordFailure(const std::string& fail_type,
                                        const std::string& reason) {
  media_log_->AddEvent(media_log_->CreateStringEvent(
      MediaLogEvent::MEDIA_ERROR_LOG_ENTRY, "error",
      std::string("DX11VP9Failure(") + fail_type + ")=" + reason));
}

scoped_refptr<VP9Picture> D3D11VP9Accelerator::CreateVP9Picture() {
  D3D11PictureBuffer* picture_buffer = client_->GetPicture();
  if (!picture_buffer)
    return nullptr;
  return base::MakeRefCounted<D3D11VP9Picture>(picture_buffer);
}

bool D3D11VP9Accelerator::BeginFrame(const D3D11VP9Picture& pic) {
  // This |decrypt_context| has to be outside the if block because pKeyInfo in
  // D3D11_VIDEO_DECODER_BEGIN_FRAME_CRYPTO_SESSION is a pointer (to a GUID).
  base::Optional<CdmProxyContext::D3D11DecryptContext> decrypt_context;
  std::unique_ptr<D3D11_VIDEO_DECODER_BEGIN_FRAME_CRYPTO_SESSION> content_key;
  if (const DecryptConfig* config = pic.decrypt_config()) {
    DCHECK(cdm_proxy_context_) << "No CdmProxyContext but picture is encrypted";
    decrypt_context = cdm_proxy_context_->GetD3D11DecryptContext(
        CdmProxy::KeyType::kDecryptAndDecode, config->key_id());
    if (!decrypt_context) {
      RecordFailure("crypto_config",
                    "Cannot find the decrypt context for the frame.");
      return false;  // TODO(crbug.com/894573): support kTryAgain.
    }

    content_key =
        std::make_unique<D3D11_VIDEO_DECODER_BEGIN_FRAME_CRYPTO_SESSION>();
    content_key->pCryptoSession = decrypt_context->crypto_session;
    content_key->pBlob = const_cast<void*>(decrypt_context->key_blob);
    content_key->BlobSize = decrypt_context->key_blob_size;
    content_key->pKeyInfoId = &decrypt_context->key_info_guid;
  }

  HRESULT hr;
  do {
    hr = video_context_->DecoderBeginFrame(
        video_decoder_.Get(), pic.picture_buffer()->output_view().Get(),
        content_key ? sizeof(*content_key) : 0, content_key.get());
  } while (hr == E_PENDING || hr == D3DERR_WASSTILLDRAWING);

  if (FAILED(hr)) {
    RecordFailure("DecoderBeginFrame", logging::SystemErrorCodeToString(hr));
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
  COPY_PARAM(refresh_frame_context);
  COPY_PARAM(frame_parallel_decoding_mode);
  COPY_PARAM(intra_only);
  COPY_PARAM(frame_context_idx);
  COPY_PARAM(allow_high_precision_mv);

  // extra_plane is initialized to zero.

  pic_params->BitDepthMinus8Luma = pic_params->BitDepthMinus8Chroma =
      pic.frame_hdr->bit_depth - 8;

  pic_params->CurrPic.Index7Bits = pic.level();
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
}

void D3D11VP9Accelerator::CopyReferenceFrames(
    const D3D11VP9Picture& pic,
    DXVA_PicParams_VP9* pic_params,
    const Vp9ReferenceFrameVector& ref_frames) {
  D3D11_TEXTURE2D_DESC texture_descriptor;
  pic.picture_buffer()->Texture()->GetDesc(&texture_descriptor);

  for (size_t i = 0; i < base::size(pic_params->ref_frame_map); i++) {
    auto ref_pic = ref_frames.GetFrame(i);
    if (ref_pic) {
      scoped_refptr<D3D11VP9Picture> our_ref_pic(
          static_cast<D3D11VP9Picture*>(ref_pic.get()));
      pic_params->ref_frame_map[i].Index7Bits = our_ref_pic->level();
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
  for (size_t i = 0; i < base::size(pic_params->frame_refs); i++) {
    pic_params->frame_refs[i] =
        pic_params->ref_frame_map[pic.frame_hdr->ref_frame_idx[i]];
  }

  for (size_t i = 0; i < base::size(pic_params->ref_frame_sign_bias); i++) {
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

  // base::size(...) doesn't work well in an array initializer.
  DCHECK_EQ(4lu, base::size(pic_params->ref_deltas));
  int ref_deltas[4] = {0};
  for (size_t i = 0; i < base::size(pic_params->ref_deltas); i++) {
    if (loop_filter_params.update_ref_deltas[i])
      ref_deltas[i] = loop_filter_params.ref_deltas[i];
    pic_params->ref_deltas[i] = ref_deltas[i];
  }

  int mode_deltas[2] = {0};
  DCHECK_EQ(2lu, base::size(pic_params->mode_deltas));
  for (size_t i = 0; i < base::size(pic_params->mode_deltas); i++) {
    if (loop_filter_params.update_mode_deltas[i])
      mode_deltas[i] = loop_filter_params.mode_deltas[i];
    pic_params->mode_deltas[i] = mode_deltas[i];
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

  for (size_t i = 0; i < base::size(segmentation_params.tree_probs); i++) {
    COPY_PARAM(tree_probs[i]);
  }

  for (size_t i = 0; i < base::size(segmentation_params.pred_probs); i++) {
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
#define GET_BUFFER(type)                                 \
  RETURN_ON_HR_FAILURE(GetDecoderBuffer,                 \
                       video_context_->GetDecoderBuffer( \
                           video_decoder_.Get(), type, &buffer_size, &buffer))
#define RELEASE_BUFFER(type) \
  RETURN_ON_HR_FAILURE(      \
      ReleaseDecoderBuffer,  \
      video_context_->ReleaseDecoderBuffer(video_decoder_.Get(), type))

  UINT buffer_size;
  void* buffer;

  GET_BUFFER(D3D11_VIDEO_DECODER_BUFFER_PICTURE_PARAMETERS);
  memcpy(buffer, &pic_params, sizeof(pic_params));
  RELEASE_BUFFER(D3D11_VIDEO_DECODER_BUFFER_PICTURE_PARAMETERS);

  size_t buffer_offset = 0;
  while (buffer_offset < pic.frame_hdr->frame_size) {
    GET_BUFFER(D3D11_VIDEO_DECODER_BUFFER_BITSTREAM);
    size_t copy_size = pic.frame_hdr->frame_size - buffer_offset;
    bool contains_end = true;
    if (copy_size > buffer_size) {
      copy_size = buffer_size;
      contains_end = false;
    }
    memcpy(buffer, pic.frame_hdr->data + buffer_offset, copy_size);
    RELEASE_BUFFER(D3D11_VIDEO_DECODER_BUFFER_BITSTREAM);

    DXVA_Slice_VPx_Short slice_info;

    GET_BUFFER(D3D11_VIDEO_DECODER_BUFFER_SLICE_CONTROL);
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
    RELEASE_BUFFER(D3D11_VIDEO_DECODER_BUFFER_SLICE_CONTROL);

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
                             video_decoder_.Get(), buffers_count, buffers));
    buffer_offset += copy_size;
  }

  return true;
#undef GET_BUFFER
#undef RELEASE_BUFFER
}

bool D3D11VP9Accelerator::SubmitDecode(
    scoped_refptr<VP9Picture> picture,
    const Vp9SegmentationParams& segmentation_params,
    const Vp9LoopFilterParams& loop_filter_params,
    const Vp9ReferenceFrameVector& reference_frames,
    const base::Closure& on_finished_cb) {
  D3D11VP9Picture* pic = static_cast<D3D11VP9Picture*>(picture.get());

  if (!BeginFrame(*pic))
    return false;

  DXVA_PicParams_VP9 pic_params = {};
  CopyFrameParams(*pic, &pic_params);
  CopyReferenceFrames(*pic, &pic_params, reference_frames);
  CopyFrameRefs(&pic_params, *pic);
  CopyLoopFilterParams(&pic_params, loop_filter_params);
  CopyQuantParams(&pic_params, *pic);
  CopySegmentationParams(&pic_params, segmentation_params);
  CopyHeaderSizeAndID(&pic_params, *pic);

  if (!SubmitDecoderBuffer(pic_params, *pic))
    return false;

  RETURN_ON_HR_FAILURE(DecoderEndFrame,
                       video_context_->DecoderEndFrame(video_decoder_.Get()));
  if (on_finished_cb)
    on_finished_cb.Run();
  return true;
}

bool D3D11VP9Accelerator::OutputPicture(scoped_refptr<VP9Picture> picture) {
  D3D11VP9Picture* pic = static_cast<D3D11VP9Picture*>(picture.get());
  client_->OutputResult(picture.get(), pic->picture_buffer());
  return true;
}

bool D3D11VP9Accelerator::IsFrameContextRequired() const {
  return false;
}

bool D3D11VP9Accelerator::GetFrameContext(scoped_refptr<VP9Picture> picture,
                                          Vp9FrameContext* frame_context) {
  return false;
}

}  // namespace media
