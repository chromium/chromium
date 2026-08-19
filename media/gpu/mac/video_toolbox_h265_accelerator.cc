// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/mac/video_toolbox_h265_accelerator.h"

#include <array>
#include <utility>

#include "base/compiler_specific.h"
#include "base/numerics/byte_conversions.h"
#include "media/base/media_log.h"

namespace media {

VideoToolboxH265Accelerator::VideoToolboxH265Accelerator(
    std::unique_ptr<MediaLog> media_log,
    DecodeCB decode_cb,
    OutputCB output_cb)
    : media_log_(std::move(media_log)),
      decode_cb_(std::move(decode_cb)),
      output_cb_(std::move(output_cb)),
      vps_tracker_("VPS", media_log_.get()),
      sps_tracker_("SPS", media_log_.get()),
      pps_tracker_("PPS", media_log_.get()) {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

VideoToolboxH265Accelerator::~VideoToolboxH265Accelerator() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

scoped_refptr<H265Picture> VideoToolboxH265Accelerator::CreateH265Picture() {
  DVLOG(4) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::MakeRefCounted<H265Picture>();
}

void VideoToolboxH265Accelerator::ProcessVPS(
    const H265VPS* vps,
    base::span<const uint8_t> vps_nalu_data) {
  DVLOG(3) << __func__ << ":"
           << " vps_video_parameter_set_id=" << vps->vps_video_parameter_set_id;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  vps_tracker_.Process(vps->vps_video_parameter_set_id, vps_nalu_data);
  if (vps->aux_alpha_layer_id) {
    alpha_vps_ids_.insert(vps->vps_video_parameter_set_id);
  } else {
    alpha_vps_ids_.erase(vps->vps_video_parameter_set_id);
  }
}

void VideoToolboxH265Accelerator::ProcessSPS(
    const H265SPS* sps,
    base::span<const uint8_t> sps_nalu_data) {
  DVLOG(3) << __func__ << ":"
           << " sps_seq_parameter_set_id=" << sps->sps_seq_parameter_set_id
           << " sps_video_parameter_set_id=" << sps->sps_video_parameter_set_id;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sps_tracker_.Process(sps->sps_seq_parameter_set_id, sps_nalu_data);
}

void VideoToolboxH265Accelerator::ProcessPPS(
    const H265PPS* pps,
    base::span<const uint8_t> pps_nalu_data) {
  DVLOG(3) << __func__ << ":"
           << " pps_pic_parameter_set_id=" << pps->pps_pic_parameter_set_id
           << " pps_seq_parameter_set_id=" << pps->pps_seq_parameter_set_id;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pps_tracker_.Process(pps->pps_pic_parameter_set_id, pps_nalu_data);
}

bool VideoToolboxH265Accelerator::CreateFormat(scoped_refptr<H265Picture> pic) {
  // Gather parameter sets and update active parameter set data.
  std::vector<const uint8_t*> parameter_set_data;
  std::vector<size_t> parameter_set_size;
  if (!vps_tracker_.ExtractForFormat(parameter_set_data, parameter_set_size) ||
      !sps_tracker_.ExtractForFormat(parameter_set_data, parameter_set_size) ||
      !pps_tracker_.ExtractForFormat(parameter_set_data, parameter_set_size)) {
    return false;
  }

  // Create the format description.
  active_format_.reset();

  OSStatus status = CMVideoFormatDescriptionCreateFromHEVCParameterSets(
      /*allocator=*/kCFAllocatorDefault,
      /*parameterSetCount=*/parameter_set_data.size(),
      /*parameterSetPointers=*/parameter_set_data.data(),
      /*parameterSetSizes=*/parameter_set_size.data(),
      /*NALUnitHeaderLength=*/kNALUHeaderLength,
      /*extensions=*/nullptr,
      /*formatDescriptionOut=*/active_format_.InitializeInto());
  if (status != noErr) {
    OSSTATUS_MEDIA_LOG(ERROR, status, media_log_.get())
        << "CMVideoFormatDescriptionCreateFromHEVCParameterSets()";
    return false;
  }

  // Record session metadata.
  active_session_metadata_ = VideoToolboxDecompressionSessionMetadata{
      /*allow_software_decoding=*/true,
      /*bit_depth=*/frame_bit_depth_,
      /*chroma_sampling=*/frame_chroma_sampling_,
      /*has_alpha=*/frame_has_alpha_,
      /*visible_rect=*/pic->visible_rect()};

  return true;
}

VideoToolboxH265Accelerator::Status
VideoToolboxH265Accelerator::SubmitFrameMetadata(
    const H265SPS* sps,
    const H265PPS* pps,
    const H265SliceHeader* slice_hdr,
    const H265Picture::Vector& ref_pic_list,
    const H265Picture::Vector& ref_pic_set_lt_curr,
    const H265Picture::Vector& ref_pic_set_st_curr_after,
    const H265Picture::Vector& ref_pic_set_st_curr_before,
    scoped_refptr<H265Picture> pic) {
  DVLOG(3) << __func__ << ":"
           << " sps_video_parameter_set_id=" << sps->sps_video_parameter_set_id
           << " sps_seq_parameter_set_id=" << sps->sps_seq_parameter_set_id
           << " pps_pic_parameter_set_id=" << pps->pps_pic_parameter_set_id;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ResetFrameData();

  if (pic->no_rasl_output_flag_ &&
      (slice_hdr->nal_unit_type == H265NALU::RASL_N ||
       slice_hdr->nal_unit_type == H265NALU::RASL_R)) {
    // Drop this RASL frame, otherwise VideoToolbox will fail to decode it.
    drop_frame_ = true;
    return Status::kOk;
  }

  // Update frame state.
  frame_is_keyframe_ = slice_hdr->irap_pic;
  frame_bit_depth_ = sps->bit_depth_y;
  frame_chroma_sampling_ = sps->GetChromaSampling();
  frame_has_alpha_ = alpha_vps_ids_.contains(sps->sps_video_parameter_set_id);

  return Status::kOk;
}

VideoToolboxH265Accelerator::Status VideoToolboxH265Accelerator::SubmitSlice(
    const H265SPS* sps,
    const H265PPS* pps,
    const H265SliceHeader* slice_hdr,
    const H265Picture::Vector& ref_pic_list0,
    const H265Picture::Vector& ref_pic_list1,
    const H265Picture::Vector& ref_pic_set_lt_curr,
    const H265Picture::Vector& ref_pic_set_st_curr_after,
    const H265Picture::Vector& ref_pic_set_st_curr_before,
    scoped_refptr<H265Picture> pic,
    const uint8_t* data,
    size_t size,
    const std::vector<SubsampleEntry>& subsamples) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (drop_frame_) {
    return Status::kOk;
  }

  vps_tracker_.ReferenceInFrame(sps->sps_video_parameter_set_id);
  sps_tracker_.ReferenceInFrame(pps->pps_seq_parameter_set_id);
  pps_tracker_.ReferenceInFrame(pps->pps_pic_parameter_set_id);
  frame_slice_data_.push_back(UNSAFE_TODO(base::span(data, size)));

  return Status::kOk;
}

VideoToolboxH265Accelerator::Status VideoToolboxH265Accelerator::SubmitDecode(
    scoped_refptr<H265Picture> pic) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (drop_frame_) {
    return Status::kOk;
  }

