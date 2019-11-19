// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d11_h264_accelerator.h"

#include <windows.h>

#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"
#include "media/base/media_log.h"
#include "media/cdm/cdm_proxy_context.h"
#include "media/gpu/h264_decoder.h"
#include "media/gpu/h264_dpb.h"
#include "media/gpu/windows/d3d11_picture_buffer.h"
#include "media/gpu/windows/return_on_failure.h"
#include "third_party/angle/include/EGL/egl.h"
#include "third_party/angle/include/EGL/eglext.h"
#include "ui/gfx/color_space.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_image_dxgi.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/scoped_binders.h"

namespace media {

using Status = H264Decoder::H264Accelerator::Status;

namespace {

// Converts SubsampleEntry to D3D11_VIDEO_DECODER_SUB_SAMPLE_MAPPING_BLOCK.
void AppendSubsamples(
    const std::vector<SubsampleEntry>& from,
    std::vector<D3D11_VIDEO_DECODER_SUB_SAMPLE_MAPPING_BLOCK>* to) {
  for (const auto& from_entry : from) {
    D3D11_VIDEO_DECODER_SUB_SAMPLE_MAPPING_BLOCK subsample = {};
    subsample.ClearSize = from_entry.clear_bytes;
    subsample.EncryptedSize = from_entry.cypher_bytes;
    to->push_back(subsample);
  }
}

}  // namespace

class D3D11H264Picture : public H264Picture {
 public:
  D3D11H264Picture(D3D11PictureBuffer* picture)
      : picture(picture), level_(picture->level()) {
    picture->set_in_picture_use(true);
  }

  D3D11PictureBuffer* picture;
  size_t level_;

 protected:
  ~D3D11H264Picture() override;
};

D3D11H264Picture::~D3D11H264Picture() {
  picture->set_in_picture_use(false);
}

D3D11H264Accelerator::D3D11H264Accelerator(
    D3D11VideoDecoderClient* client,
    MediaLog* media_log,
    CdmProxyContext* cdm_proxy_context,
    ComD3D11VideoDecoder video_decoder,
    ComD3D11VideoDevice video_device,
    std::unique_ptr<VideoContextWrapper> video_context)
    : client_(client),
      media_log_(media_log),
      cdm_proxy_context_(cdm_proxy_context),
      video_decoder_(video_decoder),
      video_device_(video_device),
      video_context_(std::move(video_context)) {
  DCHECK(client);
  DCHECK(media_log_);
  // |cdm_proxy_context_| is non-null for encrypted content but can be null for
  // clear content.
}

D3D11H264Accelerator::~D3D11H264Accelerator() {}

scoped_refptr<H264Picture> D3D11H264Accelerator::CreateH264Picture() {
  D3D11PictureBuffer* picture = client_->GetPicture();
  if (!picture) {
    return nullptr;
  }
  return base::MakeRefCounted<D3D11H264Picture>(picture);
}

Status D3D11H264Accelerator::SubmitFrameMetadata(
    const H264SPS* sps,
    const H264PPS* pps,
    const H264DPB& dpb,
    const H264Picture::Vector& ref_pic_listp0,
    const H264Picture::Vector& ref_pic_listb0,
    const H264Picture::Vector& ref_pic_listb1,
    scoped_refptr<H264Picture> pic) {
  const bool is_encrypted = pic->decrypt_config();

  std::unique_ptr<D3D11_VIDEO_DECODER_BEGIN_FRAME_CRYPTO_SESSION> content_key;
  // This decrypt context has to be outside the if block because pKeyInfo in
  // D3D11_VIDEO_DECODER_BEGIN_FRAME_CRYPTO_SESSION is a pointer (to a GUID).
  base::Optional<CdmProxyContext::D3D11DecryptContext> decrypt_context;
  if (is_encrypted) {
    DCHECK(cdm_proxy_context_) << "No CdmProxyContext but picture is encrypted";
    decrypt_context = cdm_proxy_context_->GetD3D11DecryptContext(
        CdmProxy::KeyType::kDecryptAndDecode, pic->decrypt_config()->key_id());
    if (!decrypt_context) {
      RecordFailure("Cannot find decrypt context for the frame.");
      return Status::kTryAgain;
    }

    content_key =
        std::make_unique<D3D11_VIDEO_DECODER_BEGIN_FRAME_CRYPTO_SESSION>();
    content_key->pCryptoSession = decrypt_context->crypto_session;
    content_key->pBlob = const_cast<void*>(decrypt_context->key_blob);
    content_key->BlobSize = decrypt_context->key_blob_size;
    content_key->pKeyInfoId = &decrypt_context->key_info_guid;
    frame_iv_.assign(pic->decrypt_config()->iv().begin(),
                     pic->decrypt_config()->iv().end());
  }

  HRESULT hr;
  for (;;) {
    hr = video_context_->DecoderBeginFrame(
        video_decoder_.Get(),
        static_cast<D3D11H264Picture*>(pic.get())->picture->output_view().Get(),
        content_key ? sizeof(*content_key) : 0, content_key.get());

    if (hr == E_PENDING || hr == D3DERR_WASSTILLDRAWING) {
      // Hardware is busy.  We should make the call again.
      // TODO(liberato): For now, just busy wait.
      ;
    } else if (!SUCCEEDED(hr)) {
      RecordFailure("DecoderBeginFrame failed", hr);
      return Status::kFail;
    } else {
      break;
    }
  }

  sps_ = *sps;
  for (size_t i = 0; i < 16; i++) {
    ref_frame_list_[i].bPicEntry = 0xFF;
    field_order_cnt_list_[i][0] = 0;
    field_order_cnt_list_[i][1] = 0;
    frame_num_list_[i] = 0;
  }
  used_for_reference_flags_ = 0;
  non_existing_frame_flags_ = 0;

  // TODO(liberato): this is similar to H264Accelerator.  can they share code?

  int i = 0;
  for (auto it = dpb.begin(); it != dpb.end(); i++, it++) {
    D3D11H264Picture* our_ref_pic = static_cast<D3D11H264Picture*>(it->get());
    if (!our_ref_pic->ref)
      continue;
    ref_frame_list_[i].Index7Bits = our_ref_pic->level_;
    ref_frame_list_[i].AssociatedFlag = our_ref_pic->long_term;
    field_order_cnt_list_[i][0] = our_ref_pic->top_field_order_cnt;
    field_order_cnt_list_[i][1] = our_ref_pic->bottom_field_order_cnt;
    frame_num_list_[i] = ref_frame_list_[i].AssociatedFlag
                             ? our_ref_pic->long_term_pic_num
                             : our_ref_pic->frame_num;
    unsigned ref = 3;
    used_for_reference_flags_ |= ref << (2 * i);
    non_existing_frame_flags_ |= (our_ref_pic->nonexisting) << i;
  }
  slice_info_.clear();
  return RetrieveBitstreamBuffer() ? Status::kOk : Status::kFail;
}

bool D3D11H264Accelerator::RetrieveBitstreamBuffer() {
  DCHECK(!bitstream_buffer_bytes_);
  DCHECK(!bitstream_buffer_size_);

  current_offset_ = 0;
  void* buffer;
  UINT buffer_size;
  HRESULT hr = video_context_->GetDecoderBuffer(
      video_decoder_.Get(), D3D11_VIDEO_DECODER_BUFFER_BITSTREAM, &buffer_size,
      &buffer);
  if (!SUCCEEDED(hr)) {
    RecordFailure("GetDecoderBuffer (Bitstream) failed", hr);
    return false;
  }
  bitstream_buffer_bytes_ = (uint8_t*)buffer;
  bitstream_buffer_size_ = buffer_size;

  return true;
}

void D3D11H264Accelerator::FillPicParamsWithConstants(
    DXVA_PicParams_H264* pic) {
  // From "DirectX Video Acceleration Specification for H.264/AVC Decoding":
  // "The value shall be 1 unless the restricted-mode profile in use
  // explicitly supports the value 0."
  pic->MbsConsecutiveFlag = 1;

  // The latest DXVA decoding guide says to set this to 3 if the software
  // decoder (this class) is following the guide.
  pic->Reserved16Bits = 3;

  // |ContinuationFlag| indicates that we've filled in the remaining fields.
  pic->ContinuationFlag = 1;

  // Must be zero unless bit 13 of ConfigDecoderSpecific is set.
  pic->Reserved8BitsA = 0;

  // Unused, should always be zero.
  pic->Reserved8BitsB = 0;

  // Should always be 1.
  pic->StatusReportFeedbackNumber = 1;

  // UNUSED: slice_group_map_type (undocumented)
  // UNUSED: slice_group_change_rate (undocumented)
}

#define ARG_SEL(_1, _2, NAME, ...) NAME

#define SPS_TO_PP1(a) pic_param->a = sps->a;
#define SPS_TO_PP2(a, b) pic_param->a = sps->b;
#define SPS_TO_PP(...) ARG_SEL(__VA_ARGS__, SPS_TO_PP2, SPS_TO_PP1)(__VA_ARGS__)
void D3D11H264Accelerator::PicParamsFromSPS(DXVA_PicParams_H264* pic_param,
                                            const H264SPS* sps,
                                            bool field_pic) {
  // The H.264 specification now calls this |max_num_ref_frames|, while
  // DXVA_PicParams_H264 continues to use the old name, |num_ref_frames|.
  // See DirectX Video Acceleration for H.264/MPEG-4 AVC Decoding (4.2).
  SPS_TO_PP(num_ref_frames, max_num_ref_frames);
  SPS_TO_PP(wFrameWidthInMbsMinus1, pic_width_in_mbs_minus1);
  SPS_TO_PP(wFrameHeightInMbsMinus1, pic_height_in_map_units_minus1);
  SPS_TO_PP(residual_colour_transform_flag, separate_colour_plane_flag);
  SPS_TO_PP(chroma_format_idc);
  SPS_TO_PP(frame_mbs_only_flag);
  SPS_TO_PP(bit_depth_luma_minus8);
  SPS_TO_PP(bit_depth_chroma_minus8);
  SPS_TO_PP(log2_max_frame_num_minus4);
  SPS_TO_PP(pic_order_cnt_type);
  SPS_TO_PP(log2_max_pic_order_cnt_lsb_minus4);
  SPS_TO_PP(delta_pic_order_always_zero_flag);
  SPS_TO_PP(direct_8x8_inference_flag);

  pic_param->MbaffFrameFlag = sps->mb_adaptive_frame_field_flag && field_pic;
  pic_param->field_pic_flag = field_pic;

  pic_param->MinLumaBipredSize8x8Flag = sps->level_idc >= 31;
}
#undef SPS_TO_PP
#undef SPS_TO_PP2
#undef SPS_TO_PP1

#define PPS_TO_PP1(a) pic_param->a = pps->a;
#define PPS_TO_PP2(a, b) pic_param->a = pps->b;
#define PPS_TO_PP(...) ARG_SEL(__VA_ARGS__, PPS_TO_PP2, PPS_TO_PP1)(__VA_ARGS__)
bool D3D11H264Accelerator::PicParamsFromPPS(DXVA_PicParams_H264* pic_param,
                                            const H264PPS* pps) {
  PPS_TO_PP(constrained_intra_pred_flag);
  PPS_TO_PP(weighted_pred_flag);
  PPS_TO_PP(weighted_bipred_idc);

  PPS_TO_PP(transform_8x8_mode_flag);
  PPS_TO_PP(pic_init_qs_minus26);
  PPS_TO_PP(chroma_qp_index_offset);
  PPS_TO_PP(second_chroma_qp_index_offset);
  PPS_TO_PP(pic_init_qp_minus26);
  PPS_TO_PP(num_ref_idx_l0_active_minus1, num_ref_idx_l0_default_active_minus1);
  PPS_TO_PP(num_ref_idx_l1_active_minus1, num_ref_idx_l1_default_active_minus1);
  PPS_TO_PP(entropy_coding_mode_flag);
  PPS_TO_PP(pic_order_present_flag,
            bottom_field_pic_order_in_frame_present_flag);
  PPS_TO_PP(deblocking_filter_control_present_flag);
  PPS_TO_PP(redundant_pic_cnt_present_flag);

  PPS_TO_PP(num_slice_groups_minus1);
  if (pic_param->num_slice_groups_minus1) {
    // TODO(liberato): UMA?
    // TODO(liberato): media log?
    LOG(ERROR) << "num_slice_groups_minus1 == "
               << pic_param->num_slice_groups_minus1;
    return false;
  }
  return true;
}
#undef PPS_TO_PP
#undef PPS_TO_PP2
#undef PPS_TO_PP1

#undef ARG_SEL

void D3D11H264Accelerator::PicParamsFromSliceHeader(
    DXVA_PicParams_H264* pic_param,
    const H264SliceHeader* slice_hdr) {
  pic_param->sp_for_switch_flag = slice_hdr->sp_for_switch_flag;
  pic_param->field_pic_flag = slice_hdr->field_pic_flag;
  pic_param->CurrPic.AssociatedFlag = slice_hdr->bottom_field_flag;
  pic_param->IntraPicFlag = slice_hdr->IsISlice();
}

void D3D11H264Accelerator::PicParamsFromPic(DXVA_PicParams_H264* pic_param,
                                            scoped_refptr<H264Picture> pic) {
  pic_param->CurrPic.Index7Bits =
      static_cast<D3D11H264Picture*>(pic.get())->level_;
  pic_param->RefPicFlag = pic->ref;
  pic_param->frame_num = pic->frame_num;

  if (pic_param->field_pic_flag && pic_param->CurrPic.AssociatedFlag) {
    pic_param->CurrFieldOrderCnt[1] = pic->bottom_field_order_cnt;
    pic_param->CurrFieldOrderCnt[0] = 0;
  } else if (pic_param->field_pic_flag && !pic_param->CurrPic.AssociatedFlag) {
    pic_param->CurrFieldOrderCnt[0] = pic->top_field_order_cnt;
    pic_param->CurrFieldOrderCnt[1] = 0;
  } else {
    pic_param->CurrFieldOrderCnt[0] = pic->top_field_order_cnt;
    pic_param->CurrFieldOrderCnt[1] = pic->bottom_field_order_cnt;
  }
}

Status D3D11H264Accelerator::SubmitSlice(
    const H264PPS* pps,
    const H264SliceHeader* slice_hdr,
    const H264Picture::Vector& ref_pic_list0,
    const H264Picture::Vector& ref_pic_list1,
    scoped_refptr<H264Picture> pic,
    const uint8_t* data,
    size_t size,
    const std::vector<SubsampleEntry>& subsamples) {
  const bool is_encrypted = pic->decrypt_config();
  DXVA_PicParams_H264 pic_param = {};
  FillPicParamsWithConstants(&pic_param);

  PicParamsFromSPS(&pic_param, &sps_, slice_hdr->field_pic_flag);
  if (!PicParamsFromPPS(&pic_param, pps))
    return Status::kFail;
  PicParamsFromSliceHeader(&pic_param, slice_hdr);
  PicParamsFromPic(&pic_param, std::move(pic));

  memcpy(pic_param.RefFrameList, ref_frame_list_,
         sizeof pic_param.RefFrameList);

  memcpy(pic_param.FieldOrderCntList, field_order_cnt_list_,
         sizeof pic_param.FieldOrderCntList);

  memcpy(pic_param.FrameNumList, frame_num_list_,
         sizeof pic_param.FrameNumList);
  pic_param.UsedForReferenceFlags = used_for_reference_flags_;
  pic_param.NonExistingFrameFlags = non_existing_frame_flags_;

  UINT buffer_size;
  void* buffer;
  HRESULT hr = video_context_->GetDecoderBuffer(
      video_decoder_.Get(), D3D11_VIDEO_DECODER_BUFFER_PICTURE_PARAMETERS,
      &buffer_size, &buffer);
  if (!SUCCEEDED(hr)) {
    RecordFailure("ReleaseDecoderBuffer (PictureParams) failed", hr);
    return Status::kFail;
  }

  memcpy(buffer, &pic_param, sizeof(pic_param));
  hr = video_context_->ReleaseDecoderBuffer(
      video_decoder_.Get(), D3D11_VIDEO_DECODER_BUFFER_PICTURE_PARAMETERS);
  if (!SUCCEEDED(hr)) {
    RecordFailure("ReleaseDecoderBuffer (PictureParams) failed", hr);
    return Status::kFail;
  }

  DXVA_Qmatrix_H264 iq_matrix_buf = {};

  if (pps->pic_scaling_matrix_present_flag) {
    for (int i = 0; i < 6; ++i) {
      for (int j = 0; j < 16; ++j)
        iq_matrix_buf.bScalingLists4x4[i][j] = pps->scaling_list4x4[i][j];
    }

    for (int i = 0; i < 2; ++i) {
      for (int j = 0; j < 64; ++j)
        iq_matrix_buf.bScalingLists8x8[i][j] = pps->scaling_list8x8[i][j];
    }
  } else {
    for (int i = 0; i < 6; ++i) {
      for (int j = 0; j < 16; ++j)
        iq_matrix_buf.bScalingLists4x4[i][j] = sps_.scaling_list4x4[i][j];
    }

    for (int i = 0; i < 2; ++i) {
      for (int j = 0; j < 64; ++j)
        iq_matrix_buf.bScalingLists8x8[i][j] = sps_.scaling_list8x8[i][j];
    }
  }
  hr = video_context_->GetDecoderBuffer(
      video_decoder_.Get(),
      D3D11_VIDEO_DECODER_BUFFER_INVERSE_QUANTIZATION_MATRIX, &buffer_size,
      &buffer);
  if (!SUCCEEDED(hr)) {
    RecordFailure("GetDecoderBuffer (QuantMatrix) failed", hr);
    return Status::kFail;
  }
  memcpy(buffer, &iq_matrix_buf, sizeof(iq_matrix_buf));
  hr = video_context_->ReleaseDecoderBuffer(
      video_decoder_.Get(),
      D3D11_VIDEO_DECODER_BUFFER_INVERSE_QUANTIZATION_MATRIX);
  if (!SUCCEEDED(hr)) {
    RecordFailure("ReleaseDecoderBuffer (QuantMatrix) failed", hr);
    return Status::kFail;
  }

  // Ideally all slices in a frame are put in the same bitstream buffer.
  // However the bitstream buffer may not fit all the data, so split on the
  // necessary boundaries.

  size_t out_bitstream_size = size + 3;

  size_t remaining_bitstream = out_bitstream_size;
  size_t start_location = 0;

  if (is_encrypted) {
    // For now, the entire frame has to fit into the bitstream buffer. This way
    // the subsample ClearSize adjustment below should work.
    if (bitstream_buffer_size_ < remaining_bitstream) {
      RecordFailure("Input slice NALU (" + std::to_string(remaining_bitstream) +
                    ") too big to fit in the bistream buffer (" +
                    std::to_string(bitstream_buffer_size_) + ").");
      return Status::kFail;
    }

    AppendSubsamples(subsamples, &subsamples_);
    if (!subsamples.empty()) {
      // 3 added to clear bytes because a start code is prepended to the slice
      // NALU.
      // TODO(rkuroiwa): This should be done right after the start code is
      // written to the buffer, but currently the start code is written in the
      // loop (which is not the right place, there's only one slice NALU passed
      // into this function) and it's not easy to identify where the subsample
      // starts in the buffer.
      subsamples_[subsamples_.size() - subsamples.size()].ClearSize += 3;
    }
  }

  while (remaining_bitstream > 0) {
    if (bitstream_buffer_size_ < remaining_bitstream &&
        slice_info_.size() > 0) {
      if (!SubmitSliceData()) {
        RecordFailure("SubmitSliceData failed");
        return Status::kFail;
      }

      if (!RetrieveBitstreamBuffer()) {
        RecordFailure("RetrieveBitstreamBuffer failed");
        return Status::kFail;
      }
    }

    size_t bytes_to_copy = remaining_bitstream;
    bool contains_end = true;
    if (bytes_to_copy > bitstream_buffer_size_) {
      bytes_to_copy = bitstream_buffer_size_;
      contains_end = false;
    }
    size_t real_bytes_to_copy = bytes_to_copy;
    // TODO(jbauman): fix hack
    uint8_t* out_start = bitstream_buffer_bytes_;
    if (bytes_to_copy >= 3 && start_location == 0) {
      *(out_start++) = 0;
      *(out_start++) = 0;
      *(out_start++) = 1;
      real_bytes_to_copy -= 3;
    }
    memcpy(out_start, data + start_location, real_bytes_to_copy);

    DXVA_Slice_H264_Short slice_info = {};
    slice_info.BSNALunitDataLocation = (UINT)current_offset_;
    slice_info.SliceBytesInBuffer = (UINT)bytes_to_copy;
    if (contains_end && start_location == 0)
      slice_info.wBadSliceChopping = 0;
    else if (!contains_end && start_location == 0)
      slice_info.wBadSliceChopping = 1;
    else if (contains_end && start_location != 0)
      slice_info.wBadSliceChopping = 2;
    else
      slice_info.wBadSliceChopping = 3;

    slice_info_.push_back(slice_info);
    bitstream_buffer_size_ -= bytes_to_copy;
    current_offset_ += bytes_to_copy;
    start_location += bytes_to_copy;
    remaining_bitstream -= bytes_to_copy;
    bitstream_buffer_bytes_ += bytes_to_copy;
  }

  return Status::kOk;
}

bool D3D11H264Accelerator::SubmitSliceData() {
  CHECK(slice_info_.size() > 0);
  UINT buffer_size;
  void* buffer;

  // TODO(liberato): Should we release the other buffers on failure?

  HRESULT hr = video_context_->GetDecoderBuffer(
      video_decoder_.Get(), D3D11_VIDEO_DECODER_BUFFER_SLICE_CONTROL,
      &buffer_size, &buffer);
  if (!SUCCEEDED(hr)) {
    RecordFailure("GetDecoderBuffer (SliceControl) failed", hr);
    return false;
  }

  CHECK_LE(sizeof(slice_info_[0]) * slice_info_.size(), buffer_size);
  memcpy(buffer, &slice_info_[0], sizeof(slice_info_[0]) * slice_info_.size());
  hr = video_context_->ReleaseDecoderBuffer(
      video_decoder_.Get(), D3D11_VIDEO_DECODER_BUFFER_SLICE_CONTROL);
  if (!SUCCEEDED(hr)) {
    RecordFailure("ReleaseDecoderBuffer (SliceControl) failed", hr);
    return false;
  }

  hr = video_context_->ReleaseDecoderBuffer(
      video_decoder_.Get(), D3D11_VIDEO_DECODER_BUFFER_BITSTREAM);
  if (!SUCCEEDED(hr)) {
    RecordFailure("ReleaseDecoderBuffer (BitStream) failed", hr);
    return false;
  }

  VideoContextWrapper::VideoBufferWrapper buffers[4] = {};
  buffers[0].BufferType = D3D11_VIDEO_DECODER_BUFFER_PICTURE_PARAMETERS;
  buffers[0].DataOffset = 0;
  buffers[0].DataSize = sizeof(DXVA_PicParams_H264);
  buffers[1].BufferType =
      D3D11_VIDEO_DECODER_BUFFER_INVERSE_QUANTIZATION_MATRIX;
  buffers[1].DataOffset = 0;
  buffers[1].DataSize = sizeof(DXVA_Qmatrix_H264);
  buffers[2].BufferType = D3D11_VIDEO_DECODER_BUFFER_SLICE_CONTROL;
  buffers[2].DataOffset = 0;
  buffers[2].DataSize = sizeof(slice_info_[0]) * slice_info_.size();
  buffers[3].BufferType = D3D11_VIDEO_DECODER_BUFFER_BITSTREAM;
  buffers[3].DataOffset = 0;
  buffers[3].DataSize = current_offset_;

  if (!frame_iv_.empty()) {
    buffers[3].pIV = frame_iv_.data();
    buffers[3].IVSize = frame_iv_.size();
    // Subsmaples matter iff there is IV, for decryption.
    if (!subsamples_.empty()) {
      buffers[3].pSubSampleMappingBlock = subsamples_.data();
      buffers[3].SubSampleMappingCount = subsamples_.size();
    }
  }

  hr = video_context_->SubmitDecoderBuffers(video_decoder_.Get(),
                                            base::size(buffers), buffers);
  current_offset_ = 0;
  slice_info_.clear();
  bitstream_buffer_bytes_ = nullptr;
  bitstream_buffer_size_ = 0;
  frame_iv_.clear();
  subsamples_.clear();
  if (!SUCCEEDED(hr)) {
    RecordFailure("SubmitDecoderBuffers failed", hr);
    return false;
  }

  return true;
}

Status D3D11H264Accelerator::SubmitDecode(scoped_refptr<H264Picture> pic) {
  if (!SubmitSliceData()) {
    RecordFailure("SubmitSliceData failed");
    return Status::kFail;
  }

  HRESULT hr = video_context_->DecoderEndFrame(video_decoder_.Get());
  if (!SUCCEEDED(hr)) {
    RecordFailure("DecoderEndFrame failed", hr);
    return Status::kFail;
  }

  return Status::kOk;
}

void D3D11H264Accelerator::Reset() {
  if (!bitstream_buffer_bytes_)
    return;

  HRESULT hr = video_context_->ReleaseDecoderBuffer(
      video_decoder_.Get(), D3D11_VIDEO_DECODER_BUFFER_BITSTREAM);

  bitstream_buffer_bytes_ = nullptr;
  bitstream_buffer_size_ = 0;
  current_offset_ = 0;
  CHECK(SUCCEEDED(hr));
}

bool D3D11H264Accelerator::OutputPicture(scoped_refptr<H264Picture> pic) {
  D3D11H264Picture* our_pic = static_cast<D3D11H264Picture*>(pic.get());

  client_->OutputResult(our_pic, our_pic->picture);
  return true;
}

void D3D11H264Accelerator::RecordFailure(const std::string& reason,
                                         HRESULT hr) const {
  std::string hr_string;
  if (!SUCCEEDED(hr))
    hr_string = ": " + logging::SystemErrorCodeToString(hr);

  DLOG(ERROR) << reason << hr_string;
  media_log_->AddEvent(media_log_->CreateStringEvent(
      MediaLogEvent::MEDIA_ERROR_LOG_ENTRY, "error", hr_string + reason));
}

}  // namespace media