  // Extract changed parameter sets and update active parameter set data.
  std::vector<base::span<const uint8_t>> combined_nalu_data;
  if (!vps_tracker_.ExtractForInbandUpdate(combined_nalu_data) ||
      !sps_tracker_.ExtractForInbandUpdate(combined_nalu_data) ||
      !pps_tracker_.ExtractForInbandUpdate(combined_nalu_data)) {
    return Status::kFail;
  }

  // Create a new format description if necessary.
  // We assume that session metadata can only change at a keyframe.
  // TODO(crbug.com/40227557): It's not clear when it is better to inline the
  // parameter sets vs. creating a new format.
  if (!active_format_ || (combined_nalu_data.size() && frame_is_keyframe_)) {
    combined_nalu_data.clear();
    if (!CreateFormat(pic)) {
      return Status::kFail;
    }
  }

  // Append slice data.
  combined_nalu_data.insert(combined_nalu_data.end(), frame_slice_data_.begin(),
                            frame_slice_data_.end());

  base::apple::ScopedCFTypeRef<CMSampleBufferRef> sample =
      VideoToolboxCreateSampleBufferFromNALUs(
          combined_nalu_data, active_format_.get(), media_log_.get());
  if (!sample) {
    return Status::kFail;
  }

  decode_cb_.Run(std::move(sample), active_session_metadata_, std::move(pic));
  return Status::kOk;
}

bool VideoToolboxH265Accelerator::OutputPicture(
    scoped_refptr<H265Picture> pic) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // We don't care about outputs, just pass them along.
  output_cb_.Run(std::move(pic));
  return true;
}

void VideoToolboxH265Accelerator::Reset() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The decompression session will probably also be reset, so we can't expect
  // it to know about any parameter sets. https://crbug.com/1493624
  vps_tracker_.ResetActive();
  sps_tracker_.ResetActive();
  pps_tracker_.ResetActive();
  active_format_.reset();

  ResetFrameData();
}

void VideoToolboxH265Accelerator::ResetFrameData() {
  DVLOG(4) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  vps_tracker_.ResetFrame();
  sps_tracker_.ResetFrame();
  pps_tracker_.ResetFrame();
  frame_slice_data_.clear();
  frame_bit_depth_ = 8;
  frame_chroma_sampling_ = VideoChromaSampling::k420;
  frame_is_keyframe_ = false;
  frame_has_alpha_ = false;
  drop_frame_ = false;
}

bool VideoToolboxH265Accelerator::IsChromaSamplingSupported(
    VideoChromaSampling format) {
  return true;
}

bool VideoToolboxH265Accelerator::IsAlphaLayerSupported() {
  return true;
}

}  // namespace media
